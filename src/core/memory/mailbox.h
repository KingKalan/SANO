#ifndef MAILBOX_H
#define MAILBOX_H

#include "../cpu/SystemBusDevice.hpp"
#include <vector>
#include <string>
#include <functional>

/**
 * Mailbox - Inter-CPU communication
 * Dual-port SRAM for message passing between CPUs
 * 
 * Mailbox A: Main CPU <-> Graphics CPU
 * Mailbox B: Main CPU <-> Sound CPU
 */
class Mailbox : public SystemBusDevice {
public:
    // Constructor
    Mailbox(uint32_t baseAddress, uint32_t size, const std::string& name = "Mailbox");
    ~Mailbox() override = default;
    
    // SystemBusDevice interface
    uint8_t readByte(const Address& address) override;
    void storeByte(const Address& address, uint8_t value) override;
    bool decodeAddress(const Address& address, Address& decoded) override;
    
    uint32_t getBaseAddress() const { return baseAddress; }
    uint32_t getSize() const { return size; }
    
    // Mailbox status flags
    bool hasNewData() const { return newDataFlag; }
    void clearNewDataFlag() { newDataFlag = false; }
    void setNewDataFlag() { newDataFlag = true; }
    
    bool isBusy() const { return busyFlag; }
    void setBusy(bool busy) { busyFlag = busy; }
    
    // Write notification callback (notifies CPLD2 when mailbox is written)
    using WriteCallback = std::function<void()>;
    void setWriteCallback(WriteCallback callback) { writeCallback = callback; }

    // Clear all mailbox data
    void clear();
    
    // Get name
    const std::string& getName() const { return name; }
    
    // Direct access (for debugging)
    uint8_t* getPointer() { return data.data(); }
    const uint8_t* getPointer() const { return data.data(); }
    
private:
    uint32_t baseAddress;
    uint32_t size;
    std::string name;
    std::vector<uint8_t> data;
    
    // Status flags
    bool newDataFlag;
    bool busyFlag;
    
    // Write notification callback
    WriteCallback writeCallback;
};

#endif // MAILBOX_H
