#ifndef MEMORY_BUS_H
#define MEMORY_BUS_H

#include <cstdint>
#include <vector>
#include <memory>

/**
 * Memory-mapped device interface
 * All devices (RAM, CPLD registers, cartridge) implement this
 */
class MemoryDevice {
public:
    virtual ~MemoryDevice() = default;
    
    virtual uint8_t read(uint32_t address) = 0;
    virtual void write(uint32_t address, uint8_t value) = 0;
    
    virtual uint32_t getBaseAddress() const = 0;
    virtual uint32_t getSize() const = 0;
};

/**
 * Memory Bus
 * Maps address space to memory devices
 * Each CPU has its own memory bus
 */
class MemoryBus {
public:
    MemoryBus();
    ~MemoryBus();
    
    // Read/Write operations
    uint8_t read(uint32_t address);
    void write(uint32_t address, uint8_t value);
    
    // 16-bit operations (little-endian)
    uint16_t read16(uint32_t address);
    void write16(uint32_t address, uint16_t value);
    
    // Map a device to address range
    void mapDevice(MemoryDevice* device, uint32_t baseAddress, uint32_t size);
    
    // Unmount all devices (cleanup)
    void unmapAll();
    
    // Debug: dump memory region
    void dumpMemory(uint32_t start, uint32_t length);
    
private:
    struct MappedRegion {
        uint32_t baseAddress;
        uint32_t endAddress;
        MemoryDevice* device;
    };
    
    std::vector<MappedRegion> mappedRegions;
    
    // Find device for address
    MemoryDevice* findDevice(uint32_t address);
};

#endif // MEMORY_BUS_H
