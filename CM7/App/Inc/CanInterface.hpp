#pragma once

#include "main.h"
#include "fdcan.h"
#include "CanProtocolTypes.hpp"
#include "CClock.hpp"

class CCanInterface {
public:
    CCanInterface(FDCAN_HandleTypeDef* pHandler);

    uint32_t executeTransaction(const MoteusCanFrame* pTxFrames, uint32_t pTxCount, 
                                 uint32_t pExpectedReplies, uint32_t pTimeoutUs,
                                 MoteusCanFrame* pOutRxFrames, uint32_t pMaxRxCapacity);
    float getAvgBusTime();

private:
    static uint32_t lenToDlc(uint8_t len);
    static uint8_t dlcToLen(uint32_t dlc);

private:
    FDCAN_HandleTypeDef* mHfdcan;
    CClock mCLock;

    uint32_t mLastBusTimeUs = 0;
    float mAvgBusTimeUs = 0.0f;
    static constexpr float FILTER_ALPHA = 0.05f;
};