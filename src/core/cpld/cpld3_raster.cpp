#include "cpld3_raster.h"
#include <iostream>

CPLD3_Raster::CPLD3_Raster()
    : tableMode(false)
    , scrollOffsetReg(0)
    , paletteSelectReg(0)
    , currentScrollOffset(0)
    , currentPaletteSelect(0)
    , tableIndex(0)
    , tableAddr(0)
    , tableByteOffset(0)
    , irqScanline(0)
    , irqEnable(false)
    , irqPending(false)
    , irqCallback(nullptr)
{
    reset();
}

void CPLD3_Raster::reset() {
    tableMode = false;
    scrollOffsetReg = 0;
    paletteSelectReg = 0;
    currentScrollOffset = 0;
    currentPaletteSelect = 0;
    tableIndex = 0;
    tableAddr = 0;
    tableByteOffset = 0;
    irqScanline = 0;
    irqEnable = false;
    irqPending = false;
    
    // Clear table
    for (auto& entry : scanlineTable) {
        entry.scrollOffset = 0;
        entry.paletteSelect = 0;
    }
}

uint8_t CPLD3_Raster::readByte(const Address& address) {
    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();
    uint32_t offset = flatAddr - getBaseAddress();
    
    switch (offset) {
        // SCROLL_OFFSET ($400300)
        case 0x00:
            return scrollOffsetReg & 0xFF;
        case 0x01:
            return (scrollOffsetReg >> 8) & 0xFF;
            
        // PALETTE_SELECT ($400302)
        case 0x02:
            return paletteSelectReg;
        case 0x03:
            return 0x00;
            
        // IRQ_SCANLINE ($400304)
        case 0x04:
            return irqScanline & 0xFF;
        case 0x05:
            return (irqScanline >> 8) & 0xFF;
            
        // IRQ_ENABLE ($400306)
        case 0x06:
            return irqEnable ? 0x01 : 0x00;
        case 0x07:
            return 0x00;
            
        // IRQ_STATUS ($400308)
        case 0x08:
            return irqPending ? 0x01 : 0x00;
        case 0x09:
            return 0x00;
            
        // TABLE_MODE ($400310)
        case 0x10:
            return tableMode ? 0x01 : 0x00;
        case 0x11:
            return 0x00;
            
        // TABLE_ADDR ($400312)
        case 0x12:
            return tableAddr & 0xFF;
        case 0x13:
            return (tableAddr >> 8) & 0xFF;
            
        // TABLE_STATUS ($400316)
        case 0x16:
            return tableIndex & 0xFF;
        case 0x17:
            return (tableIndex >> 8) & 0xFF;
            
        default:
            return 0x00;
    }
}

void CPLD3_Raster::storeByte(const Address& address, uint8_t value) {

    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();
    uint32_t offset = flatAddr - getBaseAddress();
    
    switch (offset) {
        // SCROLL_OFFSET ($400300)
        case 0x00:
            scrollOffsetReg = (scrollOffsetReg & 0xFF00) | value;
            break;
        case 0x01:
            scrollOffsetReg = (scrollOffsetReg & 0x00FF) | (value << 8);
            break;
            
        // PALETTE_SELECT ($400302)
        case 0x02:
            paletteSelectReg = value;
            break;
            
        // IRQ_SCANLINE ($400304)
        case 0x04:
            irqScanline = (irqScanline & 0xFF00) | value;
            break;
        case 0x05:
            irqScanline = (irqScanline & 0x00FF) | ((value & 0x01) << 8);
            break;
            
        // IRQ_ENABLE ($400306)
        case 0x06:
            irqEnable = (value & 0x01) != 0;
            break;
            
        // IRQ_STATUS ($400308) - write 1 to clear
        case 0x08:
            if (value & 0x01) {
                irqPending = false;
            }
            break;
            
        // TABLE_MODE ($400310)
        case 0x10:
            tableMode = (value & 0x01) != 0;
            if (tableMode) {
                tableIndex = 0;  // Reset table position
            }
            break;
            
        // TABLE_ADDR ($400312)
        case 0x12:
            tableAddr = (tableAddr & 0xFF00) | value;
            tableByteOffset = 0;
            break;
        case 0x13:
            tableAddr = (tableAddr & 0x00FF) | ((value & 0x01) << 8);
            tableByteOffset = 0;
            break;
            
        // TABLE_DATA ($400314)
        case 0x14:
            if (tableAddr < 262) {
                if (tableByteOffset == 0) {
                    // Scroll offset low byte
                    scanlineTable[tableAddr].scrollOffset = 
                        (scanlineTable[tableAddr].scrollOffset & 0xFF00) | value;
                    tableByteOffset++;
                } else if (tableByteOffset == 1) {
                    // Scroll offset high byte
                    scanlineTable[tableAddr].scrollOffset = 
                        (scanlineTable[tableAddr].scrollOffset & 0x00FF) | (value << 8);
                    tableByteOffset++;
                } else if (tableByteOffset == 2) {
                    // Palette select
                    scanlineTable[tableAddr].paletteSelect = value;
                    tableByteOffset = 0;
                    tableAddr++;  // Auto-increment after complete entry
                }
            }
            break;
            
        default:
            break;
    }
}

bool CPLD3_Raster::decodeAddress(const Address& address, Address& decoded) {
    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();
    if (flatAddr >= getBaseAddress() && flatAddr < getBaseAddress() + getSize()) {
        decoded = address;
        return true;
    }
    return false;
}

void CPLD3_Raster::onHSync(uint16_t currentLine) {
    // Update effects for this scanline
    updateEffects();
    
    // Check IRQ condition
    checkIRQ(currentLine);
}

void CPLD3_Raster::updateEffects() {
    if (tableMode) {
        // Table mode: use pre-loaded table
        if (tableIndex < 262) {
            currentScrollOffset = scanlineTable[tableIndex].scrollOffset;
            currentPaletteSelect = scanlineTable[tableIndex].paletteSelect;
        }
        
        // Auto-advance table index
        tableIndex++;
        if (tableIndex >= 262) {
            tableIndex = 0;  // Wrap at frame end
        }
    } else {
        // Register mode: use register values
        currentScrollOffset = scrollOffsetReg;
        currentPaletteSelect = paletteSelectReg;
    }
}

void CPLD3_Raster::checkIRQ(uint16_t currentLine) {
    if (irqEnable && (currentLine == irqScanline)) {
        if (!irqPending) {
            irqPending = true;
            if (irqCallback) {
                irqCallback();
            }
        }
    }
}
