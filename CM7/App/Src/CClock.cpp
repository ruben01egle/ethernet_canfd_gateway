#include "CClock.hpp"

#include "main.h"

CClock::CClock()
{
    reset();
}

void CClock::reset()
{
    mTotalMicroseconds = 0;
    mLastTicks = DWT->CYCCNT;
}

uint32_t CClock::getUS() {
    uint32_t currentTicks = DWT->CYCCNT;
    
    uint32_t diffTicks = currentTicks - mLastTicks;
    
    uint32_t diffUS = diffTicks / (SystemCoreClock / 1000000);
    
    if (diffUS > 0) {
        mTotalMicroseconds += diffUS;
        
        mLastTicks += diffUS * (SystemCoreClock / 1000000);
    }
    
    return mTotalMicroseconds;
}

float CClock::getMS()
{
    getUS();
    return static_cast<float>(mTotalMicroseconds) / 1000;
}
