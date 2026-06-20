#pragma once

#include <stdint.h>

class CClock
{
public:
    CClock();
    void reset();
    // either function needs to be called more than every 10s to be accurate
    uint32_t getUS();
    float getMS();

private:
    uint32_t mTotalMicroseconds;
    uint32_t mLastTicks;
};