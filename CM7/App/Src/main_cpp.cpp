#include "Bridge.hpp"
#include "main.h"

#include "stdio.h"
#include <cstring>

#include "CInterCoreLink.hpp"
#include "CUDPServer.hpp"
#include "CanInterface.hpp"

#include "Moteus.h"
#include "MoteusStm32Fdcan.h"
#include "moteus_multiplex.h"

extern FDCAN_HandleTypeDef hfdcan1;

static auto make_options() {
  MoteusController<MoteusStm32FdCan>::Options options;
  options.id = 1;
  options.disable_brs = false;
  return options;
}

MoteusStm32FdCan canBus = MoteusStm32FdCan(&hfdcan1);
MoteusController<MoteusStm32FdCan> moteus1(canBus, make_options());

void logger_wrapper(const char* pMessage) 
{
    CInterCoreLink& link = CInterCoreLink::getInstance();
    
    CInterCoreLink::LogMessage msg = {};

    std::strncpy(msg.text, pMessage, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';
    
    link.mSharedMemory.log_msg.push(msg); 
}

void MakeStop(MoteusController<MoteusStm32FdCan>& controller, FDCAN_TxHeaderTypeDef& txHeader, uint8_t* txData)
{
    mm::Query::Format query_override;
    query_override.position = mm::kInt16;
    query_override.velocity = mm::kInt16;
    query_override.torque = mm::kInt16;
    query_override.fault = mm::kInt8;
    mm::CanFdFrame stop_frame = controller.MakeStop(&query_override);
    int8_t rounded_size_stop = mm::RoundUpDlc(stop_frame.size);
    for (int8_t i = stop_frame.size; i < rounded_size_stop; i++) {
        stop_frame.data[i] = 0x50;
    }
    stop_frame.size = rounded_size_stop;

    // 1. Konfiguration des TX-Headers für CAN FD mit BRS
    txHeader.Identifier = stop_frame.arbitration_id;
    txHeader.IdType = FDCAN_EXTENDED_ID;
    txHeader.TxFrameType = FDCAN_DATA_FRAME;
    txHeader.DataLength = MoteusStm32FdCan::lenToDlc(stop_frame.size);
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch = FDCAN_BRS_ON;                 // <--- HIER schaltet der STM32 auf 5 Mbit/s!
    txHeader.FDFormat = FDCAN_FD_CAN;                      // Es ist ein CAN FD Frame
    txHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker = 0;

    ::memcpy(txData, &stop_frame.data[0], stop_frame.size);
}

void MakePos(MoteusController<MoteusStm32FdCan>& controller, FDCAN_TxHeaderTypeDef& txHeader, uint8_t* txData)
{
    MoteusController<MoteusStm32FdCan>::PositionMode::Command cmd;
    cmd.position = NaN;
    cmd.velocity = 0.0;
    mm::CanFdFrame pos_frame = controller.MakePosition(cmd);
    int8_t rounded_size_pos = mm::RoundUpDlc(pos_frame.size);
    for (int8_t i = pos_frame.size; i < rounded_size_pos; i++) {
        pos_frame.data[i] = 0x50;
    }
    pos_frame.size = rounded_size_pos;

    // 1. Konfiguration des TX-Headers für CAN FD mit BRS
    txHeader.Identifier = pos_frame.arbitration_id;                           // Test-ID
    txHeader.IdType = FDCAN_EXTENDED_ID;
    txHeader.TxFrameType = FDCAN_DATA_FRAME;
    txHeader.DataLength = MoteusStm32FdCan::lenToDlc(pos_frame.size);
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch = FDCAN_BRS_ON;                 // <--- HIER schaltet der STM32 auf 5 Mbit/s!
    txHeader.FDFormat = FDCAN_FD_CAN;                      // Es ist ein CAN FD Frame
    txHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker = 0;

    ::memcpy(txData, &pos_frame.data[0], pos_frame.size);
}

void send_canfd(FDCAN_ProtocolStatusTypeDef& status, FDCAN_TxHeaderTypeDef& tx_header, uint8_t* tx_data)
{
    // 1. VOR dem Senden prüfen: Sind wir im Bus-Off?
    HAL_FDCAN_GetProtocolStatus(&hfdcan1, &status);
    if (status.DataLastErrorCode != 0 && status.DataLastErrorCode != 7) {
    char error_msg[64];
        // status.DataLastErrorCode: 1=Stuff, 2=Form, 3=Ack, 4=Bit1, 5=Bit0, 6=CRC
        sprintf(error_msg, "CAN Data Error LEC: %ld", status.DataLastErrorCode);
        logger_wrapper(error_msg);
    }
    if (status.BusOff) 
    {
        logger_wrapper("CAN Bus-Off erkannt! Reinitialisiere...");
        
        // Hard-Reset der CAN-Peripherie, um die Error-Counter zu nullen
        HAL_FDCAN_Stop(&hfdcan1);
        
        // Wichtig: Alle alten, blockierten Sendeanforderungen löschen
        HAL_FDCAN_AbortTxRequest(&hfdcan1, 0xFFFFFFFF); 
        
        HAL_FDCAN_Start(&hfdcan1);
    }

    // 2. Normaler Sendeversuch
    if (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) > 0) 
    {
        HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx_header, tx_data);
    }
}

extern "C" void init_cpp()
{
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_12, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_13, GPIO_PIN_RESET);
}

extern "C" void main_cpp()
{
    // Call master init for link once from cm7
    CInterCoreLink& dataLink = CInterCoreLink::getInstance();
    CUDPServer udpServer(logger_wrapper);
    CCanInterface canInterface(&hfdcan1, logger_wrapper);
    if (!canInterface.init()) {
        Error_Handler();
    }

    udpServer.begin(6666);
    CInterCoreLink::IpAddr ipBuffer;
    while(true) {
        udpServer.updateServer();
        if (udpServer.getIpAddress(ipBuffer.ipAddress)) {
            dataLink.mSharedMemory.ip_addr.push(ipBuffer); 
            
            char logBuf[60];
            snprintf(logBuf, sizeof(logBuf), "Netif Up! IP: %s", ipBuffer.ipAddress);
            logger_wrapper(logBuf);
            
            break;
        }
        HAL_Delay(100);
    }

    // Definition der Strukturen (sollten vor der Schleife stehen)
    FDCAN_ProtocolStatusTypeDef status = {};
    FDCAN_TxHeaderTypeDef TxHeaderStop = {};
    uint8_t TxDataStop[64]; // CAN FD erlaubt bis zu 64 Byte Daten

    MakeStop(moteus1, TxHeaderStop, TxDataStop);

    FDCAN_TxHeaderTypeDef TxHeaderPos = {};
    uint8_t TxDataPos[64]; // CAN FD erlaubt bis zu 64 Byte Daten

    MakePos(moteus1, TxHeaderPos, TxDataPos);

    send_canfd(status, TxHeaderStop, TxDataStop);
    
    while(true) {
        /*
        if (udpServer.hasPendingTransaction()) {
            
            const UdpRequestHeader& header = udpServer.getActiveHeader();
            const MoteusCanFrame* txFrames = udpServer.getTxFrames();

            MoteusCanFrame rxResponses[12]; 

            uint32_t repliesFound = canInterface.executeTransaction(
                txFrames, 
                header.frameCount, 
                header.expectedReplies, 
                header.timeoutUs,
                rxResponses, 
                12
            );

            udpServer.sendResponse(rxResponses, repliesFound);
            dataLink.mSharedMemory.avgBusTimeUs.store(canInterface.getAvgBusTime());
            
        }
        */
        send_canfd(status, TxHeaderPos, TxDataPos);
        CUDPServer::updateServer();
        HAL_Delay(50);
    }
}