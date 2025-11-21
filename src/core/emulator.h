#ifndef EMULATOR_H
#define EMULATOR_H

#include <memory>
#include <string>
#include <cstdint>
#include "memory/mailbox.h"

// Forward declarations
class Cpu65816;
class RAM;
class Cartridge;
class MasterClock;
class CPLD1_Audio;
class CPLD2_Video;
class CPLD3_Raster;
class VideoRenderer;
class AudioMixer;
class AudioOutput;
class Mailbox;
class SystemBus;

/**
 * SANo Emulator
 * 
 * Main emulator controller that coordinates all subsystems:
 * - 3 CPUs (Main, Graphics, Sound)
 * - Memory systems (RAM, mailboxes, cartridge)
 * - Video rendering
 * - Audio output
 * - Timing synchronization
 * 
 * This is the top-level class that ties everything together.
 */
class Emulator {
public:
    Emulator();
    ~Emulator();
    
    // Initialization
    bool initialize();
    void shutdown();
    
    // ROM loading
    bool loadROM(const std::string& filename);
    bool loadROMFromMemory(const uint8_t* data, size_t size);
    void unloadROM();
    
    // Emulation control
    void reset();
    void run();          // Run until stopped
    void runFrame();     // Run one frame (60 Hz)
    void step();         // Run one instruction on Main CPU
    void stop();
    
    // State
    bool isRunning() const { return running; }
    bool isPaused() const { return paused; }
    bool isROMLoaded() const;
    
    // Pause/resume
    void pause();
    void resume();
    
    // Video
    const uint32_t* getFramebuffer() const;
    int getFramebufferWidth() const;
    int getFramebufferHeight() const;
    
    // Audio
    void setAudioEnabled(bool enabled);
    void setMasterVolume(float volume);  // 0.0-1.0
    
    // Performance
    double getEmulationSpeed() const;
    uint64_t getFrameCount() const;
    
    // Debug access
    Cpu65816* getMainCPU() const { return mainCPU.get(); }
    Cpu65816* getGraphicsCPU() const { return graphicsCPU.get(); }
    Cpu65816* getSoundCPU() const { return soundCPU.get(); }
    MasterClock* getClock() const { return clock.get(); }
    VideoRenderer* getVideoRenderer() const { return videoRenderer.get(); }
    
private:
    // Core components
    std::unique_ptr<MasterClock> clock;
    std::unique_ptr<Cartridge> cartridge;
    
    // CPUs
    std::unique_ptr<Cpu65816> mainCPU;
    std::unique_ptr<Cpu65816> graphicsCPU;
    std::unique_ptr<Cpu65816> soundCPU;
    
    // Memory
    std::unique_ptr<RAM> mainRAM;
    std::unique_ptr<RAM> graphicsRAM;
    std::unique_ptr<RAM> soundRAM;
    std::unique_ptr<Mailbox> mailboxA;  // Main <-> Graphics
    std::unique_ptr<Mailbox> mailboxB;  // Main <-> Sound
    
    // System buses
    std::unique_ptr<SystemBus> mainBus;
    std::unique_ptr<SystemBus> graphicsBus;
    std::unique_ptr<SystemBus> soundBus;
    
    // CPLDs
    std::unique_ptr<CPLD1_Audio> cpld1;
    std::unique_ptr<CPLD2_Video> cpld2;
    std::unique_ptr<CPLD3_Raster> cpld3;
    
    // Video
    std::unique_ptr<VideoRenderer> videoRenderer;
    
    // Audio
    std::unique_ptr<AudioMixer> audioMixer;
    std::unique_ptr<AudioOutput> audioOutput;
    
    // State
    bool running;
    bool paused;
    bool initialized;
    
    // Initialization helpers
    bool initializeCPUs();
    bool initializeMemory();
    bool initializeVideo();
    bool initializeAudio();
    void setupMemoryMaps();
    void setupCallbacks();
    
    // Emulation loop helpers
    void runMainCPU(uint32_t cycles);
    void runGraphicsCPU(uint32_t cycles);
    void runSoundCPU(uint32_t cycles);
    
    // Event handlers
    void onVBlank();
    void onScanline(int scanline);
    void onAudioSample();
};

#endif // EMULATOR_H
