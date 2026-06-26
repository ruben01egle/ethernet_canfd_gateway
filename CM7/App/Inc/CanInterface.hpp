#pragma once

#include "main.h"
#include "fdcan.h"
#include "CanProtocolTypes.hpp"
#include "CClock.hpp"

class CCanInterface {
public:
    typedef void (*LogFunc)(const char*);
public:
    CCanInterface(FDCAN_HandleTypeDef* pHandler, LogFunc pLogFun);

    bool init();
    uint32_t executeTransaction(const MoteusCanFrame* pTxFrames, uint32_t pTxCount, 
                                 uint32_t pExpectedReplies, uint32_t pTimeoutUs,
                                 MoteusCanFrame* pOutRxFrames, uint32_t pMaxRxCapacity);
    float getAvgBusTime();

private:
    uint32_t lenToDlc(uint8_t len);
    uint8_t dlcToLen(uint32_t dlc);
    void recover_bus_error();
    void make_frame(const MoteusCanFrame& src, FDCAN_TxHeaderTypeDef& txHeader, uint8_t* txData);

private:
    FDCAN_HandleTypeDef* mHfdcan;
    CClock mCLock;
    LogFunc logger;

    uint32_t mLastBusTimeUs = 0;
    float mAvgBusTimeUs = 0.0f;
    static constexpr float FILTER_ALPHA = 0.05f;
};