#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <QObject>
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <memory>

// Forward declarations
class AudioMixer;

/**
 * Audio Output
 * 
 * Qt Multimedia wrapper for audio output.
 * Manages the Qt audio system and feeds it with samples from AudioMixer.
 * 
 * Usage:
 *   AudioOutput output;
 *   output.setMixer(&mixer);
 *   output.start();
 *   // ... audio plays automatically ...
 *   output.stop();
 */
class AudioOutput : public QObject {
    Q_OBJECT
    
public:
    explicit AudioOutput(QObject* parent = nullptr);
    ~AudioOutput();
    
    // Configuration
    void setMixer(AudioMixer* mixer) { this->mixer = mixer; }
    
    // Playback control
    bool start();
    void stop();
    void pause();
    void resume();
    
    // Volume control
    void setVolume(float volume);  // 0.0-1.0
    float getVolume() const { return volume; }
    
    // Status
    bool isPlaying() const { return playing; }
    int getBufferSize() const { return bufferSize; }
    
    // Constants
    static constexpr int SAMPLE_RATE = 32000;  // 32 kHz
    static constexpr int CHANNELS = 2;         // Stereo
    static constexpr int SAMPLE_SIZE = 16;     // 16-bit
    
private:
    // Audio device (custom QIODevice that reads from mixer)
    class AudioDevice : public QIODevice {
    public:
        explicit AudioDevice(AudioMixer* mixer, QObject* parent = nullptr);
        
        qint64 readData(char* data, qint64 maxlen) override;
        qint64 writeData(const char* data, qint64 len) override;
        
    private:
        AudioMixer* mixer;
    };
    
    // Components
    AudioMixer* mixer;
    QAudioFormat format;
    std::unique_ptr<QAudioSink> audioSink;
    std::unique_ptr<AudioDevice> audioDevice;
    
    // State
    bool playing;
    float volume;
    int bufferSize;
    
    // Setup
    void setupFormat();
    bool initializeAudio();
};

#endif // AUDIO_OUTPUT_H
