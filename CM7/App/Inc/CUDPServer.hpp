#ifndef CUDPSERVER_HPP
#define CUDPSERVER_HPP

#include "lwip/udp.h"
#include <string>

#include "CanProtocolTypes.hpp"

class CUDPServer {
public:
    typedef void (*LogFunc)(const char*);
public:
    CUDPServer(LogFunc pLogFun);
    static void updateServer();
    
    bool begin(uint16_t port);
    bool getIpAddress(char* pOutBuffer);

    bool hasPendingTransaction() const { return mPendingTransaction; }
    const UdpRequestHeader& getActiveHeader() const { return mActiveHeader; }
    const MoteusCanFrame* getTxFrames() const { return mTxFrames; }
    bool sendResponse(const MoteusCanFrame* pResponseFrames, uint32_t pCount);

private:
    static void _static_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, 
                                     const ip_addr_t *addr, u16_t port);

    void handlePacket(struct pbuf *p, const ip_addr_t *addr, uint16_t port);

private:
    void* mContextNode;
    struct udp_pcb* mUDPPCB;

    bool mPendingTransaction;
    UdpRequestHeader mActiveHeader;
    static constexpr uint32_t MAX_FRAMES_PER_TRANSACTION = 32;
    MoteusCanFrame mTxFrames[MAX_FRAMES_PER_TRANSACTION];

    ip_addr_t mPeerAddr;
    uint16_t mPeerPort;
    LogFunc logger;
};

#endif