#include "mailbox.h"
#include <iostream>
#include <algorithm>

Mailbox::Mailbox(uint32_t baseAddress, uint32_t size, const std::string& name)
    : baseAddress(baseAddress)
    , size(size)
    , name(name)
    , newDataFlag(false)
    , busyFlag(false)
    , writeCallback(nullptr)
{
    data.resize(size, 0x00);
}

uint8_t Mailbox::readByte(const Address& address) {
        uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();
    // Calculate offset from base address
    uint32_t offset = (flatAddr - baseAddress) & 0xFFFFFF;
    
    if (offset < size) {
        // Reading clears new data flag (data has been consumed)
        if (newDataFlag) {
            newDataFlag = false;
        }
        return data[offset];
    }
    
    std::cerr << "Mailbox " << name << ": Read out of bounds at offset $" 
              << std::hex << offset << std::dec << std::endl;
    return 0xFF;
}

void Mailbox::storeByte(const Address& address, uint8_t value) {
        uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();
    // Calculate offset from base address
    uint32_t offset = (flatAddr - baseAddress) & 0xFFFFFF;

    std::cout << "[MAILBOX WRITE ATTEMPT] bank=$" << std::hex << (int)address.getBank()
    << " offset=$" << address.getOffset()
    << " flat=$" << flatAddr << std::dec << std::endl;

    std::cout << "[MAILBOX DEBUG] " << name << " storeByte called! flatAddr=$"
    << std::hex << flatAddr << " value=$" << (int)value << std::dec << std::endl;
    
    if (offset < size) {
        data[offset] = value;
        
        // Writing sets new data flag
        newDataFlag = true;
        
        // Notify CPLD2 that mailbox was written
        if (writeCallback) {
            writeCallback();
        }
    } else {
        std::cerr << "Mailbox " << name << ": Write out of bounds at offset $" 
                  << std::hex << offset << std::dec << std::endl;
    }
}

bool Mailbox::decodeAddress(const Address& address, Address& decoded) {
    uint32_t flatAddr = (address.getBank() << 16) | address.getOffset();

    //std::cout << "[MAILBOX] " << name << " decodeAddress called: flatAddr=$"
    //<< std::hex << flatAddr << " baseAddress=$" << baseAddress
    //<< " size=$" << size << std::dec << std::endl;

    if (flatAddr >= baseAddress && flatAddr < baseAddress + size) {
        decoded = address;
        //std::cout << "[MAILBOX] " << name << " ACCEPTED address" << std::endl;
        return true;
    }

    //std::cout << "[MAILBOX] " << name << " REJECTED address" <<    std::endl;
    return false;

}

void Mailbox::clear() {
    std::fill(data.begin(), data.end(), 0x00);
    newDataFlag = false;
    busyFlag = false;
}
