#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include <memory>
#include "../cpu/SystemBusDevice.hpp"
/**
 * Cartridge
 * 
 * Handles SANo cartridge ROM and optional save RAM:
 * - ROM: Up to 64MB (16 banks × 4MB)
 * - Banking: Bank register at $420000
 * - ROM window: $C00000-$FFFFFF (4MB, bank-switched)
 * - Save RAM: Optional 64KB at $700000-$70FFFF
 * 
 * ROM Header Structure (at $000000):
 * - 256 bytes with resource pointers
 * - Reset vector at $00FFFC
 */
class Cartridge : public SystemBusDevice {
public:
    Cartridge();
    ~Cartridge();
    
    // ROM loading
    bool loadROM(const std::string& filename);
    bool loadROM(const uint8_t* data, size_t size);
    void unload();
    
    // Memory access
    uint8_t readByte(const Address& address) override;
    void storeByte(const Address& address, uint8_t value) override;
    bool decodeAddress(const Address& address, Address& decoded) override;
    
    // Banking
    void setBank(uint8_t bank);
    uint8_t getCurrentBank() const { return currentBank; }
    
    // Save RAM
    bool hasSaveRAM() const { return saveRAM.size() > 0; }
    bool loadSaveRAM(const std::string& filename);
    bool saveSaveRAM(const std::string& filename);
    void createSaveRAM();  // Initialize empty 64KB save RAM
    
    // ROM info
    bool isLoaded() const { return rom.size() > 0; }
    size_t getROMSize() const { return rom.size(); }
    int getBankCount() const;
    
    // Header parsing
    struct ROMHeader {
        uint32_t mainCPU_entryPoint;        // 24-bit entry point for Main CPU
        uint32_t graphicsCPU_entryPoint;    // 24-bit entry point for Graphics CPU
        uint32_t soundCPU_entryPoint;       // 24-bit entry point for Sound CPU
        uint32_t paletteData;       // Pointer to palette data
        uint32_t tileData;          // Pointer to tile data
        uint32_t audioData;         // Pointer to audio data
        char title[32];             // Game title (null-terminated)
        uint8_t version;            // ROM version
        uint8_t reserved[185];      // Reserved for future use
        
        bool isValid() const;
    };
    
    const ROMHeader& getHeader() const { return header; }
    
    // Memory map constants
    static constexpr uint32_t ROM_WINDOW_START = 0xC00000;
    static constexpr uint32_t ROM_WINDOW_END = 0xFFFFFF;
    static constexpr uint32_t ROM_WINDOW_SIZE = 0x400000;  // 4MB
    
    static constexpr uint32_t BANK_REGISTER = 0x420000;
    
    static constexpr uint32_t SAVE_RAM_START = 0x700000;
    static constexpr uint32_t SAVE_RAM_END = 0x70FFFF;
    static constexpr uint32_t SAVE_RAM_SIZE = 0x10000;  // 64KB
    
    static constexpr int MAX_BANKS = 16;
    static constexpr uint32_t BANK_SIZE = 0x400000;  // 4MB per bank
    
private:
    // ROM data
    std::vector<uint8_t> rom;
    
    // Save RAM (optional)
    std::vector<uint8_t> saveRAM;
    
    // Current bank
    uint8_t currentBank;
    
    // ROM header
    ROMHeader header;
    
    // Helper functions
    void parseHeader();
    uint32_t mapAddress(uint32_t address);
    bool addressInROMWindow(uint32_t address) const;
    bool addressInSaveRAM(uint32_t address) const;
};

#endif // CARTRIDGE_H
