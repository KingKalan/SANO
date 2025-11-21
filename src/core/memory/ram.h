#ifndef RAM_H
#define RAM_H

#include "../cpu/SystemBusDevice.hpp"
#include <vector>
#include <string>

/**
 * Generic RAM module
 * Implements MemoryDevice interface for memory-mapped RAM
 */
class RAM : public SystemBusDevice {
public:
    // Constructor
    RAM(uint32_t baseAddress, uint32_t size, const std::string& name = "RAM");
    ~RAM() override = default;
    
    // SystemBusDevice interface
    uint8_t readByte(const Address& address) override;
    void storeByte(const Address& address, uint8_t value) override;
    bool decodeAddress(const Address& address, Address& decoded) override;
    
    // Direct memory access (for debugging/testing)
    uint8_t* getPointer() { return data.data(); }
    const uint8_t* getPointer() const { return data.data(); }
    
    // Load data from file
    bool loadFromFile(const std::string& filename, uint32_t offset = 0);
    
    // Save data to file
    bool saveToFile(const std::string& filename) const;
    
    // Clear all RAM
    void clear(uint8_t value = 0x00);
    
    // Get name (for debugging)
    const std::string& getName() const { return name; }
    
private:
    uint32_t baseAddress;
    uint32_t size;
    std::string name;
    std::vector<uint8_t> data;
};

#endif // RAM_H
