#ifndef CINTERCORELINK_HPP
#define CINTERCORELINK_HPP

#include <atomic>
#include <stdint.h>

#include "CRingBuffer.hpp"

constexpr uint32_t SRAM4_START = 0x38000000;
constexpr uint32_t SRAM4_SIZE = 64 * 1024;

class CInterCoreLink
{
public:
    struct LogMessage {
        char text[60];
    };
    struct IpAddr {
        char ipAddress[16];
    };
    struct sharedMemory{
        std::atomic<uint32_t> magic_byte;
        CRingBuffer<LogMessage, 32> log_msg;
        CRingBuffer<IpAddr, 1> ip_addr;
        std::atomic<uint32_t> avgBusTimeUs;
    };
    static_assert(sizeof(sharedMemory) <= SRAM4_SIZE, "SRAM4 overflow");

public:
    static CInterCoreLink& getInstance();
    CInterCoreLink(const CInterCoreLink&) = delete;
    void operator=(const CInterCoreLink&) = delete;

private:
    CInterCoreLink(bool pMasterInit);

public:
    bool isInitalized();
    void reset();

public:
    sharedMemory& mSharedMemory;

private:
    static constexpr uint32_t INIT_MAGIC = 0x55AAEEFF;
};

#endif