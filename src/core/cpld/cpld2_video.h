#ifndef CPLD2_VIDEO_H
#define CPLD2_VIDEO_H

#include "../cpu/SystemBusDevice.hpp"
#include <cstdint>
#include <functional>
#include "../memory/ram.h"
#include "../memory/mailbox.h"

class Mailbox;

/**
 * CPLD #2: Video Timing Generator & VRAM Arbiter
 * 
 * Generates HSYNC/VSYNC timing signals
 * Tracks raster position (scanline, pixel)
 * Arbitrates VRAM access (G-CPU vs raster engine)
 * Manages mailbox control
 * 
 * Register Map: $400200-$40021F
 */
class CPLD2_Video : public SystemBusDevice {
public:
    CPLD2_Video();
    ~CPLD2_Video() override = default;

    void setGraphicsRAM(RAM* ram) { graphicsRAM = ram; }
    void setGraphicsCPUReset(std::function<void(bool)> callback) { graphicsCPUReset = callback; }
    
    // MemoryDevice interface
    uint8_t readByte(const Address& address) override;
    void storeByte(const Address& address, uint8_t value) override;
    bool decodeAddress(const Address& address, Address& decoded) override;
    uint32_t getBaseAddress() const { return 0x400200; }
    uint32_t getSize() const { return 0x20; }

    // Mailbox management (CPLD2 watches mailboxes and triggers IRQs)
    void setMailboxA(Mailbox* mailbox) { mailboxA = mailbox; }
    void setMailboxB(Mailbox* mailbox) { mailboxB = mailbox; }

    // Called by emulator when mailbox is written
    void onMailboxAWrite();
    void onMailboxBWrite();
    
    // Timing - called at PIXCLK rate (13.5 MHz)
    void tick();
    
    // Video mode
    enum class VideoMode {
        MODE_240P = 0,
        MODE_480I = 1
    };
    
    void setVideoMode(VideoMode mode) { videoMode = mode; }
    VideoMode getVideoMode() const { return videoMode; }
    uint8_t getRegister(uint8_t reg) {
        uint32_t addr = getBaseAddress() + reg;
        Address address(addr >> 16, addr & 0xFFFF);
        return readByte(address);
    }

    void setRegister(uint8_t reg, uint8_t value) {
        uint32_t addr = getBaseAddress() + reg;
        Address address(addr >> 16, addr & 0xFFFF);
        storeByte(address, value);
    }

    // Raster position
    uint16_t getRasterLine() const { return rasterLine; }
    uint16_t getRasterX() const { return rasterX; }
    
    // Blanking status
    bool isInVBlank() const { return inVBlank; }
    bool isInHBlank() const { return inHBlank; }
    
    // VRAM arbiter - check if G-CPU can access VRAM
    bool allowGCpuVramAccess() const;
    
    // IRQ callbacks
    using IRQCallback = std::function<void()>;
    void setVBlankCallback(IRQCallback callback) { vblankCallback = callback; }
    void setHBlankCallback(IRQCallback callback) { hblankCallback = callback; }

    // Mailbox IRQ callbacks
    void setMailboxACallback(IRQCallback callback) { mailboxACallback = callback; }
    void setMailboxBCallback(IRQCallback callback) { mailboxBCallback = callback; }
    
    // Reset
    void reset();
    
private:

    RAM* graphicsRAM = nullptr;
    std::function<void(bool)> graphicsCPUReset;

    // Video mode
    VideoMode videoMode;
    
    // Raster position
    uint16_t rasterLine;
    uint16_t rasterX;
    
    // Blanking flags
    bool inVBlank;
    bool inHBlank;
    
    // IRQ state
    bool vblankIRQPending;
    bool hblankIRQPending;
    
    // IRQ callbacks
    IRQCallback vblankCallback;
    IRQCallback hblankCallback;
    
    // Timing constants (240p mode)
    static constexpr uint16_t PIXELS_PER_LINE = 857;
    static constexpr uint16_t LINES_PER_FRAME_240P = 262;
    static constexpr uint16_t LINES_PER_FRAME_480I = 525;
    
    static constexpr uint16_t HBLANK_START = 0;
    static constexpr uint16_t HBLANK_END = 137;
    static constexpr uint16_t ACTIVE_START = 138;
    
    static constexpr uint16_t VBLANK_LINES_240P = 22;
    static constexpr uint16_t VBLANK_LINES_480I = 22;  // Per field
    
    // Update blanking flags
    void updateBlankingFlags();
    
    // Get total lines for current mode
    uint16_t getTotalLines() const;

    // Mailbox references
    Mailbox* mailboxA;
    Mailbox* mailboxB;

    // Mailbox IRQ callbacks
    IRQCallback mailboxACallback;
    IRQCallback mailboxBCallback;
};

#endif // CPLD2_VIDEO_H
