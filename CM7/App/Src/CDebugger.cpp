#include "CDebugger.hpp"

#include <cstring>

CDebugger::CDebugger(UART_HandleTypeDef pHuart): mHuart(pHuart)
{
}

void CDebugger::log(const char* pMsg)
{
    HAL_UART_Transmit(&mHuart, (uint8_t*)pMsg, strlen(pMsg), 100);
}

void CDebugger::logLine(const char* pMsg)
{
    log(pMsg);
    log("\r\n");
}