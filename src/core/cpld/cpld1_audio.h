#ifndef CPLD1_AUDIO_H
#define CPLD1_AUDIO_H

#include "../cpu/SystemBusDevice.hpp"
#include <queue>
#include <array>
#include <cstdint>
#include <functional>
#include "../memory/mailbox.h"
#include "../memory/ram.h"
/**
 * CPLD #1: Audio FIFO Serializer & TDM Generator
 * 
 * Manages 8 independent 256-sample FIFOs
 * Drains at 32 kHz and generates TDM output to ADAU1452 DSP
 * Generates IRQ when FIFO level < threshold
 * 
 * Register Map: $400100-$40011F
 */
class CPLD1_Audio : public SystemBusDevice {
public:

    void setSoundRAM(RAM* ram) { soundRAM = ram; }
    void setSoundCPUReset(std::function<void(bool)> callback) { soundCPUReset = callback; }

    CPLD1_Audio();
    ~CPLD1_Audio() override = default;
    
    // MemoryDevice interface
    uint8_t readByte(const Address& address) override;
    void storeByte(const Address& address, uint8_t value) override;
    bool decodeAddress(const Address& address, Address& decoded) override;
    uint32_t getBaseAddress() const { return 0x400100; }
    uint32_t getSize() const { return 0x20; }
    
    // Timing - called at 32 kHz sample rate
    void tick();
    
    // Get mixed audio output (for emulator audio system)
    void getAudioFrame(int16_t& leftOut, int16_t& rightOut);
    
    // IRQ callback
    using IRQCallback = std::function<void()>;
    void setIRQCallback(IRQCallback callback) { irqCallback = callback; }
    
    // Reset
    void reset();
    
    // Debug
    uint8_t getFIFOLevel(int channel) const;
    bool getIRQStatus(int channel) const;

    void setMailboxB(Mailbox* mailbox) { mailboxB = mailbox; }
    void setMailboxBCallback(std::function<void()> callback) { mailboxBCallback = callback; }
    void onMailboxBWrite();
    
private:

    RAM* soundRAM = nullptr;
    std::function<void(bool)> soundCPUReset;

    // FIFO structure
    struct AudioFIFO {
        std::queue<int16_t> samples;  // Max 256 samples
        bool irqPending;
        
        AudioFIFO() : irqPending(false) {}
        
        void clear() {
            while (!samples.empty()) samples.pop();
            irqPending = false;
        }
        
        uint8_t getLevel() const {
            return static_cast<uint8_t>(samples.size());
        }
        
        bool isFull() const {
            return samples.size() >= 256;
        }
        
        bool isEmpty() const {
            return samples.empty();
        }
    };
    
    // 8 channel FIFOs
    std::array<AudioFIFO, 8> fifos;
    
    // Registers
    uint8_t irqThreshold;  // FIFO low threshold (default 128)
    uint8_t irqStatus;     // IRQ flags (bit per channel)
    bool enabled;          // Master enable
    
    // IRQ callback
    IRQCallback irqCallback;
    
    // Check if any IRQ pending
    void updateIRQ();
    
    // Check individual channel IRQ
    bool checkChannelIRQ(int channel);

    // Mailbox B reference
    Mailbox* mailboxB = nullptr;
    std::function<void()> mailboxBCallback;
};

#endif // CPLD1_AUDIO_H
