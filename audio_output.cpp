#include "audio_output.h"
#include "audio_mixer.h"
#include <QAudioDevice>
#include <QMediaDevices>

//=============================================================================
// AudioDevice Implementation (QIODevice wrapper for AudioMixer)
//=============================================================================

AudioOutput::AudioDevice::AudioDevice(AudioMixer* mixer, QObject* parent)
    : QIODevice(parent)
    , mixer(mixer)
{
    open(QIODevice::ReadOnly);
}

qint64 AudioOutput::AudioDevice::readData(char* data, qint64 maxlen) {
    if (!mixer) return 0;
    
    // Calculate number of frames to generate
    // Each frame = 2 channels × 2 bytes = 4 bytes
    int numFrames = maxlen / 4;
    if (numFrames == 0) return 0;
    
    // Generate samples from mixer
    mixer->generateSamples(reinterpret_cast<int16_t*>(data), numFrames);
    
    return numFrames * 4;
}

qint64 AudioOutput::AudioDevice::writeData(const char* data, qint64 len) {
    // Read-only device
    Q_UNUSED(data);
    Q_UNUSED(len);
    return -1;
}

//=============================================================================
// AudioOutput Implementation
//=============================================================================

AudioOutput::AudioOutput(QObject* parent)
    : QObject(parent)
    , mixer(nullptr)
    , playing(false)
    , volume(1.0f)
    , bufferSize(0)
{
    setupFormat();
}

AudioOutput::~AudioOutput() {
    stop();
}

void AudioOutput::setupFormat() {
    // Configure audio format: 32 kHz, 16-bit stereo
    format.setSampleRate(SAMPLE_RATE);
    format.setChannelCount(CHANNELS);
    format.setSampleFormat(QAudioFormat::Int16);
}

bool AudioOutput::initializeAudio() {
    if (!mixer) {
        qWarning("AudioOutput: No mixer set");
        return false;
    }
    
    // Get default audio output device
    QAudioDevice audioDevice = QMediaDevices::defaultAudioOutput();
    if (audioDevice.isNull()) {
        qWarning("AudioOutput: No audio output device available");
        return false;
    }
    
    // Check if format is supported
    if (!audioDevice.isFormatSupported(format)) {
        qWarning("AudioOutput: Requested format not supported");
        
        // Try to find a similar format
        QAudioFormat nearestFormat = audioDevice.preferredFormat();
        nearestFormat.setSampleRate(SAMPLE_RATE);
        nearestFormat.setChannelCount(CHANNELS);
        
        if (audioDevice.isFormatSupported(nearestFormat)) {
            format = nearestFormat;
            qInfo("AudioOutput: Using nearest supported format");
        } else {
            return false;
        }
    }
    
    // Create audio sink
    audioSink = std::make_unique<QAudioSink>(audioDevice, format);
    audioSink->setVolume(volume);
    
    // Calculate buffer size (aim for ~50ms latency)
    bufferSize = (SAMPLE_RATE * CHANNELS * sizeof(int16_t) * 50) / 1000;
    audioSink->setBufferSize(bufferSize);
    
    qInfo("AudioOutput: Initialized - Sample Rate: %d Hz, Channels: %d, Buffer: %d bytes",
          format.sampleRate(), format.channelCount(), bufferSize);
    
    return true;
}

bool AudioOutput::start() {
    if (playing) {
        qWarning("AudioOutput: Already playing");
        return true;
    }
    
    if (!mixer) {
        qWarning("AudioOutput: Cannot start - no mixer set");
        return false;
    }
    
    // Initialize audio system
    if (!initializeAudio()) {
        qWarning("AudioOutput: Failed to initialize audio");
        return false;
    }
    
    // Create audio device wrapper
    audioDevice = std::make_unique<AudioDevice>(mixer);
    
    // Start playback
    audioSink->start(audioDevice.get());
    
    if (audioSink->error() != QAudio::NoError) {
        qWarning("AudioOutput: Failed to start playback - error: %d", 
                 static_cast<int>(audioSink->error()));
        audioDevice.reset();
        audioSink.reset();
        return false;
    }
    
    playing = true;
    qInfo("AudioOutput: Playback started");
    return true;
}

void AudioOutput::stop() {
    if (!playing) return;
    
    if (audioSink) {
        audioSink->stop();
    }
    
    audioDevice.reset();
    audioSink.reset();
    playing = false;
    
    qInfo("AudioOutput: Playback stopped");
}

void AudioOutput::pause() {
    if (!playing || !audioSink) return;
    
    audioSink->suspend();
    qInfo("AudioOutput: Playback paused");
}

void AudioOutput::resume() {
    if (!playing || !audioSink) return;
    
    audioSink->resume();
    qInfo("AudioOutput: Playback resumed");
}

void AudioOutput::setVolume(float vol) {
    volume = qBound(0.0f, vol, 1.0f);
    
    if (audioSink) {
        audioSink->setVolume(volume);
    }
}
