#ifndef AUDIO_MIXER_H
#define AUDIO_MIXER_H

#include <cstdint>
#include <array>
#include <vector>
#include <memory>

// Forward declaration
class CPLD1_Audio;

/**
 * Audio Mixer
 * 
 * Emulates the ADAU1452 DSP audio mixing:
 * - Takes 8 mono channels @ 32 kHz
 * - Mixes to stereo output
 * - Applies per-channel volume and panning
 * - Automatic gain normalization
 * 
 * The mixer reads samples from CPLD1_Audio FIFOs and produces
 * stereo PCM output for the Qt audio system.
 */
class AudioMixer {
public:
    AudioMixer();
    ~AudioMixer();
    
    // Configuration
    void setCPLD1(CPLD1_Audio* cpld1) { this->cpld1 = cpld1; }
    
    // Audio generation
    void generateSamples(int16_t* buffer, int numFrames);
    
    // Per-channel controls
    void setChannelVolume(int channel, float volume);    // 0.0-1.0
    void setChannelPan(int channel, float pan);          // -1.0 (left) to +1.0 (right)
    void setChannelMute(int channel, bool muted);
    
    // Master controls
    void setMasterVolume(float volume);                  // 0.0-1.0
    void setAutoGainControl(bool enabled);               // Prevent clipping
    
    // Reset
    void reset();
    
    // Constants
    static constexpr int SAMPLE_RATE = 32000;  // 32 kHz
    static constexpr int NUM_CHANNELS = 8;
    
private:
    // CPLD reference
    CPLD1_Audio* cpld1;
    
    // Per-channel settings
    struct ChannelState {
        float volume;      // 0.0-1.0
        float pan;         // -1.0 to +1.0
        bool muted;
        
        ChannelState() : volume(1.0f), pan(0.0f), muted(false) {}
    };
    
    std::array<ChannelState, NUM_CHANNELS> channels;
    
    // Master settings
    float masterVolume;
    bool autoGainControl;
    
    // AGC state
    float currentGain;
    float targetGain;
    
    // Mixing helpers
    void mixFrame(int16_t& leftOut, int16_t& rightOut);
    void applyAGC(int16_t& left, int16_t& right);
    float calculatePeakLevel();
    
    // Clamping
    int16_t clamp(float sample);
};

#endif // AUDIO_MIXER_H
