#ifndef CDEBUGGER_HPP
#define CDEBUGGER_HPP

#include "main.h"

class CDebugger
{
public:
    CDebugger(UART_HandleTypeDef pHuart);
    void log(const char* pMsg);
    void logLine(const char* pMsg);

private:
    UART_HandleTypeDef mHuart;
};

#endif