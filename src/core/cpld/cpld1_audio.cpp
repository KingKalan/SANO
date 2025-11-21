#include "cpld1_audio.h"
#include <iostream>
#include <algorithm>
#include "../memory/mailbox.h"

CPLD1_Audio::CPLD1_Audio()
    : irqThreshold(128)
    , irqStatus(0)
    , enabled(true)
    , irqCallback(nullptr)
{
    reset();
}

void CPLD1_Audio::reset() {
    for (auto& fifo : fifos) {
        fifo.clear();
    }
    irqThreshold = 128;
    irqStatus = 0;
    enabled = true;
}

uint8_t CPLD1_Audio::readByte(const Address& address) {
    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();
    uint32_t offset = flatAddr - getBaseAddress();
    
    switch (offset) {
        // FIFO_STATUS_0_3 ($400110)
        case 0x10:
            return fifos[0].getLevel();
        case 0x11:
            return fifos[1].getLevel();
        case 0x12:
            return fifos[2].getLevel();
        case 0x13:
            return fifos[3].getLevel();
            
        // FIFO_STATUS_4_7 ($400112)
        case 0x14:
            return fifos[4].getLevel();
        case 0x15:
            return fifos[5].getLevel();
        case 0x16:
            return fifos[6].getLevel();
        case 0x17:
            return fifos[7].getLevel();
            
        // IRQ_STATUS ($400118)
        case 0x18:
            return irqStatus;
            
        // IRQ_THRESHOLD ($40011C)
        case 0x1C:
            return irqThreshold;
            
        default:
            return 0x00;
    }
}

void CPLD1_Audio::storeByte(const Address& address, uint8_t value) {
    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();
    uint32_t offset = flatAddr - getBaseAddress();
    
    // FIFO writes ($400100-$40010E, 16-bit values)
    if (offset >= 0x00 && offset <= 0x0E && (offset % 2) == 0) {
        int channel = offset / 2;
        
        // For 16-bit writes, we need to handle both bytes
        // This is simplified - in real implementation we'd buffer the bytes
        // For now, treat each byte write as a separate 8-bit sample (extended to 16-bit)
        int16_t sample = static_cast<int16_t>(value) << 8;
        
        if (!fifos[channel].isFull()) {
            fifos[channel].samples.push(sample);
        }
        // If full, sample is dropped
        
        return;
    }
    
    switch (offset) {
        // IRQ_CLEAR ($40011A)
        case 0x1A:
            // Clear IRQ flags for channels with bit set
            for (int i = 0; i < 8; i++) {
                if (value & (1 << i)) {
                    fifos[i].irqPending = false;
                    irqStatus &= ~(1 << i);
                }
            }
            updateIRQ();
            break;
            
        // IRQ_THRESHOLD ($40011C)
        case 0x1C:
            irqThreshold = value;
            updateIRQ();  // Recheck IRQ conditions
            break;
            
        // CONFIG ($40011E)
        case 0x1E:
            enabled = (value & 0x01) != 0;
            break;
            
        default:
            break;
    }
}

bool CPLD1_Audio::decodeAddress(const Address& address, Address& decoded) {
    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();
    if (flatAddr >= getBaseAddress() && flatAddr < getBaseAddress() + getSize()) {
        decoded = address;
        return true;
    }
    return false;
}

void CPLD1_Audio::tick() {
    if (!enabled) {
        return;
    }
    
    // Called at 32 kHz - drain one sample from each FIFO
    for (int ch = 0; ch < 8; ch++) {
        if (!fifos[ch].samples.empty()) {
            fifos[ch].samples.pop();
            
            // Check if FIFO dropped below threshold
            if (fifos[ch].getLevel() < irqThreshold) {
                if (!fifos[ch].irqPending) {
                    fifos[ch].irqPending = true;
                    irqStatus |= (1 << ch);
                }
            }
        }
    }
    
    updateIRQ();
}

void CPLD1_Audio::getAudioFrame(int16_t& leftOut, int16_t& rightOut) {
    // Simple mixing: sum all 8 channels and normalize
    int32_t mixL = 0;
    int32_t mixR = 0;
    
    for (int ch = 0; ch < 8; ch++) {
        if (!fifos[ch].samples.empty()) {
            int16_t sample = fifos[ch].samples.front();
            mixL += sample;
            mixR += sample;
        }
    }
    
    // Normalize by number of channels to prevent clipping
    mixL /= 8;
    mixR /= 8;
    
    // Clamp to 16-bit range
    leftOut = std::clamp(mixL, (int32_t)-32768, (int32_t)32767);
    rightOut = std::clamp(mixR, (int32_t)-32768, (int32_t)32767);
}

void CPLD1_Audio::updateIRQ() {
    // Check if any channel has IRQ pending
    bool anyIRQ = (irqStatus != 0);
    
    if (anyIRQ && irqCallback) {
        irqCallback();
    }
}

void CPLD1_Audio::onMailboxBWrite() {
    if (mailboxB && soundRAM) {
        Address addr(0x41, 0x0000);  // Mailbox B base
        uint8_t cmd = mailboxB->readByte(addr);

        if (cmd == 0x01) {
            addr = Address(0x41, 0x0001);
            uint8_t destLo = mailboxB->readByte(addr);
            addr = Address(0x41, 0x0002);
            uint8_t destHi = mailboxB->readByte(addr);
            uint16_t destAddr = destLo | (destHi << 8);

            addr = Address(0x41, 0x0003);
            uint8_t lenLo = mailboxB->readByte(addr);
            addr = Address(0x41, 0x0004);
            uint8_t lenHi = mailboxB->readByte(addr);
            uint16_t length = lenLo | (lenHi << 8);

            std::cout << "[CPLD1] Boot command: copy " << length
            << " bytes to Sound RAM $" << std::hex << destAddr << std::dec << std::endl;

            for (uint16_t i = 0; i < length; i++) {
                addr = Address(0x41, 0x0005 + i);
                uint8_t data = mailboxB->readByte(addr);
                Address ramAddr(0x00, destAddr + i);
                soundRAM->storeByte(ramAddr, data);
            }

            if (soundCPUReset) {
                std::cout << "[CPLD1] Releasing Sound CPU reset" << std::endl;
                soundCPUReset(false);
            }

            return;
        }
    }

    if (mailboxBCallback) {
        mailboxBCallback();
    }
}

bool CPLD1_Audio::checkChannelIRQ(int channel) {
    if (channel < 0 || channel >= 8) {
        return false;
    }
    return fifos[channel].getLevel() < irqThreshold;
}

uint8_t CPLD1_Audio::getFIFOLevel(int channel) const {
    if (channel < 0 || channel >= 8) {
        return 0;
    }
    return fifos[channel].getLevel();
}

bool CPLD1_Audio::getIRQStatus(int channel) const {
    if (channel < 0 || channel >= 8) {
        return false;
    }
    return (irqStatus & (1 << channel)) != 0;
}
