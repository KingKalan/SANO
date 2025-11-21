#include "master_clock.h"
#include <chrono>
#include <algorithm>

MasterClock::MasterClock()
    : mainCPUCycles(0)
    , graphicsCPUCycles(0)
    , soundCPUCycles(0)
    , masterCycles(0)
    , frameCount(0)
    , currentScanline(0)
    , currentPixel(0)
    , targetMainCycles(0)
    , targetGraphicsCycles(0)
    , targetSoundCycles(0)
    , audioSampleCounter(0)
    , audioSamplesThisFrame(0)
    , onScanline(nullptr)
    , onVBlank(nullptr)
    , onAudioSample(nullptr)
    , realTimeStart(0)
    , emulatedTimeStart(0)
{
    reset();
}

MasterClock::~MasterClock() {
}

void MasterClock::reset() {
    mainCPUCycles = 0;
    graphicsCPUCycles = 0;
    soundCPUCycles = 0;
    masterCycles = 0;
    frameCount = 0;
    currentScanline = 0;
    currentPixel = 0;
    audioSampleCounter = 0;
    audioSamplesThisFrame = 0;
    
    // Set initial frame targets
    targetMainCycles = CYCLES_PER_FRAME_MAIN;
    targetGraphicsCycles = CYCLES_PER_FRAME_GRAPHICS;
    targetSoundCycles = CYCLES_PER_FRAME_SOUND;
    
    // Initialize performance tracking
    auto now = std::chrono::steady_clock::now();
    realTimeStart = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
    emulatedTimeStart = 0;
}

//=============================================================================
// Cycle Tracking
//=============================================================================

void MasterClock::addMainCPUCycles(uint32_t cycles) {
    mainCPUCycles += cycles;
    
    // Convert to master clock cycles (Graphics CPU is master)
    // Ratio: Graphics/Main = 13.5/7.159 ≈ 1.886
    masterCycles = (graphicsCPUCycles * GRAPHICS_CPU_FREQ) / GRAPHICS_CPU_FREQ;
    
    updateVideoTiming();
    updateAudioTiming();
}

void MasterClock::addGraphicsCPUCycles(uint32_t cycles) {
    graphicsCPUCycles += cycles;
    masterCycles = graphicsCPUCycles;  // Graphics CPU is the master clock
    
    updateVideoTiming();
    updateAudioTiming();
}

void MasterClock::addSoundCPUCycles(uint32_t cycles) {
    soundCPUCycles += cycles;
    
    updateAudioTiming();
}

//=============================================================================
// Video Timing
//=============================================================================

void MasterClock::updateVideoTiming() {
    // Graphics CPU runs at pixel clock (13.5 MHz)
    // Calculate current scanline and pixel from Graphics CPU cycles
    
    uint64_t cyclesThisFrame = graphicsCPUCycles % (GRAPHICS_CPU_FREQ / FRAME_RATE);
    uint64_t totalPixels = cyclesThisFrame;  // 1 cycle = 1 pixel at PIXCLK
    
    int oldScanline = currentScanline;
    
    currentScanline = totalPixels / PIXELS_PER_SCANLINE;
    currentPixel = totalPixels % PIXELS_PER_SCANLINE;
    
    // Scanline callback
    if (currentScanline != oldScanline && onScanline) {
        onScanline(currentScanline);
    }
    
    // VBlank callback
    if (oldScanline < SCANLINES_PER_FRAME && currentScanline >= SCANLINES_PER_FRAME && onVBlank) {
        onVBlank();
    }
}

void MasterClock::advanceScanline() {
    currentScanline++;
    currentPixel = 0;
    
    if (currentScanline >= TOTAL_SCANLINES) {
        currentScanline = 0;
        frameCount++;
    }
    
    if (onScanline) {
        onScanline(currentScanline);
    }
    
    if (currentScanline == SCANLINES_PER_FRAME && onVBlank) {
        onVBlank();
    }
}

//=============================================================================
// Audio Timing
//=============================================================================

void MasterClock::updateAudioTiming() {
    // Audio runs at 32 kHz
    // Calculate how many samples should have been generated
    uint64_t expectedSamples = (masterCycles * AUDIO_SAMPLE_RATE) / GRAPHICS_CPU_FREQ;
    
    while (audioSampleCounter < expectedSamples) {
        if (onAudioSample) {
            onAudioSample();
        }
        audioSampleCounter++;
        audioSamplesThisFrame++;
    }
}

//=============================================================================
// Frame Synchronization
//=============================================================================

void MasterClock::runFrame() {
    // Set targets for next frame
    targetMainCycles = mainCPUCycles + CYCLES_PER_FRAME_MAIN;
    targetGraphicsCycles = graphicsCPUCycles + CYCLES_PER_FRAME_GRAPHICS;
    targetSoundCycles = soundCPUCycles + CYCLES_PER_FRAME_SOUND;

    // Reset frame counters
    audioSamplesThisFrame = 0;

    frameCount++;  // <-- ADD THIS!
}

bool MasterClock::shouldRunMainCPU() const {
    // Run Main CPU if it's behind schedule
    return mainCPUCycles < targetMainCycles;
}

bool MasterClock::shouldRunGraphicsCPU() const {
    // Run Graphics CPU if it's behind schedule
    return graphicsCPUCycles < targetGraphicsCycles;
}

bool MasterClock::shouldRunSoundCPU() const {
    // Run Sound CPU if it's behind schedule
    return soundCPUCycles < targetSoundCycles;
}

//=============================================================================
// Performance Tracking
//=============================================================================

double MasterClock::getEmulationSpeed() const {
    auto now = std::chrono::steady_clock::now();
    uint64_t realTimeNow = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
    
    uint64_t realTimeElapsed = realTimeNow - realTimeStart;
    if (realTimeElapsed == 0) return 1.0;
    
    // Calculate emulated time in microseconds
    uint64_t emulatedTime = (graphicsCPUCycles * 1000000ULL) / GRAPHICS_CPU_FREQ;
    uint64_t emulatedElapsed = emulatedTime - emulatedTimeStart;
    
    return static_cast<double>(emulatedElapsed) / static_cast<double>(realTimeElapsed);
}

//=============================================================================
// Helper Functions
//=============================================================================

void MasterClock::checkCallbacks() {
    // Called periodically to check for events
    updateVideoTiming();
    updateAudioTiming();
}
