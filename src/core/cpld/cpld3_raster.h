#ifndef CPLD3_RASTER_H
#define CPLD3_RASTER_H

#include "../cpu/SystemBusDevice.hpp"
#include <array>
#include <cstdint>
#include <functional>

/**
 * CPLD #3: Raster FX Engine
 * 
 * Provides per-scanline effects:
 * - Horizontal scroll offset (parallax scrolling)
 * - Palette bank selection (color effects)
 * - Split-line IRQ (mid-frame updates)
 * 
 * Two modes:
 * - Register mode: G-CPU writes value, used for all lines
 * - Table mode: Pre-loaded 262-entry table, auto-advance
 * 
 * Register Map: $400300-$40031F
 */
class CPLD3_Raster : public SystemBusDevice {
public:
    CPLD3_Raster();
    ~CPLD3_Raster() override = default;
    
    // MemoryDevice interface
    uint8_t readByte(const Address& address) override;
    void storeByte(const Address& address, uint8_t value) override;
    bool decodeAddress(const Address& address, Address& decoded) override;
    
    uint32_t getBaseAddress() const { return 0x400300; }
    uint32_t getSize() const { return 0x20; }
    
    // Called on HSYNC (once per scanline)
    void onHSync(uint16_t currentLine);
    
    // Get current effect values for raster engine
    int16_t getScrollOffset() const { return currentScrollOffset; }
    uint8_t getPaletteSelect() const { return currentPaletteSelect; }
    
    // IRQ callback
    using IRQCallback = std::function<void()>;
    void setIRQCallback(IRQCallback callback) { irqCallback = callback; }
    
    // Reset
    void reset();
    
private:
    // Operating mode
    bool tableMode;
    
    // Register mode values
    int16_t scrollOffsetReg;
    uint8_t paletteSelectReg;
    
    // Current output values (latched on HSYNC)
    int16_t currentScrollOffset;
    uint8_t currentPaletteSelect;
    
    // Table mode
    struct TableEntry {
        int16_t scrollOffset;
        uint8_t paletteSelect;
        
        TableEntry() : scrollOffset(0), paletteSelect(0) {}
    };
    
    std::array<TableEntry, 262> scanlineTable;
    uint16_t tableIndex;
    
    // Table loading
    uint16_t tableAddr;
    uint8_t tableByteOffset;  // 0=scroll_lo, 1=scroll_hi, 2=palette
    
    // Split-line IRQ
    uint16_t irqScanline;
    bool irqEnable;
    bool irqPending;
    
    IRQCallback irqCallback;
    
    // Update current values (called on HSYNC)
    void updateEffects();
    
    // Check IRQ condition
    void checkIRQ(uint16_t currentLine);
};

#endif // CPLD3_RASTER_H
