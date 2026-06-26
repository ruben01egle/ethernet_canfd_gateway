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

uint8_t dlcToLen(uint32_t dlc) {
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

void compare_can_frames(const FDCAN_TxHeaderTypeDef& expectedHeader, const uint8_t* expectedData,
                         const FDCAN_TxHeaderTypeDef& actualHeader, const uint8_t* actualData,
                         const char* contextName) 
{
    char logBuf[128];
    bool mismatchDetected = false;

    snprintf(logBuf, sizeof(logBuf), "--- Vergleiche [%s] mit Test-Frame ---", contextName);
    logger_wrapper(logBuf);

    if (expectedHeader.Identifier != actualHeader.Identifier) {
        snprintf(logBuf, sizeof(logBuf), "Mismatch Identifier: Erwartet 0x%LX, Erhalten 0x%LX", expectedHeader.Identifier, actualHeader.Identifier);
        logger_wrapper(logBuf);
        mismatchDetected = true;
    }

    // 2. IdType (Standard- oder Extended ID)
    if (expectedHeader.IdType != actualHeader.IdType) {
        snprintf(logBuf, sizeof(logBuf), "Mismatch IdType: Erwartet 0x%LX (%s), Erhalten 0x%LX (%s)", 
                 expectedHeader.IdType, (expectedHeader.IdType == FDCAN_EXTENDED_ID) ? "EXT" : "STD",
                 actualHeader.IdType, (actualHeader.IdType == FDCAN_EXTENDED_ID) ? "EXT" : "STD");
        logger_wrapper(logBuf);
        mismatchDetected = true;
    }

    // 3. TxFrameType (Data Frame oder Remote Frame)
    if (expectedHeader.TxFrameType != actualHeader.TxFrameType) {
        snprintf(logBuf, sizeof(logBuf), "Mismatch TxFrameType: Erwartet 0x%LX, Erhalten 0x%LX", expectedHeader.TxFrameType, actualHeader.TxFrameType);
        logger_wrapper(logBuf);
        mismatchDetected = true;
    }

    // 4. DataLength (DLC)
    if (expectedHeader.DataLength != actualHeader.DataLength) {
        snprintf(logBuf, sizeof(logBuf), "Mismatch DataLength (DLC): Erwartet 0x%LX, Erhalten 0x%LX", expectedHeader.DataLength, actualHeader.DataLength);
        logger_wrapper(logBuf);
        mismatchDetected = true;
    }

    // 5. ErrorStateIndicator (ESI - Aktiv/Passiv)
    if (expectedHeader.ErrorStateIndicator != actualHeader.ErrorStateIndicator) {
        snprintf(logBuf, sizeof(logBuf), "Mismatch ErrorStateIndicator: Erwartet 0x%LX, Erhalten 0x%LX", expectedHeader.ErrorStateIndicator, actualHeader.ErrorStateIndicator);
        logger_wrapper(logBuf);
        mismatchDetected = true;
    }

    // 6. BitRateSwitch (BRS - Umschalten auf z.B. 5 Mbit/s)
    if (expectedHeader.BitRateSwitch != actualHeader.BitRateSwitch) {
        snprintf(logBuf, sizeof(logBuf), "Mismatch BitRateSwitch (BRS): Erwartet %s, Erhalten %s", 
                 (expectedHeader.BitRateSwitch == FDCAN_BRS_ON) ? "AN" : "AUS", 
                 (actualHeader.BitRateSwitch == FDCAN_BRS_ON) ? "AN" : "AUS");
        logger_wrapper(logBuf);
        mismatchDetected = true;
    }

    // 7. FDFormat (Klassisches CAN vs. CAN FD)
    if (expectedHeader.FDFormat != actualHeader.FDFormat) {
        snprintf(logBuf, sizeof(logBuf), "Mismatch FDFormat: Erwartet 0x%LX (%s), Erhalten 0x%LX (%s)", 
                 expectedHeader.FDFormat, (expectedHeader.FDFormat == FDCAN_FD_CAN) ? "FD" : "Classic",
                 actualHeader.FDFormat, (actualHeader.FDFormat == FDCAN_FD_CAN) ? "FD" : "Classic");
        logger_wrapper(logBuf);
        mismatchDetected = true;
    }

    // 8. TxEventFifoControl (Soll das Event im FIFO landen?)
    if (expectedHeader.TxEventFifoControl != actualHeader.TxEventFifoControl) {
        snprintf(logBuf, sizeof(logBuf), "Mismatch TxEventFifoControl: Erwartet 0x%LX, Erhalten 0x%LX", expectedHeader.TxEventFifoControl, actualHeader.TxEventFifoControl);
        logger_wrapper(logBuf);
        mismatchDetected = true;
    }

    // 9. MessageMarker (Identifikationsmarker für die Applikation)
    if (expectedHeader.MessageMarker != actualHeader.MessageMarker) {
        snprintf(logBuf, sizeof(logBuf), "Mismatch MessageMarker: Erwartet 0x%X, Erhalten 0x%X", expectedHeader.MessageMarker, actualHeader.MessageMarker);
        logger_wrapper(logBuf);
        mismatchDetected = true;
    }

    // 2. Daten/Payload vergleichen (Nur wenn DLC identisch ist, sonst macht Byte-Vergleich wenig Sinn)
    if (expectedHeader.DataLength == actualHeader.DataLength) {
        uint8_t len = dlcToLen(expectedHeader.DataLength);
        if (std::memcmp(expectedData, actualData, len) != 0) {
            logger_wrapper("Mismatch in Payload-Daten entdeckt:");
            for (int i = 0; i < len; i++) {
                if (expectedData[i] != actualData[i]) {
                    snprintf(logBuf, sizeof(logBuf), "  Byte [%d]: Erwartet 0x%02X, Erhalten 0x%02X", i, expectedData[i], actualData[i]);
                    logger_wrapper(logBuf);
                }
            }
            mismatchDetected = true;
        }
    }
    else {
        logger_wrapper("Mismatch in Payload-Len");
        mismatchDetected = true;
    }

    if (!mismatchDetected) {
        logger_wrapper("SUCCESS: Frames sind absolut identisch!");
    }
    logger_wrapper("----------------------------------------");
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
        
        if (udpServer.hasPendingTransaction()) {
            
            const UdpRequestHeader& header = udpServer.getActiveHeader();
            const MoteusCanFrame* txFrames = udpServer.getTxFrames();

            FDCAN_TxHeaderTypeDef TxHeaderTest = {};
            uint8_t TxDataTest[64];
            canInterface.make_frame(txFrames[0], TxHeaderTest, TxDataTest);

            compare_can_frames(TxHeaderStop, TxDataStop, TxHeaderTest, TxDataTest, "StopFrame_vs_TestFrame");
            udpServer.setPending(false);

            /*
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
            */
        }
        
        send_canfd(status, TxHeaderPos, TxDataPos);
        CUDPServer::updateServer();
        HAL_Delay(50);
    }
}