#include "audio_mixer.h"
#include "cpld1_audio.h"
#include <algorithm>
#include <cmath>

AudioMixer::AudioMixer()
    : cpld1(nullptr)
    , masterVolume(1.0f)
    , autoGainControl(true)
    , currentGain(1.0f)
    , targetGain(1.0f)
{
    reset();
}

AudioMixer::~AudioMixer() {
}

void AudioMixer::reset() {
    // Reset all channels to defaults
    for (auto& ch : channels) {
        ch.volume = 1.0f;
        ch.pan = 0.0f;
        ch.muted = false;
    }
    
    masterVolume = 1.0f;
    autoGainControl = true;
    currentGain = 1.0f;
    targetGain = 1.0f;
}

//=============================================================================
// Sample Generation
//=============================================================================

void AudioMixer::generateSamples(int16_t* buffer, int numFrames) {
    if (!cpld1) {
        // No audio source - output silence
        std::fill_n(buffer, numFrames * 2, 0);
        return;
    }
    
    // Generate stereo frames
    for (int i = 0; i < numFrames; ++i) {
        int16_t left = 0, right = 0;
        
        // Mix all channels
        mixFrame(left, right);
        
        // Apply automatic gain control if enabled
        if (autoGainControl) {
            applyAGC(left, right);
        }
        
        // Write stereo pair
        buffer[i * 2 + 0] = left;
        buffer[i * 2 + 1] = right;
    }
}

void AudioMixer::mixFrame(int16_t& leftOut, int16_t& rightOut) {
    float leftSum = 0.0f;
    float rightSum = 0.0f;
    
    // Mix all 8 channels
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
        if (channels[ch].muted) continue;
        
        // Get sample from CPLD FIFO
        int16_t leftSample, rightSample;
        cpld1->getAudioFrame(leftSample, rightSample);
        
        // Apply channel volume
        float sample = static_cast<float>(leftSample) * channels[ch].volume;
        
        // Apply panning
        // pan: -1.0 = full left, 0.0 = center, +1.0 = full right
        float leftGain, rightGain;
        if (channels[ch].pan <= 0.0f) {
            // Panned left
            leftGain = 1.0f;
            rightGain = 1.0f + channels[ch].pan;  // 0.0 to 1.0
        } else {
            // Panned right
            leftGain = 1.0f - channels[ch].pan;   // 1.0 to 0.0
            rightGain = 1.0f;
        }
        
        leftSum += sample * leftGain;
        rightSum += sample * rightGain;
    }
    
    // Apply master volume
    leftSum *= masterVolume;
    rightSum *= masterVolume;
    
    // Clamp to 16-bit range
    leftOut = clamp(leftSum);
    rightOut = clamp(rightSum);
}

//=============================================================================
// Automatic Gain Control
//=============================================================================

void AudioMixer::applyAGC(int16_t& left, int16_t& right) {
    // Calculate peak level of current samples
    float leftF = static_cast<float>(left);
    float rightF = static_cast<float>(right);
    float peak = std::max(std::abs(leftF), std::abs(rightF));
    
    // If peak exceeds 16-bit range, reduce gain
    if (peak > 32767.0f) {
        targetGain = 32767.0f / peak;
    } else {
        // Slowly recover gain toward 1.0
        targetGain = 1.0f;
    }
    
    // Smooth gain changes (attack/release)
    const float alpha = 0.01f;  // Smoothing factor
    currentGain += (targetGain - currentGain) * alpha;
    
    // Apply gain
    left = clamp(leftF * currentGain);
    right = clamp(rightF * currentGain);
}

float AudioMixer::calculatePeakLevel() {
    // Helper function for AGC analysis (not currently used)
    return 1.0f;
}

//=============================================================================
// Channel Controls
//=============================================================================

void AudioMixer::setChannelVolume(int channel, float volume) {
    if (channel >= 0 && channel < NUM_CHANNELS) {
        channels[channel].volume = std::clamp(volume, 0.0f, 1.0f);
    }
}

void AudioMixer::setChannelPan(int channel, float pan) {
    if (channel >= 0 && channel < NUM_CHANNELS) {
        channels[channel].pan = std::clamp(pan, -1.0f, 1.0f);
    }
}

void AudioMixer::setChannelMute(int channel, bool muted) {
    if (channel >= 0 && channel < NUM_CHANNELS) {
        channels[channel].muted = muted;
    }
}

//=============================================================================
// Master Controls
//=============================================================================

void AudioMixer::setMasterVolume(float volume) {
    masterVolume = std::clamp(volume, 0.0f, 1.0f);
}

void AudioMixer::setAutoGainControl(bool enabled) {
    autoGainControl = enabled;
    if (enabled) {
        currentGain = 1.0f;
        targetGain = 1.0f;
    }
}

//=============================================================================
// Helpers
//=============================================================================

int16_t AudioMixer::clamp(float sample) {
    if (sample > 32767.0f) return 32767;
    if (sample < -32768.0f) return -32768;
    return static_cast<int16_t>(sample);
}
