#include "Bridge.hpp"
#include "main.h"

#include "stdio.h"
#include <cstring>

#include "CInterCoreLink.hpp"
#include "CUDPServer.hpp"
#include "CanInterface.hpp"

extern FDCAN_HandleTypeDef hfdcan1;

void logger_wrapper(const char* pMessage) 
{
    CInterCoreLink& link = CInterCoreLink::getInstance();
    
    CInterCoreLink::LogMessage msg = {};

    std::strncpy(msg.text, pMessage, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';
    
    link.mSharedMemory.log_msg.push(msg); 
}

extern "C" void init_cpp()
{
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_12, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_13, GPIO_PIN_RESET);
}

extern "C" void main_cpp()
{
    // Call master init for link once from cm7
    CInterCoreLink& dataLink = CInterCoreLink::getInstance();
    CUDPServer udpServer(logger_wrapper);
    CCanInterface canInterface(&hfdcan1, logger_wrapper);

    udpServer.begin(6666);
    CInterCoreLink::IpAddr ipBuffer;
    while(true) {
        udpServer.updateServer();
        if (udpServer.getIpAddress(ipBuffer.ipAddress)) {
            dataLink.mSharedMemory.ip_addr.push(ipBuffer); 
            
            char logBuf[60];
            snprintf(logBuf, sizeof(logBuf), "Netif Up! IP: %s", ipBuffer.ipAddress);
            logger_wrapper(logBuf);
            
            break;
        }
        HAL_Delay(100);
    }
    
    while(true) {
        if (udpServer.hasPendingTransaction()) {
            
            const UdpRequestHeader& header = udpServer.getActiveHeader();
            const MoteusCanFrame* txFrames = udpServer.getTxFrames();

            MoteusCanFrame rxResponses[12]; 

            uint32_t repliesFound = canInterface.executeTransaction(
                txFrames, 
                header.frameCount, 
                header.expectedReplies, 
                header.timeoutUs,
                rxResponses, 
                12
            );

            udpServer.sendResponse(rxResponses, repliesFound);
            dataLink.mSharedMemory.avgBusTimeUs.store(canInterface.getAvgBusTime());
            
        }
        CUDPServer::updateServer();
    }
}