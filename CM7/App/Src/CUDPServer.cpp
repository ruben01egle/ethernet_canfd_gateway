#include "CUDPServer.hpp"

#include "lwip.h"
#include <cstring>


CUDPServer::CUDPServer(LogFunc pLogFun) : mUDPPCB(nullptr)
{
    logger = pLogFun;
    mPendingTransaction = false;
    std::memset(&mPeerAddr, 0, sizeof(ip_addr_t));
    std::memset(&mActiveHeader, 0, sizeof(UdpRequestHeader));
}

bool CUDPServer::begin(uint16_t port)
{
    mUDPPCB = udp_new();
    if (!mUDPPCB) {
        logger("Failed to init udp layer interface");
        return false;
    }

    udp_recv(mUDPPCB, _static_recv_callback, this);

    if (udp_bind(mUDPPCB, IP_ADDR_ANY, port) != ERR_OK) {
        udp_remove(mUDPPCB);
        logger("Failed to bind udp socket");
        return false;
    }
    logger("UDP initialised succesfully");
    return true;
}

bool CUDPServer::getIpAddress(char* pOutBuffer)
{
    extern struct netif gnetif;
    if (netif_is_up(&gnetif) && netif_is_link_up(&gnetif)) {
        sprintf(pOutBuffer, "%d.%d.%d.%d", 
                ip4_addr1_16(netif_ip4_addr(&gnetif)),
                ip4_addr2_16(netif_ip4_addr(&gnetif)),
                ip4_addr3_16(netif_ip4_addr(&gnetif)),
                ip4_addr4_16(netif_ip4_addr(&gnetif)));
        return true;
    }
    return false;
}


void CUDPServer::updateServer()
{
    extern struct netif gnetif;
    ethernetif_input(&gnetif);
    sys_check_timeouts();
}

bool CUDPServer::sendResponse(const MoteusCanFrame* pResponseFrames, uint32_t pCount) {
    if (!mPendingTransaction || !mUDPPCB) return false;

    uint32_t responseSize = pCount * sizeof(MoteusCanFrame);

    struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, responseSize, PBUF_RAM);
    if (!p) return false;

    if (responseSize > 0) {
        std::memcpy(p->payload, pResponseFrames, responseSize);
    }

    err_t err = udp_sendto(mUDPPCB, p, &mPeerAddr, mPeerPort);
    pbuf_free(p);
    mPendingTransaction = false;

    if (err != ERR_OK) {
        char errorBuf[64];
        snprintf(errorBuf, sizeof(errorBuf), "UDP Send Error: %d", (int)err);
        logger(errorBuf);
    }

    return (err == ERR_OK);
}

void CUDPServer::_static_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    CUDPServer* server = static_cast<CUDPServer*>(arg);
    server->handlePacket(p, addr, port);
}

void CUDPServer::handlePacket(struct pbuf *p, const ip_addr_t *addr, uint16_t port) {
    if (mPendingTransaction) {
        if (logger) logger("UDP: Dropped packet, previous transaction still pending!");
        pbuf_free(p);
        return;
    }

    if (p->tot_len < sizeof(UdpRequestHeader)) {
        if (logger) logger("UDP: Packet too short (missing header).");
        pbuf_free(p);
        return;
    }

    pbuf_copy_partial(p, &mActiveHeader, sizeof(UdpRequestHeader), 0);

    if (mActiveHeader.frameCount > MAX_FRAMES_PER_TRANSACTION) {
        if (logger) logger("UDP: Too many frames in request!");
        pbuf_free(p);
        return;
    }

    uint32_t expectedSize = sizeof(UdpRequestHeader) + (mActiveHeader.frameCount * sizeof(MoteusCanFrame));
    if (p->tot_len < expectedSize) {
        if (logger) logger("UDP: Packet size mismatch with frameCount.");
        pbuf_free(p);
        return;
    }

    pbuf_copy_partial(p, mTxFrames, mActiveHeader.frameCount * sizeof(MoteusCanFrame), sizeof(UdpRequestHeader));

    mPeerAddr = *addr;
    mPeerPort = port;
    
    mPendingTransaction = true;

    pbuf_free(p);
}