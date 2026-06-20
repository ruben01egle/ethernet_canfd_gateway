#include "Bridge.hpp"
#include "main.h"
#include <string>

#include "CDisplay.hpp"
#include "CInterCoreLink.hpp"


extern "C" void init_cpp()
{
    CInterCoreLink& dataLink = CInterCoreLink::getInstance();
    
    while (!dataLink.isInitalized()) {
        HAL_Delay(100);
    }
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_14, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_15, GPIO_PIN_SET);
    CDisplay& display = CDisplay::getInstance();
    display.init();
    display.drawConsoleWidget();
}

extern "C" void main_cpp()
{
    CInterCoreLink& dataLink = CInterCoreLink::getInstance();
    CDisplay& display = CDisplay::getInstance();
    display.logStatus("Up and running");

    CInterCoreLink::IpAddr recieved_ipaddr;
    CInterCoreLink::LogMessage received_msg;
    uint32_t recieved_bustime;

    while (true) {
        if (dataLink.mSharedMemory.ip_addr.pop(recieved_ipaddr)) {
            display.logIP(recieved_ipaddr.ipAddress);
        }
        if (dataLink.mSharedMemory.log_msg.pop(received_msg)) {
            display.logLine(received_msg.text);
        }
        recieved_bustime = dataLink.mSharedMemory.avgBusTimeUs.load();
        display.logBusTime(recieved_bustime);
        
        HAL_Delay(100);
    }
}