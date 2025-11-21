#include "video_renderer.h"
#include "../cpld/cpld2_video.h"
#include "../cpld/cpld3_raster.h"
#include "../memory/ram.h"
#include <cstring>
#include <algorithm>
#include <iostream>

VideoRenderer::VideoRenderer()
    : cpld2(nullptr)
    , cpld3(nullptr)
    , vram(nullptr)
    , paletteDirty(true)
    , spriteCacheDirty(true)
{
    reset();
}

VideoRenderer::~VideoRenderer() {
}

void VideoRenderer::reset() {
    framebuffer.fill(0xFF000000);  // Black
    paletteDirty = true;
    spriteCacheDirty = true;

    // Initialize default grayscale palette
    for (int i = 0; i < 256; ++i) {
        paletteRGBA[i] = 0xFF000000 | (i << 16) | (i << 8) | i;
    }
    
    for (auto& buf : layerBuffers) {
        buf.color.fill(0);
        buf.priority.fill(0);
        buf.alpha.fill(16);  // Fully opaque
    }
}

//=============================================================================
// Frame Rendering
//=============================================================================

void VideoRenderer::renderFrame() {
    for (int line = 0; line < HEIGHT; ++line) {
        renderScanline(line);

        static int frameCount = 0;
        frameCount++;

        if (frameCount == 120) {
            std::cout << "[RENDERER F120] VRAM[$20]=$" << std::hex << (int)readVRAM(0x20) << std::dec << std::endl;
            std::cout << "[RENDERER F120] framebuffer[32]=$" << std::hex << framebuffer[32] << std::dec << std::endl;
        }
    }
}

void VideoRenderer::renderScanline(uint16_t line) {
    if (!cpld2 || !vram) return;
    
    // Clear line buffers
    //clearBuffers();
    
    // Update palette cache if needed (ALWAYS, regardless of mode)
    if (paletteDirty) {
        updatePaletteCache();
        paletteDirty = false;
    }
    
    // Get video mode from CPLD2
    uint8_t videoMode = cpld2->getRegister(0x00);
    if ((videoMode & 0x03) == 0){
        renderFramebufferMode(line);
        return;
    }

    uint8_t layerEnable = cpld2->getRegister(0x01);

    if (spriteCacheDirty) {
        updateSpriteCache();
        spriteCacheDirty = false;
    }
    
    // Render based on mode
    switch (videoMode & 0x03) {
        case 0:  // Framebuffer mode
            renderFramebufferMode(line);
            return;
            
        case 1:  // Standard mode (5 tilemaps + sprites)
        case 2:  // Max layers mode (6 tilemaps, no sprites)
        case 3:  // Background-only mode (2 backgrounds)
            // Render enabled tilemap layers
            if (layerEnable & 0x01) renderTileLayer(line, 0);  // BG0
            if (layerEnable & 0x02) renderTileLayer(line, 1);  // BG1
            if (layerEnable & 0x04) renderTileLayer(line, 2);  // FG0
            if (layerEnable & 0x08) renderTileLayer(line, 3);  // FG1
            if (layerEnable & 0x10) renderTileLayer(line, 4);  // HUD
            
            // Render sprites (if not in background-only or max layers mode)
            if ((videoMode & 0x03) == 1 && (layerEnable & 0x20)) {
                renderSpritesOnLine(line);
            }
            break;
    }
    
    // Composite all layers
    compositeBuffers(line);
    
    // Apply post-processing effects
    applyEffects(line);
}

//=============================================================================
// Cache Updates
//=============================================================================

void VideoRenderer::updatePaletteCache() {
    // Convert RGB565 palette to RGBA8888
    for (int i = 0; i < 256; ++i) {
        uint16_t rgb565 = readVRAM16(PALETTE_RAM + i * 2);
        paletteRGBA[i] = rgb565_to_rgba8888(rgb565);
    }
}

void VideoRenderer::updateSpriteCache() {
    // Load sprite attributes from OAM
    for (int i = 0; i < 512; ++i) {
        uint32_t oamAddr = SPRITE_OAM + i * 8;
        
        spriteCache[i].x = readVRAM16(oamAddr + 0);
        spriteCache[i].y = readVRAM16(oamAddr + 2);
        spriteCache[i].tile = readVRAM(oamAddr + 4);
        spriteCache[i].attributes = readVRAM(oamAddr + 5);
        spriteCache[i].flags = readVRAM(oamAddr + 6);
        spriteCache[i].priority = readVRAM(oamAddr + 7);
    }
}

//=============================================================================
// Framebuffer Mode Rendering
//=============================================================================

void VideoRenderer::renderFramebufferMode(uint16_t line) {
    // Direct framebuffer rendering (8bpp indexed)
    // Framebuffer is 320Ã—240 Ã— 1 byte = 76,800 bytes
    uint32_t fbAddr = FRAMEBUFFER + line * WIDTH;
    
    for (int x = 0; x < WIDTH; ++x) {
        uint8_t palIndex = readVRAM(fbAddr + x);
        framebuffer[line * WIDTH + x] = paletteRGBA[palIndex];
    }
}

//=============================================================================
// Tile Layer Rendering
//=============================================================================

void VideoRenderer::renderTileLayer(uint16_t line, int layerIndex) {
    // Get layer configuration from CPLD2
    uint8_t baseReg = 0x10 + layerIndex * 8;
    uint16_t scrollX = cpld2->getRegister(baseReg + 0) | (cpld2->getRegister(baseReg + 1) << 8);
    uint16_t scrollY = cpld2->getRegister(baseReg + 2) | (cpld2->getRegister(baseReg + 3) << 8);
    uint8_t control = cpld2->getRegister(baseReg + 4);
    uint8_t priority = cpld2->getRegister(baseReg + 5);
    
    // Decode control bits
    uint8_t bpp = ((control >> 0) & 0x03);  // 0=2bpp, 1=4bpp, 2=8bpp
    uint8_t tileSize = (control >> 2) & 0x01;  // 0=8Ã—8, 1=16Ã—16
    uint8_t mapSize = (control >> 3) & 0x01;   // 0=32Ã—32, 1=64Ã—64
    uint8_t palBank = (control >> 4) & 0x0F;
    
    // Get tilemap base address
    static const uint32_t tilemapBases[] = {
        TILEMAP_BG0, TILEMAP_BG1, TILEMAP_FG0, TILEMAP_FG1, TILEMAP_HUD
    };
    uint32_t tilemapBase = tilemapBases[layerIndex];
    
    // Calculate which row of tiles we're on
    uint16_t worldY = (line + scrollY) & 0x1FF;  // Wrap at 512
    uint16_t tileY = worldY / (tileSize ? 16 : 8);
    uint16_t pixelY = worldY % (tileSize ? 16 : 8);
    
    // Tilemap dimensions
    uint16_t mapWidth = (mapSize ? 64 : 32);
    
    // Render tiles for this scanline
    for (int screenX = 0; screenX < WIDTH; ++screenX) {
        uint16_t worldX = (screenX + scrollX) & 0x1FF;  // Wrap at 512
        uint16_t tileX = worldX / (tileSize ? 16 : 8);
        uint16_t pixelX = worldX % (tileSize ? 16 : 8);
        
        // Get tile entry from tilemap
        uint32_t tileMapAddr = tilemapBase + (tileY * mapWidth + tileX) * 2;
        uint16_t tileEntry = readVRAM16(tileMapAddr);
        
        // Decode tile entry: [tile:10][palBank:4][vflip:1][hflip:1]
        uint16_t tileNum = tileEntry & 0x3FF;
        bool hflip = (tileEntry & 0x0400) != 0;
        bool vflip = (tileEntry & 0x0800) != 0;
        uint8_t tilePalBank = (tileEntry >> 12) & 0x0F;
        
        // Apply flip to pixel coordinates
        uint16_t px = hflip ? ((tileSize ? 15 : 7) - pixelX) : pixelX;
        uint16_t py = vflip ? ((tileSize ? 15 : 7) - pixelY) : pixelY;
        
        // Calculate tile data address
        uint32_t bytesPerTile = (tileSize ? 16 : 8) * (tileSize ? 16 : 8);
        if (bpp == 1) bytesPerTile /= 2;  // 4bpp
        else if (bpp == 0) bytesPerTile /= 4;  // 2bpp
        
        uint32_t tileAddr = TILE_DATA + tileNum * bytesPerTile;
        
        // Get pixel color
        uint8_t colorIndex = 0;
        
        if (tileSize == 0) {  // 8Ã—8 tile
            uint32_t pixelAddr = tileAddr + py * 8;
            
            switch (bpp) {
                case 0: {  // 2bpp
                    uint8_t byte = readVRAM(pixelAddr + px / 4);
                    colorIndex = (byte >> ((3 - (px % 4)) * 2)) & 0x03;
                    colorIndex |= (tilePalBank << 4);
                    break;
                }
                case 1: {  // 4bpp
                    uint8_t byte = readVRAM(pixelAddr + px / 2);
                    colorIndex = (px & 1) ? (byte & 0x0F) : (byte >> 4);
                    colorIndex |= (tilePalBank << 4);
                    break;
                }
                case 2: {  // 8bpp
                    colorIndex = readVRAM(pixelAddr + px);
                    break;
                }
            }
        }
        else {  // 16Ã—16 tile
            uint32_t pixelAddr = tileAddr + py * 16;
            
            switch (bpp) {
                case 0: {  // 2bpp
                    uint8_t byte = readVRAM(pixelAddr + px / 4);
                    colorIndex = (byte >> ((3 - (px % 4)) * 2)) & 0x03;
                    colorIndex |= (tilePalBank << 4);
                    break;
                }
                case 1: {  // 4bpp
                    uint8_t byte = readVRAM(pixelAddr + px / 2);
                    colorIndex = (px & 1) ? (byte & 0x0F) : (byte >> 4);
                    colorIndex |= (tilePalBank << 4);
                    break;
                }
                case 2: {  // 8bpp
                    colorIndex = readVRAM(pixelAddr + px);
                    break;
                }
            }
        }
        
        // Skip transparent pixels (color 0)
        if (colorIndex == 0) continue;
        
        // Write to layer buffer
        layerBuffers[layerIndex].color[screenX] = colorIndex;
        layerBuffers[layerIndex].priority[screenX] = priority;
        layerBuffers[layerIndex].alpha[screenX] = 16;  // Opaque
    }
}

//=============================================================================
// Sprite Rendering
//=============================================================================

void VideoRenderer::renderSpritesOnLine(uint16_t line) {
    // Sprites are in layer buffer index 5
    constexpr int SPRITE_LAYER = 5;
    
    int spritesOnLine = 0;
    
    // Render sprites in reverse priority order (highest last)
    for (int i = 511; i >= 0 && spritesOnLine < 128; --i) {
        const Sprite& spr = spriteCache[i];
        if (!spr.enabled()) continue;
        
        // Get sprite dimensions
        static const int spriteSizes[] = { 8, 16, 32, 64 };
        int spriteHeight = spriteSizes[spr.size()];
        
        // Check if sprite intersects this scanline
        if (line < spr.y || line >= spr.y + spriteHeight) continue;
        
        spritesOnLine++;
        
        int spriteWidth = spriteHeight;  // Square sprites
        uint16_t spriteY = line - spr.y;
        
        // Apply vertical flip
        if (spr.vflip()) {
            spriteY = spriteHeight - 1 - spriteY;
        }
        
        // Get tile data
        uint32_t tileAddr = TILE_DATA + spr.tile * 64;  // Assume 8Ã—8 base tile, 8bpp
        
        // Render sprite pixels
        for (int sx = 0; sx < spriteWidth; ++sx) {
            int screenX = spr.x + sx;
            if (screenX < 0 || screenX >= WIDTH) continue;
            
            uint16_t spriteX = sx;
            
            // Apply horizontal flip
            if (spr.hflip()) {
                spriteX = spriteWidth - 1 - spriteX;
            }
            
            // Get pixel (simplified - assumes 8Ã—8 tiles)
            uint32_t pixelAddr = tileAddr + (spriteY % 8) * 8 + (spriteX % 8);
            uint8_t colorIndex = readVRAM(pixelAddr);
            
            // Apply palette bank
            colorIndex = (colorIndex & 0x0F) | (spr.palBank() << 4);
            
            // Skip transparent pixels
            if ((colorIndex & 0x0F) == 0) continue;
            
            // Check if higher priority than what's already in buffer
            if (spr.priority >= layerBuffers[SPRITE_LAYER].priority[screenX]) {
                layerBuffers[SPRITE_LAYER].color[screenX] = colorIndex;
                layerBuffers[SPRITE_LAYER].priority[screenX] = spr.priority;
                layerBuffers[SPRITE_LAYER].alpha[screenX] = spr.alpha();
            }
        }
    }
}

//=============================================================================
// Compositing
//=============================================================================

void VideoRenderer::clearBuffers() {
    for (auto& buf : layerBuffers) {
        buf.color.fill(0);
        buf.priority.fill(0);
        buf.alpha.fill(16);
    }
    
    finalBuffer.color.fill(0);  // Backdrop color
    finalBuffer.priority.fill(0);
    finalBuffer.alpha.fill(16);
}

void VideoRenderer::compositeBuffers(uint16_t line) {
    // Composite layers back-to-front based on priority
    // Priority: 0 = back, 15 = front
    
    for (int x = 0; x < WIDTH; ++x) {
        uint8_t topColor = 0;  // Backdrop
        uint8_t topPriority = 0;
        uint8_t topAlpha = 16;
        
        // Find highest priority visible pixel
        for (int layer = 0; layer < 6; ++layer) {
            uint8_t color = layerBuffers[layer].color[x];
            uint8_t priority = layerBuffers[layer].priority[x];
            uint8_t alpha = layerBuffers[layer].alpha[x];
            
            // Skip transparent pixels
            if (color == 0) continue;
            
            // Check priority
            if (priority >= topPriority) {
                if (alpha == 16) {
                    // Fully opaque - replace
                    topColor = color;
                    topPriority = priority;
                    topAlpha = alpha;
                }
                else if (alpha > 0) {
                    // Alpha blend with what's below
                    uint32_t fgColor = paletteRGBA[color];
                    uint32_t bgColor = paletteRGBA[topColor];
                    uint32_t blended = blendAlpha(fgColor, bgColor, alpha);
                    
                    // Convert back to palette index (approximate)
                    // For now, just use the fg color
                    topColor = color;
                    topPriority = priority;
                    topAlpha = 16;
                }
            }
        }
        
        finalBuffer.color[x] = topColor;
        finalBuffer.priority[x] = topPriority;
        finalBuffer.alpha[x] = topAlpha;
    }
    
    // Convert to RGBA and write to framebuffer
    for (int x = 0; x < WIDTH; ++x) {
        uint8_t palIndex = finalBuffer.color[x];
        framebuffer[line * WIDTH + x] = paletteRGBA[palIndex];
    }
}

//=============================================================================
// Effects
//=============================================================================

void VideoRenderer::applyEffects(uint16_t line) {
    if (!cpld2) return;
    
    // Get global effects
    uint8_t brightness = cpld2->getRegister(0x08);  // 0-31
    int8_t tintR = cpld2->getRegister(0x09);
    int8_t tintG = cpld2->getRegister(0x0A);
    int8_t tintB = cpld2->getRegister(0x0B);
    
    // Apply to framebuffer line
    for (int x = 0; x < WIDTH; ++x) {
        uint32_t color = framebuffer[line * WIDTH + x];
        
        // Apply brightness
        if (brightness != 31) {
            color = applyBrightness(color, brightness);
        }
        
        // Apply tint
        if (tintR != 0 || tintG != 0 || tintB != 0) {
            color = applyTint(color, tintR, tintG, tintB);
        }
        
        framebuffer[line * WIDTH + x] = color;
    }
}

uint32_t VideoRenderer::applyBrightness(uint32_t color, uint8_t brightness) {
    // brightness: 0=black, 31=normal
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = (color >> 0) & 0xFF;
    uint8_t a = (color >> 24) & 0xFF;
    
    r = (r * brightness) / 31;
    g = (g * brightness) / 31;
    b = (b * brightness) / 31;
    
    return (a << 24) | (r << 16) | (g << 8) | b;
}

uint32_t VideoRenderer::applyTint(uint32_t color, int8_t tintR, int8_t tintG, int8_t tintB) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = (color >> 0) & 0xFF;
    uint8_t a = (color >> 24) & 0xFF;
    
    // Clamp after adding tint
    r = std::clamp(int(r) + tintR, 0, 255);
    g = std::clamp(int(g) + tintG, 0, 255);
    b = std::clamp(int(g) + tintB, 0, 255);
    
    return (a << 24) | (r << 16) | (g << 8) | b;
}

uint32_t VideoRenderer::blendAlpha(uint32_t fg, uint32_t bg, uint8_t alpha) {
    // alpha: 0-16, where 16 = fully opaque
    uint8_t fgR = (fg >> 16) & 0xFF;
    uint8_t fgG = (fg >> 8) & 0xFF;
    uint8_t fgB = (fg >> 0) & 0xFF;
    
    uint8_t bgR = (bg >> 16) & 0xFF;
    uint8_t bgG = (bg >> 8) & 0xFF;
    uint8_t bgB = (bg >> 0) & 0xFF;
    
    uint8_t r = (fgR * alpha + bgR * (16 - alpha)) / 16;
    uint8_t g = (fgG * alpha + bgG * (16 - alpha)) / 16;
    uint8_t b = (fgB * alpha + bgB * (16 - alpha)) / 16;
    
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

//=============================================================================
// VRAM Access
//=============================================================================

uint8_t VideoRenderer::readVRAM(uint32_t flatAddr) {
    if (!vram || flatAddr >= 0x80000) return 0;
    Address addr(flatAddr >> 16, flatAddr & 0xFFFF);
    return vram->readByte(addr);
}

uint16_t VideoRenderer::readVRAM16(uint32_t addr) {
    uint8_t lo = readVRAM(addr);
    uint8_t hi = readVRAM(addr + 1);
    return lo | (hi << 8);
}

//=============================================================================
// Color Conversion
//=============================================================================

uint32_t VideoRenderer::rgb565_to_rgba8888(uint16_t rgb565) {
    uint8_t r5 = (rgb565 >> 11) & 0x1F;
    uint8_t g6 = (rgb565 >> 5) & 0x3F;
    uint8_t b5 = (rgb565 >> 0) & 0x1F;
    
    // Expand to 8-bit
    uint8_t r = (r5 << 3) | (r5 >> 2);
    uint8_t g = (g6 << 2) | (g6 >> 4);
    uint8_t b = (b5 << 3) | (b5 >> 2);
    
    return 0xFF000000 | (b << 16) | (g << 8) | r;
}
