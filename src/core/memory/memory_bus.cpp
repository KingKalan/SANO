#include "memory_bus.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

MemoryBus::MemoryBus() {
    // Empty - devices mapped later
}

MemoryBus::~MemoryBus() {
    unmapAll();
}

uint8_t MemoryBus::read(uint32_t address) {
    // Mask to 24-bit address space (65C816 limit)
    address &= 0xFFFFFF;
    
    MemoryDevice* device = findDevice(address);
    if (device) {
        return device->read(address);
    }
    
    // Open bus - return 0xFF (common behavior)
    return 0xFF;
}

void MemoryBus::write(uint32_t address, uint8_t value) {
    // Mask to 24-bit address space
    address &= 0xFFFFFF;
    
    MemoryDevice* device = findDevice(address);
    if (device) {
        device->write(address, value);
    }
    // Writes to unmapped space are ignored
}

uint16_t MemoryBus::read16(uint32_t address) {
    // Little-endian: low byte first, high byte second
    uint8_t low = read(address);
    uint8_t high = read(address + 1);
    return (high << 8) | low;
}

void MemoryBus::write16(uint32_t address, uint16_t value) {
    // Little-endian: low byte first, high byte second
    write(address, value & 0xFF);
    write(address + 1, (value >> 8) & 0xFF);
}

void MemoryBus::mapDevice(MemoryDevice* device, uint32_t baseAddress, uint32_t size) {
    if (!device || size == 0) {
        return;
    }
    
    MappedRegion region;
    region.baseAddress = baseAddress & 0xFFFFFF;
    region.endAddress = (baseAddress + size - 1) & 0xFFFFFF;
    region.device = device;
    
    // Check for overlapping regions (warn but allow)
    for (const auto& existing : mappedRegions) {
        if (!(region.endAddress < existing.baseAddress || 
              region.baseAddress > existing.endAddress)) {
            std::cerr << "Warning: Memory region overlap detected at $" 
                      << std::hex << std::setw(6) << std::setfill('0') 
                      << baseAddress << std::dec << std::endl;
        }
    }
    
    mappedRegions.push_back(region);
    
    // Sort by base address for faster lookup
    std::sort(mappedRegions.begin(), mappedRegions.end(),
              [](const MappedRegion& a, const MappedRegion& b) {
                  return a.baseAddress < b.baseAddress;
              });
}

void MemoryBus::unmapAll() {
    mappedRegions.clear();
}

MemoryDevice* MemoryBus::findDevice(uint32_t address) {
    // Binary search for efficiency
    for (const auto& region : mappedRegions) {
        if (address >= region.baseAddress && address <= region.endAddress) {
            return region.device;
        }
        // Early exit if we've passed the address
        if (address < region.baseAddress) {
            break;
        }
    }
    return nullptr;
}

void MemoryBus::dumpMemory(uint32_t start, uint32_t length) {
    std::cout << "Memory dump from $" << std::hex << std::setw(6) 
              << std::setfill('0') << start << ":" << std::endl;
    
    for (uint32_t i = 0; i < length; i++) {
        if (i % 16 == 0) {
            std::cout << std::hex << std::setw(6) << std::setfill('0') 
                      << (start + i) << ": ";
        }
        
        uint8_t byte = read(start + i);
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(byte) << " ";
        
        if ((i + 1) % 16 == 0 || i == length - 1) {
            std::cout << std::endl;
        }
    }
    std::cout << std::dec;
}
