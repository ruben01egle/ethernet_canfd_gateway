#ifndef CDISPLAY_HPP
#define CDISPLAY_HPP

#include "main.h"
#include "stm32h747i_discovery_lcd.h"
#include "stm32_lcd.h"

class CDisplay
{
public:
    struct widget {
        uint32_t xpos;
        uint32_t ypos;
        uint32_t width;
        uint32_t height;
        uint32_t backcolor;
        uint32_t textcolor;
        sFONT* font;
    };
public:
    static CDisplay& getInstance();
    CDisplay(const CDisplay&) = delete;
    void operator=(const CDisplay&) = delete;

public:
    int32_t init();
    void logStatus(const char* pStatusMsg);
    void logIP(const char* pIpAddr);
    void logBusTime(uint32_t busTimeUs);
    void drawConsoleWidget();
    void logLine(const char* pMsg);
    static void LOG(const char* msg);

private:
    CDisplay();

private:
    widget mStatusBar;
    widget mConsole;

    static constexpr uint32_t MAX_LOG_LINES = 18;
    static constexpr uint32_t LINE_LENGTH = 60;
    char mLogBuffer[MAX_LOG_LINES][LINE_LENGTH]; 
    uint32_t mHead;
    bool mBufferFull;
};

#endif