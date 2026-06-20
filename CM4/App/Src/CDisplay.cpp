#include "CDisplay.hpp"

#include "stdio.h"
#include <cstring>

CDisplay::CDisplay()
{
    // Die StatusBar dient nun als Container für die oberen Widgets
    mStatusBar.xpos = 0;
    mStatusBar.ypos = 0;
    mStatusBar.width = 800;
    mStatusBar.height = 100;
    mStatusBar.backcolor = UTIL_LCD_COLOR_DARKGRAY;
    mStatusBar.textcolor = UTIL_LCD_COLOR_WHITE;
    mStatusBar.font = &Font24;

    mConsole.xpos = 0;
    mConsole.ypos = 101;
    mConsole.width = 800;
    mConsole.height = 379;
    mConsole.backcolor = UTIL_LCD_COLOR_BLACK;
    mConsole.textcolor = UTIL_LCD_COLOR_GREEN;
    mConsole.font = &Font16;

    mBufferFull = 0;
    strncpy(mLogBuffer[0], "logs will appear here", LINE_LENGTH-1);
    mLogBuffer[0][LINE_LENGTH-1] = '\0';
    mHead = 1;

    init();
}

CDisplay &CDisplay::getInstance()
{
    static CDisplay instance;
    return instance;
}

int32_t CDisplay::init()
{
    int32_t ret = BSP_LCD_Init(0, LCD_ORIENTATION_LANDSCAPE);
    if (ret != BSP_ERROR_NONE) {
        return ret;
    }

    UTIL_LCD_SetFuncDriver(&LCD_Driver);
    UTIL_LCD_SetLayer(0);

    UTIL_LCD_Clear(UTIL_LCD_COLOR_BLACK);
    
    UTIL_LCD_FillRect(mStatusBar.xpos, mStatusBar.ypos,
                      mStatusBar.width, mStatusBar.height,
                      mStatusBar.backcolor);

    UTIL_LCD_SetBackColor(UTIL_LCD_COLOR_WHITE);
    UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_BLUE);
    UTIL_LCD_SetFont(&Font24);
    drawConsoleWidget();
    return BSP_ERROR_NONE;
}

void CDisplay::logStatus(const char* pStatusMsg)
{
    UTIL_LCD_SetBackColor(mStatusBar.backcolor);
    UTIL_LCD_SetTextColor(mStatusBar.textcolor);
    UTIL_LCD_SetFont(mStatusBar.font);

    UTIL_LCD_FillRect(mStatusBar.xpos, mStatusBar.ypos,
                      450, mStatusBar.height,
                      mStatusBar.backcolor);

    UTIL_LCD_DisplayStringAt(mStatusBar.xpos + 5, mStatusBar.ypos + 40, (uint8_t*)pStatusMsg, LEFT_MODE);
}

void CDisplay::logIP(const char* pIpAddr)
{
    UTIL_LCD_SetBackColor(mStatusBar.backcolor);
    UTIL_LCD_SetTextColor(mStatusBar.textcolor);
    UTIL_LCD_SetFont(mStatusBar.font);

    UTIL_LCD_FillRect(mStatusBar.xpos + 450, mStatusBar.ypos,
                      350, mStatusBar.height/2,
                      mStatusBar.backcolor);

    char ipBuffer[32];
    snprintf(ipBuffer, sizeof(ipBuffer), "IP: %s", pIpAddr);
    
    UTIL_LCD_DisplayStringAt(mStatusBar.xpos + 450 + 20, mStatusBar.ypos + 20, (uint8_t*)ipBuffer, LEFT_MODE);
}

void CDisplay::logBusTime(uint32_t busTimeUs)
{
    UTIL_LCD_SetBackColor(mStatusBar.backcolor);
    UTIL_LCD_SetTextColor(mStatusBar.textcolor);
    UTIL_LCD_SetFont(mStatusBar.font);

    UTIL_LCD_FillRect(mStatusBar.xpos + 450, mStatusBar.ypos + 50,
                      350, 50,
                      mStatusBar.backcolor);

    char timeBuffer[32];
    snprintf(timeBuffer, sizeof(timeBuffer), "Bus: %lu us", busTimeUs);
    
    UTIL_LCD_DisplayStringAt(mStatusBar.xpos + 450 + 20, mStatusBar.ypos + 60, (uint8_t*)timeBuffer, LEFT_MODE);
}

void CDisplay::drawConsoleWidget()
{
    UTIL_LCD_SetBackColor(mConsole.backcolor);
    UTIL_LCD_SetTextColor(mConsole.textcolor);
    UTIL_LCD_SetFont(mConsole.font);

    UTIL_LCD_FillRect(mConsole.xpos, mConsole.ypos,
                      mConsole.width, mConsole.height,
                      mConsole.backcolor);

    uint32_t lines2draw = mBufferFull ? MAX_LOG_LINES : mHead;
    for (uint32_t i = 0; i<lines2draw; i++) {
        uint32_t idx;
        if (mBufferFull) {
            idx = (mHead + i) % MAX_LOG_LINES;
        }
        else {
            idx = i;
        }
        UTIL_LCD_DisplayStringAt(mConsole.xpos + 0, (mConsole.ypos + (20*i)), (uint8_t*)mLogBuffer[idx], LEFT_MODE);
    }
}

void CDisplay::logLine(const char *pMsg)
{
    strncpy(mLogBuffer[mHead], pMsg, LINE_LENGTH-1);
    mLogBuffer[mHead][LINE_LENGTH-1] = '\0';
    mHead = (mHead + 1) % MAX_LOG_LINES;
    if (mHead == 0) {
        mBufferFull = true;
    }
    drawConsoleWidget();
}

void CDisplay::LOG(const char *msg)
{
    getInstance().logLine(msg);
}