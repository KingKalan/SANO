#include "cartridge.h"
#include <fstream>
#include <cstring>
#include <iostream>

Cartridge::Cartridge()
    : currentBank(0)
{
    std::memset(&header, 0, sizeof(header));
}

Cartridge::~Cartridge() {
}

//=============================================================================
// ROM Loading
//=============================================================================

bool Cartridge::loadROM(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Cartridge: Failed to open ROM file: " << filename << std::endl;
        return false;
    }
    
    // Get file size
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Check size limits
    if (fileSize == 0) {
        std::cerr << "Cartridge: ROM file is empty" << std::endl;
        return false;
    }
    
    if (fileSize > BANK_SIZE * MAX_BANKS) {
        std::cerr << "Cartridge: ROM file too large (max 64MB)" << std::endl;
        return false;
    }
    
    // Allocate and read ROM
    rom.resize(fileSize);
    if (!file.read(reinterpret_cast<char*>(rom.data()), fileSize)) {
        std::cerr << "Cartridge: Failed to read ROM data" << std::endl;
        rom.clear();
        return false;
    }
    
    std::cout << "Cartridge: Loaded ROM (" << fileSize << " bytes, " 
              << getBankCount() << " banks)" << std::endl;
    
    // Parse header
    parseHeader();
    
    // Reset to bank 0
    currentBank = 0;
    
    return true;
}

bool Cartridge::loadROM(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        std::cerr << "Cartridge: Invalid ROM data" << std::endl;
        return false;
    }
    
    if (size > BANK_SIZE * MAX_BANKS) {
        std::cerr << "Cartridge: ROM data too large (max 64MB)" << std::endl;
        return false;
    }
    
    rom.assign(data, data + size);
    
    std::cout << "Cartridge: Loaded ROM from memory (" << size << " bytes)" << std::endl;
    
    parseHeader();
    currentBank = 0;
    
    return true;
}

void Cartridge::unload() {
    rom.clear();
    saveRAM.clear();
    currentBank = 0;
    std::memset(&header, 0, sizeof(header));
}

//=============================================================================
// Header Parsing
//=============================================================================

void Cartridge::parseHeader() {
    if (rom.size() < 256) {
        std::cerr << "Cartridge: ROM too small for header" << std::endl;
        return;
    }
    
    // Parse header structure (first 256 bytes)
    // Format: [entryPoint:3][paletteData:3][tileData:3][audioData:3][title:32][version:1][reserved:191]
    
    header.mainCPU_entryPoint = rom[0] | (rom[1] << 8) | (rom[2] << 16);
    header.graphicsCPU_entryPoint = rom[3] | (rom[4] <<8) | (rom[5] <<16);
    header.soundCPU_entryPoint = rom[6] | (rom[7] << 8) | (rom[8] << 16);
    header.paletteData = rom[9] | (rom[10] << 8) | (rom[11] << 16);
    header.tileData = rom[12] | (rom[13] << 8) | (rom[14] << 16);
    header.audioData = rom[15] | (rom[16] << 8) | (rom[17] << 16);
    
    // Copy title (ensure null-termination)
    std::memcpy(header.title, &rom[12], 31);
    header.title[31] = '\0';
    
    header.version = rom[50];
    
    std::cout << "Cartridge Header:" << std::endl;
    std::cout << "  Title: " << header.title << std::endl;
    std::cout << "  Main CPU Entry: $" << std::hex << header.mainCPU_entryPoint << std::dec << std::endl;
    std::cout << "  Graphics CPU Entry: $" << std::hex << header.graphicsCPU_entryPoint << std::dec << std::endl;
    std::cout << "  Sound CPU Entry; $" << std::hex << header.soundCPU_entryPoint << std::dec << std::endl;
    std::cout << "  Version: " << static_cast<int>(header.version) << std::endl;
}

bool Cartridge::ROMHeader::isValid() const {
    // Basic validation: entry point should be in ROM range
    return mainCPU_entryPoint >= ROM_WINDOW_START && mainCPU_entryPoint <= ROM_WINDOW_END;
    return graphicsCPU_entryPoint >= ROM_WINDOW_START && graphicsCPU_entryPoint <= ROM_WINDOW_END;
    return soundCPU_entryPoint >= ROM_WINDOW_START && soundCPU_entryPoint <= ROM_WINDOW_END;
}

//=============================================================================
// Memory Access
//=============================================================================

uint8_t Cartridge::readByte(const Address& address) {
    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();

    // Check reset vector area: $00FFFC-$00FFFF
    if (flatAddr >= 0x00FFFC && flatAddr <= 0x00FFFF) {
        uint32_t romAddr = flatAddr;
        if (romAddr < rom.size()) {
            uint8_t value = rom[romAddr];
            std::cout << "Reading reset vector $" << std::hex << flatAddr << " = $" << (int)value << std::dec << std::endl;
            return value;
        }
        return 0xFF;

    }

    if (flatAddr >= 0x008000 && flatAddr <= 0x00FFFF) {
        uint32_t offset = flatAddr;  // Offset into ROM
        if (offset < rom.size()) {
            return rom[offset];
            // Debug vector reads
            if (flatAddr >= 0x00FFE4 && flatAddr <= 0x00FFFF) {
                std::cout << "[ROM MIRROR] Read vector at $" << std::hex << flatAddr
                << " = $" << (int)rom[offset] << std::dec << std::endl;
            }
            return rom[offset];
        }
        return 0xFF;
    }

    if (addressInROMWindow(flatAddr)) {
        uint32_t romAddr = mapAddress(flatAddr);
        if (romAddr < rom.size()) {
            return rom[romAddr];
        }
        return 0xFF;
    }
    
    // Check save RAM
    if (addressInSaveRAM(flatAddr)) {
        uint32_t offset = flatAddr - SAVE_RAM_START;
        if (offset < saveRAM.size()) {
            return saveRAM[offset];
        }
        return 0xFF;
    }
    
    return 0xFF;  // Open bus
}

void Cartridge::storeByte(const Address& address, uint8_t value) {
    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();
    // Bank register
    if (flatAddr == BANK_REGISTER) {
        setBank(value & 0x0F);  // 4-bit bank number
        return;
    }
    
    // Save RAM (writable)
    if (addressInSaveRAM(flatAddr)) {
        uint32_t offset = flatAddr - SAVE_RAM_START;
        if (offset < saveRAM.size()) {
            saveRAM[offset] = value;
        }
        return;
    }
    
    // ROM is read-only, ignore writes
}

bool Cartridge::decodeAddress(const Address& address, Address& decoded) {
    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();

    // Check reset vector area: $00FFFC-$00FFFF
    if (flatAddr >= 0x00FFFC && flatAddr <= 0x00FFFF) {
        decoded = address;
        return true;
    }

    // ADD THIS - Bank 0 ROM mirror: $008000-$00FFFF
    if (flatAddr >= 0x008000 && flatAddr <= 0x00FFFF) {
        decoded = address;
        return true;
    }

    // Check ROM window: $C00000-$FFFFFF
    if (addressInROMWindow(flatAddr)) {
        decoded = address;
        return true;
    }

    // Check bank register: $420000
    if (flatAddr == BANK_REGISTER) {
        decoded = address;
        return true;
    }

    // Check save RAM: $700000-$70FFFF
    if (addressInSaveRAM(flatAddr)) {
        decoded = address;
        return true;
    }

    return false;
}


//=============================================================================
// Banking
//=============================================================================

void Cartridge::setBank(uint8_t bank) {
    if (bank >= MAX_BANKS) {
        bank = 0;
    }
    currentBank = bank;
}

int Cartridge::getBankCount() const {
    if (rom.empty()) return 0;
    return (rom.size() + BANK_SIZE - 1) / BANK_SIZE;
}

//=============================================================================
// Address Mapping
//=============================================================================

uint32_t Cartridge::mapAddress(uint32_t address) {
    // Map CPU address in ROM window to physical ROM address
    // ROM window: $C00000-$FFFFFF (4MB)
    // Physical ROM address = (currentBank × 4MB) + offset
    
    uint32_t offset = address - ROM_WINDOW_START;
    uint32_t romAddr = (currentBank * BANK_SIZE) + offset;
    
    return romAddr;
}

bool Cartridge::addressInROMWindow(uint32_t address) const {
    return address >= ROM_WINDOW_START && address <= ROM_WINDOW_END;
}

bool Cartridge::addressInSaveRAM(uint32_t address) const {
    return address >= SAVE_RAM_START && address <= SAVE_RAM_END;
}

//=============================================================================
// Save RAM
//=============================================================================

void Cartridge::createSaveRAM() {
    if (saveRAM.empty()) {
        saveRAM.resize(SAVE_RAM_SIZE, 0xFF);
        std::cout << "Cartridge: Created 64KB save RAM" << std::endl;
    }
}

bool Cartridge::loadSaveRAM(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cartridge: Failed to open save file: " << filename << std::endl;
        return false;
    }
    
    // Create save RAM if it doesn't exist
    if (saveRAM.empty()) {
        createSaveRAM();
    }
    
    // Read save data
    file.read(reinterpret_cast<char*>(saveRAM.data()), SAVE_RAM_SIZE);
    size_t bytesRead = file.gcount();
    
    std::cout << "Cartridge: Loaded save RAM (" << bytesRead << " bytes)" << std::endl;
    
    return true;
}

bool Cartridge::saveSaveRAM(const std::string& filename) {
    if (saveRAM.empty()) {
        std::cerr << "Cartridge: No save RAM to save" << std::endl;
        return false;
    }
    
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cartridge: Failed to create save file: " << filename << std::endl;
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(saveRAM.data()), saveRAM.size());
    
    std::cout << "Cartridge: Saved save RAM (" << saveRAM.size() << " bytes)" << std::endl;
    
    return true;
}
