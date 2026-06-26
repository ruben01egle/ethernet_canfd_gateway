#include "CanInterface.hpp"
#include <cstring>
#include <inttypes.h>
#include <cstdio>

CCanInterface::CCanInterface(FDCAN_HandleTypeDef* pHandler, LogFunc pLogFun) : mHfdcan(pHandler), logger(pLogFun) {
    mCLock.reset();
}

bool CCanInterface::init()
{
    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        logger("failed to init canbus");
        return false;
    }
    logger("canbus init succesfull");
    return true;
}

uint32_t CCanInterface::executeTransaction(const MoteusCanFrame *pTxFrames, uint32_t pTxCount,
                                           uint32_t pExpectedReplies, uint32_t pTimeoutUs,
                                           MoteusCanFrame *pOutRxFrames, uint32_t pMaxRxCapacity)
{
    uint32_t start_us = mCLock.getUS();

    FDCAN_RxHeaderTypeDef stale_rx_header = {};
    uint8_t stale_rx_data[64] = {};
    while (HAL_FDCAN_GetRxFifoFillLevel(mHfdcan, FDCAN_RX_FIFO0) > 0) {
        HAL_FDCAN_GetRxMessage(mHfdcan, FDCAN_RX_FIFO0, &stale_rx_header, stale_rx_data);
    }

    for (uint32_t i = 0; i < pTxCount; i++) {
        FDCAN_TxHeaderTypeDef txHeader = {};
        uint8_t txData[64] = {};
        make_frame(pTxFrames[i], txHeader, txData);
        
        while (HAL_FDCAN_GetTxFifoFreeLevel(mHfdcan) == 0) {
            if (mCLock.getUS() - start_us > pTimeoutUs) {
                logger("CAN Bus error: could not transmit frames in time.");
                return 0;
            }
        }
        if (HAL_FDCAN_AddMessageToTxFifoQ(mHfdcan, &txHeader, txData) != HAL_OK) {
            recover_bus_error();
            logger("CAN Bus error: could not add frame to fifo. Restarting bus.");
            return 0;
        }
    }

    uint32_t receivedCount = 0;

    while (((mCLock.getUS() - start_us) < pTimeoutUs) && 
           (receivedCount < pExpectedReplies) && 
           (receivedCount < pMaxRxCapacity)) 
    {
        if (HAL_FDCAN_GetRxFifoFillLevel(mHfdcan, FDCAN_RX_FIFO0) > 0) {
            FDCAN_RxHeaderTypeDef rxHeader = {};
            uint8_t rxData[64] = {};
            
            if (HAL_FDCAN_GetRxMessage(mHfdcan, FDCAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK) {
                MoteusCanFrame& dest = pOutRxFrames[receivedCount];
                std::memset(&dest, 0, sizeof(MoteusCanFrame));

                dest.id = rxHeader.Identifier;
                dest.len = dlcToLen(rxHeader.DataLength);
                
                dest.flags = FLAG_NONE;
                if (rxHeader.IdType == FDCAN_EXTENDED_ID) dest.flags |= FLAG_IS_EXTID;
                if (rxHeader.FDFormat == FDCAN_FD_CAN)    dest.flags |= FLAG_IS_CANFD;
                if (rxHeader.BitRateSwitch == FDCAN_BRS_ON) dest.flags |= FLAG_USE_BRS;

                std::memcpy(dest.data, rxData, dest.len);
                receivedCount++;
            }
        }
    }
    if (receivedCount != pExpectedReplies) {
        char error_msg[128];
        sprintf(error_msg, "CAN Bus timeout %ld us: did not recieve all expected replies!", pTimeoutUs);
        logger(error_msg);
    }

    uint32_t end_us = mCLock.getUS();
    mLastBusTimeUs = end_us - start_us;
    if (mAvgBusTimeUs == 0.0f) {
        mAvgBusTimeUs = static_cast<float>(mLastBusTimeUs);
    } else {
        mAvgBusTimeUs = (FILTER_ALPHA * static_cast<float>(mLastBusTimeUs)) + 
                        ((1.0f - FILTER_ALPHA) * mAvgBusTimeUs);
    }

    return receivedCount;
}

float CCanInterface::getAvgBusTime()
{
    return mAvgBusTimeUs;
}

uint32_t CCanInterface::lenToDlc(uint8_t len) {
    switch (len) {
      case 0:  return FDCAN_DLC_BYTES_0;
      case 1:  return FDCAN_DLC_BYTES_1;
      case 2:  return FDCAN_DLC_BYTES_2;
      case 3:  return FDCAN_DLC_BYTES_3;
      case 4:  return FDCAN_DLC_BYTES_4;
      case 5:  return FDCAN_DLC_BYTES_5;
      case 6:  return FDCAN_DLC_BYTES_6;
      case 7:  return FDCAN_DLC_BYTES_7;
      case 8:  return FDCAN_DLC_BYTES_8;
      case 12: return FDCAN_DLC_BYTES_12;
      case 16: return FDCAN_DLC_BYTES_16;
      case 20: return FDCAN_DLC_BYTES_20;
      case 24: return FDCAN_DLC_BYTES_24;
      case 32: return FDCAN_DLC_BYTES_32;
      case 48: return FDCAN_DLC_BYTES_48;
      default: return FDCAN_DLC_BYTES_64;
    }
}

uint8_t CCanInterface::dlcToLen(uint32_t dlc) {
    switch (dlc) {
      case FDCAN_DLC_BYTES_0:  return 0;
      case FDCAN_DLC_BYTES_1:  return 1;
      case FDCAN_DLC_BYTES_2:  return 2;
      case FDCAN_DLC_BYTES_3:  return 3;
      case FDCAN_DLC_BYTES_4:  return 4;
      case FDCAN_DLC_BYTES_5:  return 5;
      case FDCAN_DLC_BYTES_6:  return 6;
      case FDCAN_DLC_BYTES_7:  return 7;
      case FDCAN_DLC_BYTES_8:  return 8;
      case FDCAN_DLC_BYTES_12: return 12;
      case FDCAN_DLC_BYTES_16: return 16;
      case FDCAN_DLC_BYTES_20: return 20;
      case FDCAN_DLC_BYTES_24: return 24;
      case FDCAN_DLC_BYTES_32: return 32;
      case FDCAN_DLC_BYTES_48: return 48;
      default:                 return 64;
    }
}

void CCanInterface::recover_bus_error()
{
    FDCAN_ProtocolStatusTypeDef status = {};
    
    if (HAL_FDCAN_GetProtocolStatus(mHfdcan, &status) == HAL_OK) {
        if (status.LastErrorCode != 0 && status.LastErrorCode != 7) {
            char error_msg[64];
            sprintf(error_msg, "CAN Nominal Error LEC: %ld", status.LastErrorCode);
            logger(error_msg);
        }
        if (status.DataLastErrorCode != 0 && status.DataLastErrorCode != 7) {
            char error_msg[64];
            // status.DataLastErrorCode: 1=Stuff, 2=Form, 3=Ack, 4=Bit1, 5=Bit0, 6=CRC
            sprintf(error_msg, "CAN Data Error LEC: %ld", status.DataLastErrorCode);
            logger(error_msg);
        }
        if (status.BusOff) {
            HAL_FDCAN_AbortTxRequest(mHfdcan, 0xFF);
            HAL_FDCAN_Stop(mHfdcan);
            HAL_FDCAN_Start(mHfdcan);
        }
    }
}

void CCanInterface::make_frame(const MoteusCanFrame& src, FDCAN_TxHeaderTypeDef& txHeader, uint8_t *txData)
{
    txHeader.Identifier = src.id;
    txHeader.IdType     = (src.flags & FLAG_IS_EXTID) ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
    txHeader.TxFrameType         = FDCAN_DATA_FRAME;
    txHeader.DataLength          = lenToDlc(src.len);
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.FDFormat            = (src.flags & FLAG_IS_CANFD) ? FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
    txHeader.BitRateSwitch       = (src.flags & FLAG_USE_BRS) ? FDCAN_BRS_ON : FDCAN_BRS_OFF;
    txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker       = 0;
    
    ::memcpy(txData, &src.data[0], src.len);
}
