#include "ram.h"
#include <fstream>
#include <iostream>
#include <algorithm>

RAM::RAM(uint32_t baseAddress, uint32_t size, const std::string& name)
    : baseAddress(baseAddress)
    , size(size)
    , name(name)
{
    data.resize(size, 0x00);
}

uint8_t RAM::readByte(const Address& address) {
    // Calculate offset from base address
    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();
    uint32_t offset = flatAddr - baseAddress;
    
    if (offset < size) {
        return data[offset];
    }
    
    // Out of bounds - shouldn't happen if memory bus is configured correctly
    std::cerr << "RAM " << name << ": Read out of bounds at offset $" 
              << std::hex << offset << std::dec << std::endl;
    return 0xFF;
}

void RAM::storeByte(const Address& address, uint8_t value) {
    // Calculate offset from base address
    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();
    uint32_t offset = flatAddr - baseAddress;
    
    if (offset < size) {
        data[offset] = value;
    } else {
        // Out of bounds
        std::cerr << "RAM " << name << ": Write out of bounds at offset $" 
                  << std::hex << offset << std::dec << std::endl;
    }
}

bool RAM::decodeAddress(const Address& address, Address& decoded) {
    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();
    if (flatAddr >= baseAddress && flatAddr < baseAddress + size) {
        decoded = address;
        return true;
    }
    return false;
}

bool RAM::loadFromFile(const std::string& filename, uint32_t offset) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "RAM " << name << ": Failed to open file " 
                  << filename << std::endl;
        return false;
    }
    
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (offset + fileSize > size) {
        std::cerr << "RAM " << name << ": File too large for RAM (file: " 
                  << fileSize << " bytes, available: " << (size - offset) 
                  << " bytes)" << std::endl;
        file.close();
        return false;
    }
    
    if (!file.read(reinterpret_cast<char*>(data.data() + offset), fileSize)) {
        std::cerr << "RAM " << name << ": Failed to read file " 
                  << filename << std::endl;
        file.close();
        return false;
    }
    
    file.close();
    std::cout << "RAM " << name << ": Loaded " << fileSize 
              << " bytes from " << filename << " at offset $" 
              << std::hex << offset << std::dec << std::endl;
    return true;
}

bool RAM::saveToFile(const std::string& filename) const {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "RAM " << name << ": Failed to create file " 
                  << filename << std::endl;
        return false;
    }
    
    if (!file.write(reinterpret_cast<const char*>(data.data()), size)) {
        std::cerr << "RAM " << name << ": Failed to write file " 
                  << filename << std::endl;
        file.close();
        return false;
    }
    
    file.close();
    std::cout << "RAM " << name << ": Saved " << size 
              << " bytes to " << filename << std::endl;
    return true;
}

void RAM::clear(uint8_t value) {
    std::fill(data.begin(), data.end(), value);
}
