#ifndef MASTER_CLOCK_H
#define MASTER_CLOCK_H

#include <cstdint>
#include <functional>

/**
 * Master Clock
 * 
 * Coordinates timing for all SANo subsystems:
 * - 3 CPUs running at different frequencies
 * - Video at 60 Hz (240 scanlines)
 * - Audio at 32 kHz sample rate
 * 
 * Uses cycle-accurate emulation to maintain proper timing relationships.
 * All components are synchronized to prevent drift.
 */
class MasterClock {
public:
    MasterClock();
    ~MasterClock();
    
    // Clock frequencies (Hz)
    static constexpr uint32_t MAIN_CPU_FREQ = 7159000;      // 7.159 MHz
    static constexpr uint32_t GRAPHICS_CPU_FREQ = 13500000; // 13.5 MHz
    static constexpr uint32_t SOUND_CPU_FREQ = 4773000;     // 4.773 MHz
    
    // Video timing
    static constexpr uint32_t PIXEL_CLOCK = 13500000;       // 13.5 MHz
    static constexpr int FRAME_RATE = 60;                   // 60 Hz
    static constexpr int SCANLINES_PER_FRAME = 240;
    static constexpr int TOTAL_SCANLINES = 262;             // Including vblank
    static constexpr int PIXELS_PER_SCANLINE = 858;         // Including hblank
    
    // Audio timing
    static constexpr uint32_t AUDIO_SAMPLE_RATE = 32000;    // 32 kHz
    
    // Derived timing
    static constexpr uint32_t CYCLES_PER_FRAME_MAIN = MAIN_CPU_FREQ / FRAME_RATE;
    static constexpr uint32_t CYCLES_PER_FRAME_GRAPHICS = GRAPHICS_CPU_FREQ / FRAME_RATE;
    static constexpr uint32_t CYCLES_PER_FRAME_SOUND = SOUND_CPU_FREQ / FRAME_RATE;
    
    static constexpr uint32_t CYCLES_PER_SCANLINE_GRAPHICS = GRAPHICS_CPU_FREQ / (FRAME_RATE * TOTAL_SCANLINES);
    static constexpr uint32_t AUDIO_SAMPLES_PER_FRAME = AUDIO_SAMPLE_RATE / FRAME_RATE;
    
    // Cycle tracking
    void addMainCPUCycles(uint32_t cycles);
    void addGraphicsCPUCycles(uint32_t cycles);
    void addSoundCPUCycles(uint32_t cycles);
    
    // Get current cycle counts
    uint64_t getMainCPUCycles() const { return mainCPUCycles; }
    uint64_t getGraphicsCPUCycles() const { return graphicsCPUCycles; }
    uint64_t getSoundCPUCycles() const { return soundCPUCycles; }
    uint64_t getMasterCycles() const { return masterCycles; }
    
    // Video timing
    int getCurrentScanline() const { return currentScanline; }
    int getCurrentPixel() const { return currentPixel; }
    bool isVBlank() const { return currentScanline >= SCANLINES_PER_FRAME; }
    bool isHBlank() const { return currentPixel >= 720; }
    
    // Frame synchronization
    void runFrame();                    // Run one complete frame
    bool shouldRunMainCPU() const;      // Check if Main CPU needs cycles
    bool shouldRunGraphicsCPU() const;  // Check if Graphics CPU needs cycles
    bool shouldRunSoundCPU() const;     // Check if Sound CPU needs cycles
    
    // Event callbacks
    using ScanlineCallback = std::function<void(int scanline)>;
    using VBlankCallback = std::function<void()>;
    using AudioCallback = std::function<void()>;
    
    void setScanlineCallback(ScanlineCallback callback) { onScanline = callback; }
    void setVBlankCallback(VBlankCallback callback) { onVBlank = callback; }
    void setAudioCallback(AudioCallback callback) { onAudioSample = callback; }
    
    // Reset
    void reset();
    
    // Frame counter
    uint64_t getFrameCount() const { return frameCount; }
    
    // Timing info
    double getEmulationSpeed() const;   // 1.0 = real-time, >1.0 = faster
    
private:
    // Cycle counters (per CPU)
    uint64_t mainCPUCycles;
    uint64_t graphicsCPUCycles;
    uint64_t soundCPUCycles;
    
    // Master cycle counter (based on Graphics CPU, highest frequency)
    uint64_t masterCycles;
    
    // Frame tracking
    uint64_t frameCount;
    int currentScanline;
    int currentPixel;
    
    // Target cycles for current frame
    uint64_t targetMainCycles;
    uint64_t targetGraphicsCycles;
    uint64_t targetSoundCycles;
    
    // Audio sample tracking
    uint32_t audioSampleCounter;
    uint32_t audioSamplesThisFrame;
    
    // Event callbacks
    ScanlineCallback onScanline;
    VBlankCallback onVBlank;
    AudioCallback onAudioSample;
    
    // Timing helpers
    void updateVideoTiming();
    void updateAudioTiming();
    void advanceScanline();
    void checkCallbacks();
    
    // Performance tracking
    uint64_t realTimeStart;
    uint64_t emulatedTimeStart;
};

#endif // MASTER_CLOCK_H
