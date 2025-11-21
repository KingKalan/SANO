#ifndef VIDEO_RENDERER_H
#define VIDEO_RENDERER_H

#include <cstdint>
#include <array>
#include <vector>

// Forward declarations
class CPLD2_Video;
class CPLD3_Raster;
class RAM;

/**
 * Video Renderer
 * 
 * Emulates SANo video output with hardware-accurate rendering pipeline:
 * - 5 tilemap layers (BG0, BG1, FG0, FG1, HUD)
 * - 512 sprites (128 per scanline)
 * - 256-color RGB565 palette
 * - Hardware effects (mosaic, alpha, windows, raster FX)
 * - 320×240 render resolution
 */
class VideoRenderer {
public:
    VideoRenderer();
    ~VideoRenderer();
    
    // Configuration
    void setCPLD2(CPLD2_Video* cpld2) { this->cpld2 = cpld2; }
    void setCPLD3(CPLD3_Raster* cpld3) { this->cpld3 = cpld3; }
    void setVRAM(RAM* vram) { this->vram = vram; }
    
    // Frame rendering
    void renderFrame();
    void renderScanline(uint16_t line);
    
    // Framebuffer access
    const uint32_t* getFramebuffer() const { return framebuffer.data(); }
    
    // Display dimensions
    static constexpr int WIDTH = 320;
    static constexpr int HEIGHT = 240;
    
    // Reset
    void reset();
    
private:
    // CPLD and memory references
    CPLD2_Video* cpld2;
    CPLD3_Raster* cpld3;
    RAM* vram;
    
    // Framebuffer (RGBA8888 for display)
    std::array<uint32_t, WIDTH * HEIGHT> framebuffer;
    
    // Line buffers for compositing
    struct LineBuffer {
        std::array<uint8_t, WIDTH> color;      // Palette index (0-255)
        std::array<uint8_t, WIDTH> priority;   // Layer priority (0-15)
        std::array<uint8_t, WIDTH> alpha;      // Alpha level (0-16, 16=opaque)
    };
    
    std::array<LineBuffer, 6> layerBuffers; // BG0, BG1, FG0, FG1, HUD, Sprites
    LineBuffer finalBuffer;
    
    // VRAM layout (from video spec)
    static constexpr uint32_t PALETTE_RAM = 0x014000;
    static constexpr uint32_t SPRITE_OAM = 0x013000;
    static constexpr uint32_t TILEMAP_BG0 = 0x015000;
    static constexpr uint32_t TILEMAP_BG1 = 0x017000;
    static constexpr uint32_t TILEMAP_FG0 = 0x019000;
    static constexpr uint32_t TILEMAP_FG1 = 0x01B000;
    static constexpr uint32_t TILEMAP_HUD = 0x01D000;
    static constexpr uint32_t TILE_DATA = 0x020000;
    static constexpr uint32_t FRAMEBUFFER = 0x000000;
    
    // Palette cache (RGB565 → RGBA8888)
    std::array<uint32_t, 256> paletteRGBA;
    bool paletteDirty;
    
    // Sprite structures
    struct Sprite {
        uint16_t x, y;
        uint8_t tile;
        uint8_t attributes;  // [palBank:4][alpha:4]
        uint8_t flags;       // [size:2][flip:2][rotate:1][enable:1]
        uint8_t priority;
        
        bool enabled() const { return flags & 0x01; }
        bool hflip() const { return flags & 0x04; }
        bool vflip() const { return flags & 0x08; }
        bool rotate() const { return flags & 0x02; }
        uint8_t size() const { return (flags >> 4) & 0x03; }
        uint8_t palBank() const { return (attributes >> 4) & 0x0F; }
        uint8_t alpha() const { return attributes & 0x0F; }
    };
    
    std::array<Sprite, 512> spriteCache;
    bool spriteCacheDirty;
    
    // Video mode registers (from CPLD2)
    struct VideoMode {
        uint8_t mode;         // 0-3 (framebuffer, standard, max layers, bg-only)
        uint8_t layerEnable;  // Bit mask for enabled layers
        uint8_t mosaic;       // Mosaic size (0-15)
        uint8_t brightness;   // Global brightness (0-31)
        int8_t tintR, tintG, tintB;  // Color tint offsets
    };
    
    // Layer configuration (from CPLD2)
    struct LayerConfig {
        uint16_t scrollX, scrollY;
        uint8_t bpp;          // Bits per pixel (2, 4, 8)
        uint8_t tileSize;     // 0=8×8, 1=16×16
        uint8_t mapSize;      // 0=32×32, 1=64×64
        uint8_t priority;
        uint8_t palBank;
    };
    
    // Helper functions - VRAM access
    uint8_t readVRAM(uint32_t addr);
    uint16_t readVRAM16(uint32_t addr);
    void updatePaletteCache();
    void updateSpriteCache();
    
    // Helper functions - rendering
    void clearBuffers();
    void compositeBuffers(uint16_t line);
    void applyEffects(uint16_t line);
    
    // Layer rendering
    void renderFramebufferMode(uint16_t line);
    void renderTileLayer(uint16_t line, int layerIndex);
    void renderSpritesOnLine(uint16_t line);
    
    // Tile decoding
    void decodeTile8x8_2bpp(uint8_t* dest, uint32_t tileAddr, uint8_t palBank, bool hflip, bool vflip);
    void decodeTile8x8_4bpp(uint8_t* dest, uint32_t tileAddr, uint8_t palBank, bool hflip, bool vflip);
    void decodeTile8x8_8bpp(uint8_t* dest, uint32_t tileAddr, bool hflip, bool vflip);
    
    // Effects
    void applyMosaic(uint8_t* buffer, int width, uint8_t mosaicSize);
    void applyWindow(LineBuffer& buffer, uint16_t line, uint8_t windowMask);
    uint32_t applyBrightness(uint32_t color, uint8_t brightness);
    uint32_t applyTint(uint32_t color, int8_t r, int8_t g, int8_t b);
    uint32_t blendAlpha(uint32_t fg, uint32_t bg, uint8_t alpha);
    
    // Color conversion
    uint32_t rgb565_to_rgba8888(uint16_t rgb565);
};

#endif // VIDEO_RENDERER_H
