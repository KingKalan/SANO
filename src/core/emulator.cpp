#include "emulator.h"
#include "cpu/Cpu65816.hpp"
#include "cpu/Cpu65816Debugger.hpp"
#include "memory/ram.h"
#include "cartridge/cartridge.h"
#include "timing/master_clock.h"
#include "cpld/cpld1_audio.h"
#include "cpld/cpld2_video.h"
#include "cpld/cpld3_raster.h"
#include "video/video_renderer.h"
#include "audio/audio_mixer.h"
#include "audio/audio_output.h"
#include "mailbox.h"
#include "SystemBusDevice.hpp"
#include "SystemBus.hpp"
#include <iostream>
#include <iomanip>

Emulator::Emulator()
    : running(false)
    , paused(false)
    , initialized(false)
{
}

Emulator::~Emulator() {
    shutdown();
}

//=============================================================================
// Initialization
//=============================================================================

bool Emulator::initialize() {
    if (initialized) {
        return true;
    }
    
    std::cout << "Initializing SANo Emulator..." << std::endl;
    
    // Initialize master clock
    clock = std::make_unique<MasterClock>();
    
    // Initialize memory first
    if (!initializeMemory()) {
        std::cerr << "Failed to initialize memory" << std::endl;
        return false;
    }
    
    // Initialize CPUs (needs memory/buses)
    if (!initializeCPUs()) {
        std::cerr << "Failed to initialize CPUs" << std::endl;
        return false;
    }
    
    // Initialize video
    if (!initializeVideo()) {
        std::cerr << "Failed to initialize video" << std::endl;
        return false;
    }
    
    // Initialize audio
    if (!initializeAudio()) {
        std::cerr << "Failed to initialize audio" << std::endl;
        return false;
    }
    
    // Setup memory maps
    setupMemoryMaps();
    
    // Setup callbacks
    setupCallbacks();
    
    initialized = true;
    std::cout << "Emulator initialized successfully" << std::endl;
    return true;
}

void Emulator::shutdown() {
    if (running) {
        stop();
    }
    
    // Cleanup in reverse order
    audioOutput.reset();
    audioMixer.reset();
    videoRenderer.reset();
    
    cpld3.reset();
    cpld2.reset();
    cpld1.reset();
    
    soundBus.reset();
    graphicsBus.reset();
    mainBus.reset();
    
    mailboxB.reset();
    mailboxA.reset();
    soundRAM.reset();
    graphicsRAM.reset();
    mainRAM.reset();
    
    soundCPU.reset();
    graphicsCPU.reset();
    mainCPU.reset();
    
    cartridge.reset();
    clock.reset();
    
    initialized = false;
}

//=============================================================================
// ROM Loading
//=============================================================================

bool Emulator::loadROM(const std::string& filename) {
    if (!initialized) {
        std::cerr << "Emulator not initialized" << std::endl;
        return false;
    }

    // Create cartridge and load ROM
    cartridge = std::make_unique<Cartridge>();
    if (!cartridge->loadROM(filename)) {
        std::cerr << "Failed to load ROM: " << filename << std::endl;
        cartridge.reset();
        return false;
    }

    // Register cartridge with main CPU bus
    mainBus->registerDevice(cartridge.get());
    graphicsBus->registerDevice(cartridge.get());
    soundBus->registerDevice(cartridge.get());

    std::cout << "ROM loaded: " << filename << std::endl;
    return true;
}

bool Emulator::loadROMFromMemory(const uint8_t* data, size_t size) {
    if (!initialized) {
        std::cerr << "Emulator not initialized" << std::endl;
        return false;
    }
    
    cartridge = std::make_unique<Cartridge>();
    if (!cartridge->loadROM(data, size)) {
        std::cerr << "Failed to load ROM from memory" << std::endl;
        cartridge.reset();
        return false;
    }
    
    std::cout << "ROM loaded from memory" << std::endl;
    return true;
}

void Emulator::unloadROM() {
    if (running) {
        stop();
    }
    cartridge.reset();
}

bool Emulator::isROMLoaded() const {
    return cartridge != nullptr && cartridge->isLoaded();
}

//=============================================================================
// Emulation Control
//=============================================================================

void Emulator::reset() {
    if (!initialized) {
        return;
    }
    
    // Note: Cpu65816::reset() is private, so we can't call it directly
    // The CPU will reset itself when RES pin is toggled
    if (mainCPU) {
        mainCPU->setRESPin(true);
        mainCPU->setRESPin(false);

        // Set Main CPU PC from ROM header bitches!!!!! lmmfao im tired of fucking typing
        if (cartridge && cartridge->isLoaded()) {
            uint32_t entryPoint = cartridge->getHeader().mainCPU_entryPoint;
            Address startAddr(entryPoint >> 16, entryPoint & 0xFFFF);
            mainCPU->setProgramAddress(startAddr);
            std::cout << "Main CPU PC set to: $" <<std::hex << entryPoint << std::dec << std::endl;
        }
    }
    if (graphicsCPU) {
        graphicsCPU->setRESPin(false);

        // Set Graphics CPU PC from ROM header
        if (cartridge && cartridge->isLoaded()) {
            uint32_t entryPoint = cartridge->getHeader().graphicsCPU_entryPoint;

            if (entryPoint != 0) {
                graphicsCPU->setRESPin(false);
                Address startAddr(entryPoint >> 16, entryPoint & 0xFFFF);
                graphicsCPU->setProgramAddress(startAddr);
                std::cout << "Graphics CPU PC set to: $" << std::hex << entryPoint << std::dec << std::endl;
            } else {
                std::cout << "Graphics CPU held in reset (mailbox boot)" << std::endl;
            }
        }
    }
    if (soundCPU) {
        soundCPU->setRESPin(true);
        soundCPU->setRESPin(false);

        // Set Sound CPU PC from ROM header
        if (cartridge && cartridge->isLoaded()) {
            uint32_t entryPoint = cartridge->getHeader().soundCPU_entryPoint;
            Address startAddr(entryPoint >> 16, entryPoint & 0xFFFF);
            soundCPU->setProgramAddress(startAddr);
            std::cout << "Sound CPU PC set to: $" <<std::hex << entryPoint << std::dec << std::endl;
        }
    }
    
    // Reset clock
    if (clock) {
        clock->reset();
    }
    std::cout << "Emulator reset" << std::endl;
}

void Emulator::run() {
    if (!initialized || !isROMLoaded()) {
        std::cerr << "Cannot run: emulator not initialized or no ROM loaded" << std::endl;
        return;
    }
    
    running = true;
    paused = false;
    std::cout << "Emulator running" << std::endl;

}

void Emulator::runFrame() {
    if (!running || paused || !mainCPU) {
        return;
    }

    // Increment frame counter
    if (clock) {
        clock->runFrame();
    }

    //DEBUG BLOCK
    static int frameCounter = 0;
    if (frameCounter++ % 60 == 0) {
        std::cout << "runFrame() called, frame " << frameCounter << std::endl;
    }
    
    // Simple frame execution - run CPUs for one frame worth of cycles
    // At 60 Hz, each frame is ~16.67ms
    // Main CPU runs at ~3.58 MHz = ~59,667 cycles per frame
    
    const uint32_t CYCLES_PER_FRAME = 59667;
    
    runMainCPU(CYCLES_PER_FRAME);
    runGraphicsCPU(CYCLES_PER_FRAME);
    runSoundCPU(CYCLES_PER_FRAME);
    
    // Render video frame
    if (videoRenderer) {
        videoRenderer->renderFrame();
    }
}

void Emulator::step() {
    if (!running || !mainCPU) {
        return;
    }
    
    // Execute one instruction on Main CPU
    mainCPU->executeNextInstruction();
}

void Emulator::stop() {
    running = false;
    std::cout << "Emulator stopped" << std::endl;
}

void Emulator::pause() {
    paused = true;
}

void Emulator::resume() {
    paused = false;
}

//=============================================================================
// Video Access
//=============================================================================

const uint32_t* Emulator::getFramebuffer() const {
    if (videoRenderer) {
        return videoRenderer->getFramebuffer();
    }
    return nullptr;
}

int Emulator::getFramebufferWidth() const {
    return 320;
}

int Emulator::getFramebufferHeight() const {
    return 240;
}

//=============================================================================
// Audio Control
//=============================================================================

void Emulator::setAudioEnabled(bool enabled) {
    // TODO: Implement
}

void Emulator::setMasterVolume(float volume) {
    if (audioMixer) {
        // audioMixer->setMasterVolume(volume);
    }
}

//=============================================================================
// Performance
//=============================================================================

double Emulator::getEmulationSpeed() const {
    // TODO: Calculate actual speed
    return 1.0;
}

uint64_t Emulator::getFrameCount() const {
    if (clock) {
        return clock->getFrameCount();
    }
    return 0;
}

//=============================================================================
// Initialization Helpers
//=============================================================================

bool Emulator::initializeCPUs() {
    // Need to create interrupt vectors for each CPU
    // Interrupt structures have const members, so must be initialized with values
    
    // Default interrupt vectors (will be read from ROM header when ROM loads)
    // For now, point everything to ROM start address
    const uint16_t DEFAULT_VECTOR = 0x0000;  // Will be set from ROM
    
    // Create interrupt structures for Main CPU
    auto mainEmulationInt = new EmulationModeInterrupts{
        DEFAULT_VECTOR,  // coProcessorEnable
        DEFAULT_VECTOR,  // unused
        DEFAULT_VECTOR,  // abort
        DEFAULT_VECTOR,  // nonMaskableInterrupt
        DEFAULT_VECTOR,  // reset
        DEFAULT_VECTOR   // brkIrq
    };
    
    auto mainNativeInt = new NativeModeInterrupts{
        DEFAULT_VECTOR,  // coProcessorEnable
        DEFAULT_VECTOR,  // brk
        DEFAULT_VECTOR,  // abort
        DEFAULT_VECTOR,  // nonMaskableInterrupt
        DEFAULT_VECTOR,  // reset
        DEFAULT_VECTOR   // interruptRequest
    };
    
    // Graphics CPU interrupts
    auto graphicsEmulationInt = new EmulationModeInterrupts{
        DEFAULT_VECTOR, DEFAULT_VECTOR, DEFAULT_VECTOR,
        DEFAULT_VECTOR, DEFAULT_VECTOR, DEFAULT_VECTOR
    };
    
    auto graphicsNativeInt = new NativeModeInterrupts{
        DEFAULT_VECTOR, DEFAULT_VECTOR, DEFAULT_VECTOR,
        DEFAULT_VECTOR, DEFAULT_VECTOR, DEFAULT_VECTOR
    };
    
    // Sound CPU interrupts
    auto soundEmulationInt = new EmulationModeInterrupts{
        DEFAULT_VECTOR, DEFAULT_VECTOR, DEFAULT_VECTOR,
        DEFAULT_VECTOR, DEFAULT_VECTOR, DEFAULT_VECTOR
    };
    
    auto soundNativeInt = new NativeModeInterrupts{
        DEFAULT_VECTOR, DEFAULT_VECTOR, DEFAULT_VECTOR,
        DEFAULT_VECTOR, DEFAULT_VECTOR, DEFAULT_VECTOR
    };
    
    // Create CPUs with their buses and interrupt vectors
    mainCPU = std::make_unique<Cpu65816>(*mainBus, mainEmulationInt, mainNativeInt);
    graphicsCPU = std::make_unique<Cpu65816>(*graphicsBus, graphicsEmulationInt, graphicsNativeInt);
    soundCPU = std::make_unique<Cpu65816>(*soundBus, soundEmulationInt, soundNativeInt);
    
    // Set initial pin states
    mainCPU->setRDYPin(true);
    graphicsCPU->setRDYPin(true);
    soundCPU->setRDYPin(true);

    //Hold CPUs in reset unitl ROM is loaded
    mainCPU->setRESPin(true);
    //graphicsCPU->setRESPin(true);
    //soundCPU->setRESPin(true);

    return true;
}

bool Emulator::initializeMemory() {
    // Create system buses first (CPUs need them)
    mainBus = std::make_unique<SystemBus>();
    graphicsBus = std::make_unique<SystemBus>();
    soundBus = std::make_unique<SystemBus>();
    
    // Create RAM modules with base addresses and sizes
    // Main RAM: 128KB at $000000
    mainRAM = std::make_unique<RAM>(0x000000, 128 * 1024, "Main RAM");
    
    // Graphics RAM: 128KB at $000000 (in graphics CPU address space)
    graphicsRAM = std::make_unique<RAM>(0x000000, 128 * 1024, "Graphics RAM");
    
    // Sound RAM: 64KB at $000000 (in sound CPU address space)
    soundRAM = std::make_unique<RAM>(0x000000, 64 * 1024, "Sound RAM");
    
    // Create mailboxes
    // Mailbox A: Main <-> Graphics at $400000
    mailboxA = std::make_unique<Mailbox>(0x400000, 1024, "Mailbox A");
    
    // Mailbox B: Main <-> Sound at $410000
    mailboxB = std::make_unique<Mailbox>(0x410000, 1024, "Mailbox B");
    
    return true;
}

bool Emulator::initializeVideo() {
    // Create CPLDs
    cpld2 = std::make_unique<CPLD2_Video>();
    cpld3 = std::make_unique<CPLD3_Raster>();
    
    // Create video renderer
    videoRenderer = std::make_unique<VideoRenderer>();
    
    // Connect renderer to Graphics CPU RAM (VRAM)
    // VideoRenderer::setVRAM() expects a RAM* pointer
    if (graphicsRAM) {
        videoRenderer->setVRAM(graphicsRAM.get());
    }
    
    // Connect renderer to CPLDs
    videoRenderer->setCPLD2(cpld2.get());
    videoRenderer->setCPLD3(cpld3.get());
    
    return true;
}

bool Emulator::initializeAudio() {
    // Create CPLD1
    cpld1 = std::make_unique<CPLD1_Audio>();
    
    // Create audio components
    audioMixer = std::make_unique<AudioMixer>();
    audioOutput = std::make_unique<AudioOutput>();
    
    // TODO: Connect mixer to output
    // audioMixer->setOutput(audioOutput.get());
    
    return true;
}

void Emulator::setupMemoryMaps() {
    std::cout << "Emulator: Setting up memory maps..." << std::endl;

    // Register devices with Main CPU bus
    mainBus->registerDevice(mainRAM.get());
    mainBus->registerDevice(mailboxA.get());
    mainBus->registerDevice(mailboxB.get());

    // Graphics CPU - VRAM
    graphicsBus->registerDevice(graphicsRAM.get());
    graphicsBus->registerDevice(mailboxA.get());

    // Sound CPU - sound RAM
    soundBus->registerDevice(soundRAM.get());
    soundBus->registerDevice(mailboxB.get());

    std::cout << "Emulator: Memory maps configured" << std::endl;
}

void Emulator::setupCallbacks() {

    cpld2->setGraphicsRAM(graphicsRAM.get());
    cpld2->setGraphicsCPUReset([this](bool state) {
        if (!state) {
            // Release from reset - pulse it
            graphicsCPU->setRESPin(false);
            graphicsCPU->setProgramAddress(Address(0, 0));

            // Debug: What does Graphics CPU see at $0000?
            Address testAddr(0, 0);
            uint8_t fromBus = graphicsBus->readByte(testAddr);
            std::cout << "[GFX CPU] graphicsBus->readByte($0000) = $"
            << std::hex << (int)fromBus << std::dec << std::endl;

            Address pc = graphicsCPU->getProgramAddress();
            std::cout << "[GFX CPU] Released, PC=$" << std::hex
            << (int)pc.getBank() << ":" << pc.getOffset() << std::dec << std::endl;
        } else {
            graphicsCPU->setRESPin(true);
        }
    });

    cpld1->setSoundRAM(soundRAM.get());
    cpld1->setSoundCPUReset([this](bool state) {
        if (!state) {
            // Release from reset - pulse it
            soundCPU->setRESPin(false);
            soundCPU->setProgramAddress(Address(0, 0));

            Address pc = soundCPU->getProgramAddress();
            std::cout << "[SFX CPU] Released, PC=$" << std::hex
            << (int)pc.getBank() << ":" << pc.getOffset() << std::dec << std::endl;
        } else {
            soundCPU->setRESPin(true);
        }
    });

    // CPLD2 handles Mailbox A → Graphics CPU
    cpld2->setMailboxA(mailboxA.get());
    cpld2->setMailboxACallback([this]() {
        std::cout << "[CPLD2] Mailbox A written - triggering Graphics CPU IRQ" << std::endl;
        graphicsCPU->setIRQPin(true);
    });

    // CPLD1 handles Mailbox B → Sound CPU
    cpld1->setMailboxB(mailboxB.get());
    cpld1->setMailboxBCallback([this]() {
        std::cout << "[CPLD1] Mailbox B written - triggering Sound CPU IRQ" << std::endl;
        soundCPU->setIRQPin(true);
    });

    // Tell mailboxes to notify CPLD2 when written
    mailboxA->setWriteCallback([this]() {
        cpld2->onMailboxAWrite();
    });

    mailboxB->setWriteCallback([this]() {
        cpld1->onMailboxBWrite();
    });

    // CPLD2 triggers CPU IRQs when mailboxes are written
    cpld2->setMailboxACallback([this]() {
        std::cout << "[CPLD2] Mailbox A written - triggering Graphics CPU IRQ" << std::endl;
        graphicsCPU->setIRQPin(true);
    });

    cpld1->setMailboxBCallback([this]() {
        std::cout << "[CPLD2] Mailbox B written - triggering Sound CPU IRQ" << std::endl;
        soundCPU->setIRQPin(true);
    });
}

//=============================================================================
// Emulation Loop Helpers
//=============================================================================

void Emulator::runMainCPU(uint32_t cycles) {
    if (!mainCPU) return;

    //DEBUG BLOCK
    static bool first = true;
    if (first) {
        std::cout << "First runMainCPU() call" <<std::endl;
        first = false;
    }

    static int callCount = 0;
    if (callCount++ == 0) {
        Address pc = mainCPU->getProgramAddress();
        uint32_t flatPC = (pc.getBank() << 16) | pc.getOffset();
        uint8_t opcode = mainBus->readByte(pc);
        std::cout << " opcode=$" << std::hex << std::setw(2) << std::setfill('0') << (int)opcode << std::dec;
        if (opcode == 0x8F) {
            uint8_t addr_lo = mainBus->readByte(pc.newWithOffset(1));
            uint8_t addr_mid = mainBus->readByte(pc.newWithOffset(2));
            uint8_t addr_hi = mainBus->readByte(pc.newWithOffset(3));
            uint32_t targetAddr = (addr_hi << 16) | (addr_mid << 8) | addr_lo;
            std::cout << " *** STA to $" << std::hex << targetAddr << std::dec << " ***";
        }
        std::cout << "CPU PC at start: $" << std::hex << flatPC << std::dec << std::endl;
    }
    
    //Print first 10 instructions
    static int instrCount = 0;

    // Execute instructions until we've used up the cycles
    // Note: executeNextInstruction() returns true if instruction was executed
    for (uint32_t i = 0; i < cycles; ++i) {
        if (!running || paused) break;

        // TRACE: Print PC before executing
        if (instrCount < 1) {
            Address pc = mainCPU->getProgramAddress();
            uint32_t flatPC = (pc.getBank() << 16) | pc.getOffset();
            std::cout << "[TRACE] Executing at PC: $" << std::hex << flatPC << std::dec << std::endl;
            instrCount++;
        }

        mainCPU->executeNextInstruction();
    }
}

void Emulator::runGraphicsCPU(uint32_t cycles) {
    if (!graphicsCPU) return;

/*
    static bool firstExec = true;
    if (firstExec) {
        firstExec = false;

        Address pcBefore = graphicsCPU->getProgramAddress();
        std::cout << "[GFX TEST] Before: PC=$" << std::hex << pcBefore.getOffset() << std::dec << std::endl;

        graphicsCPU->executeNextInstruction();

        Address pcAfter = graphicsCPU->getProgramAddress();
        std::cout << "[GFX TEST] After: PC=$" << std::hex << pcAfter.getOffset() << std::dec << std::endl;

        return;
    }
*/
    for (uint32_t i = 0; i < cycles; ++i) {
        if (!running || paused) break;
        graphicsCPU->executeNextInstruction();
    }

    // After every 1000 frames, dump some VRAM
    static int frameCheck = 0;
    if (frameCheck++ == 60) {  // After 1 second
        std::cout << "[VRAM CHECK] VRAM[$20-$23]: ";
        for (int i = 0x20; i < 0x24; i++) {
            std::cout << std::hex << (int)graphicsRAM->readByte(Address(0, i)) << " ";
        }
        std::cout << std::dec << std::endl;
    }

    // Debug: Print PC occasionally
    static int debugCount = 0;
    if (debugCount++ % 10000 == 0) {
        Address pc = graphicsCPU->getProgramAddress();
        std::cout << "[GFX CPU] PC=$" << std::hex
        << (int)pc.getBank() << ":" << pc.getOffset()
        << " VRAM[0]=$" << (int)graphicsRAM->readByte(Address(0,0))
        << std::dec << std::endl;
    }
}

void Emulator::runSoundCPU(uint32_t cycles) {
    if (!soundCPU) return;

    for (uint32_t i = 0; i < cycles; ++i) {
        if (!running || paused) break;
        soundCPU->executeNextInstruction();
    }
}

//=============================================================================
// Event Handlers
//=============================================================================

void Emulator::onVBlank() {
    // TODO: Handle V-blank events
    // Trigger NMI on CPUs if enabled
}

void Emulator::onScanline(int scanline) {
    // TODO: Handle per-scanline events
    // Raster effects, etc.
}

void Emulator::onAudioSample() {
    // TODO: Handle audio sample generation
}
