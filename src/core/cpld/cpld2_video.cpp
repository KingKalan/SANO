#include "cpld2_video.h"
#include <iostream>
#include <iomanip>



CPLD2_Video::CPLD2_Video()
    : videoMode(VideoMode::MODE_240P)
    , rasterLine(0)
    , rasterX(0)
    , inVBlank(true)
    , inHBlank(true)
    , vblankIRQPending(false)
    , hblankIRQPending(false)
    , vblankCallback(nullptr)
    , hblankCallback(nullptr)
{
    reset();
}

void CPLD2_Video::reset() {
    rasterLine = 0;
    rasterX = 0;
    inVBlank = true;
    inHBlank = true;
    vblankIRQPending = false;
    hblankIRQPending = false;
}

uint8_t CPLD2_Video::readByte(const Address& address) {
    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();
    uint32_t offset = flatAddr - getBaseAddress();
    
    switch (offset) {
        // VIDEO_MODE ($400200)
        case 0x00:
            return static_cast<uint8_t>(videoMode);
        case 0x01:
            return 0x00;
            
        // RASTER_LINE ($400202)
        case 0x02:
            return rasterLine & 0xFF;
        case 0x03:
            return (rasterLine >> 8) & 0xFF;
            
        // RASTER_X ($400204)
        case 0x04:
            return rasterX & 0xFF;
        case 0x05:
            return (rasterX >> 8) & 0xFF;
            
        // VBLANK_STATUS ($400206)
        case 0x06:
            return inVBlank ? 0x01 : 0x00;
        case 0x07:
            return 0x00;
            
        // HBLANK_STATUS ($400208)
        case 0x08:
            return inHBlank ? 0x01 : 0x00;
        case 0x09:
            return 0x00;
            
        default:
            return 0x00;
    }
}

void CPLD2_Video::storeByte(const Address& address, uint8_t value) {
    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();
    uint32_t offset = flatAddr - getBaseAddress();
    
    switch (offset) {
        // VIDEO_MODE ($400200)
        case 0x00:
            videoMode = (value & 0x01) ? VideoMode::MODE_480I : VideoMode::MODE_240P;
            break;
            
        // IRQ_CLEAR ($40020A)
        case 0x0A:
            if (value != 0) {
                vblankIRQPending = false;
            }
            break;
            
        default:
            break;
    }
}

void CPLD2_Video::onMailboxAWrite() {
    if (mailboxA && graphicsRAM) {
        Address addr(0x40, 0x0000);  // Mailbox A base
        uint8_t cmd = mailboxA->readByte(addr);

        if (cmd == 0x01) {
            addr = Address(0x40, 0x0001);
            uint8_t destLo = mailboxA->readByte(addr);
            addr = Address(0x40, 0x0002);
            uint8_t destHi = mailboxA->readByte(addr);
            uint16_t destAddr = destLo | (destHi << 8);

            addr = Address(0x40, 0x0003);
            uint8_t lenLo = mailboxA->readByte(addr);
            addr = Address(0x40, 0x0004);
            uint8_t lenHi = mailboxA->readByte(addr);
            uint16_t length = lenLo | (lenHi << 8);

            std::cout << "[CPLD2] Boot command: copy " << length
            << " bytes to VRAM $" << std::hex << destAddr << std::dec << std::endl;

            // Copy data from mailbox to VRAM
            for (uint16_t i = 0; i < length; i++) {
                addr = Address(0x40, 0x0005 + i);
                uint8_t data = mailboxA->readByte(addr);
                Address vramAddr(0x00, destAddr + i);
                graphicsRAM->storeByte(vramAddr, data);
            }

            std::cout << "[CPLD2] VRAM hexdump after copy:" << std::endl;
            std::cout << "  ";
            for (uint16_t i = 0; i < 32; i++) {
                Address vramAddr(0x00, i);
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                << (int)graphicsRAM->readByte(vramAddr) << " ";
                if ((i + 1) % 16 == 0) std::cout << std::endl << "  ";
            }
            std::cout << std::dec << std::endl;

            if (graphicsCPUReset) {
                std::cout << "[CPLD2] Releasing Graphics CPU reset" << std::endl;
                graphicsCPUReset(false);
            }

            return;
        }
    }

    if (mailboxACallback) {
        mailboxACallback();
    }
}

void CPLD2_Video::onMailboxBWrite() {
    if (mailboxBCallback) {
        mailboxBCallback();  // Trigger Sound CPU IRQ
    }
}

bool CPLD2_Video::decodeAddress(const Address& address, Address& decoded) {
    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();
    if (flatAddr >= getBaseAddress() && flatAddr < getBaseAddress() + getSize()) {
        decoded = address;
        return true;
    }
    return false;
}

void CPLD2_Video::tick() {
    // Increment pixel counter
    rasterX++;
    
    // Check for line wrap
    if (rasterX >= PIXELS_PER_LINE) {
        rasterX = 0;
        rasterLine++;
        
        // Check for frame wrap
        uint16_t totalLines = getTotalLines();
        if (rasterLine >= totalLines) {
            rasterLine = 0;
            
            // VBlank IRQ on line 0
            if (!vblankIRQPending) {
                vblankIRQPending = true;
                if (vblankCallback) {
                    vblankCallback();
                }
            }
        }
    }
    
    // Update blanking flags
    updateBlankingFlags();
}

void CPLD2_Video::updateBlankingFlags() {
    // HBlank: pixels 0-137
    inHBlank = (rasterX >= HBLANK_START && rasterX <= HBLANK_END);
    
    // VBlank: lines 0-21 (for both modes)
    if (videoMode == VideoMode::MODE_240P) {
        inVBlank = (rasterLine < VBLANK_LINES_240P);
    } else {
        // 480i: VBlank per field
        inVBlank = (rasterLine < VBLANK_LINES_480I) || 
                   (rasterLine >= 262 && rasterLine < 262 + VBLANK_LINES_480I);
    }
}

bool CPLD2_Video::allowGCpuVramAccess() const {
    // G-CPU can access VRAM during blanking periods only
    return inHBlank || inVBlank;
}

uint16_t CPLD2_Video::getTotalLines() const {
    return (videoMode == VideoMode::MODE_240P) ? LINES_PER_FRAME_240P : LINES_PER_FRAME_480I;
}
