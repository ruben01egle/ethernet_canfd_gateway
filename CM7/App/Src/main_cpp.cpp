#include "Bridge.hpp"
#include "main.h"

#include "stdio.h"
#include <cstring>
#include <inttypes.h>

#include "CInterCoreLink.hpp"

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

void send_canfd(FDCAN_ProtocolStatusTypeDef& status, FDCAN_TxHeaderTypeDef& tx_header, uint8_t* tx_data)
{
    // 1. VOR dem Senden prüfen: Sind wir im Bus-Off?
    HAL_FDCAN_GetProtocolStatus(&hfdcan1, &status);
    if (status.DataLastErrorCode != 0 && status.DataLastErrorCode != 7) {
    char error_msg[64];
        // status.DataLastErrorCode: 1=Stuff, 2=Form, 3=Ack, 4=Bit1, 5=Bit0, 6=CRC
        sprintf(error_msg, "CAN Data Error LEC: %d", status.DataLastErrorCode);
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
    CInterCoreLink& dataLink = CInterCoreLink::getInstance();
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_12, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_13, GPIO_PIN_SET);
    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        logger_wrapper("failed to init canbus");
        Error_Handler();
    }
    logger_wrapper("canbus init succesfull");
}

extern "C" void main_cpp()
{
    // Call master init for link once from cm7
    CInterCoreLink& dataLink = CInterCoreLink::getInstance();

    mm::CanFdFrame stop_frame = moteus1.MakeStop();
    int8_t rounded_size_stop = mm::RoundUpDlc(stop_frame.size);
    for (int8_t i = stop_frame.size; i < rounded_size_stop; i++) {
        stop_frame.data[i] = 0x50;
    }
    stop_frame.size = rounded_size_stop;

    // Definition der Strukturen (sollten vor der Schleife stehen)
    FDCAN_ProtocolStatusTypeDef status = {};
    FDCAN_TxHeaderTypeDef TxHeaderStop = {};
    uint8_t TxDataStop[64]; // CAN FD erlaubt bis zu 64 Byte Daten

    // 1. Konfiguration des TX-Headers für CAN FD mit BRS
    TxHeaderStop.Identifier = stop_frame.arbitration_id;                           // Test-ID
    TxHeaderStop.IdType = FDCAN_EXTENDED_ID;
    TxHeaderStop.TxFrameType = FDCAN_DATA_FRAME;
    TxHeaderStop.DataLength = MoteusStm32FdCan::lenToDlc(stop_frame.size);
    TxHeaderStop.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    TxHeaderStop.BitRateSwitch = FDCAN_BRS_ON;                 // <--- HIER schaltet der STM32 auf 5 Mbit/s!
    TxHeaderStop.FDFormat = FDCAN_FD_CAN;                      // Es ist ein CAN FD Frame
    TxHeaderStop.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    TxHeaderStop.MessageMarker = 0;

    ::memcpy(TxDataStop, &stop_frame.data[0], stop_frame.size);


    MoteusController<MoteusStm32FdCan>::PositionMode::Command cmd;
    cmd.position = NaN;
    cmd.velocity = 0.0;
    mm::CanFdFrame pos_frame = moteus1.MakePosition(cmd);
    int8_t rounded_size_pos = mm::RoundUpDlc(pos_frame.size);
    for (int8_t i = pos_frame.size; i < rounded_size_pos; i++) {
        pos_frame.data[i] = 0x50;
    }
    pos_frame.size = rounded_size_pos;

    FDCAN_TxHeaderTypeDef TxHeaderPos = {};
    uint8_t TxDataPos[64]; // CAN FD erlaubt bis zu 64 Byte Daten

    // 1. Konfiguration des TX-Headers für CAN FD mit BRS
    TxHeaderPos.Identifier = pos_frame.arbitration_id;                           // Test-ID
    TxHeaderPos.IdType = FDCAN_EXTENDED_ID;
    TxHeaderPos.TxFrameType = FDCAN_DATA_FRAME;
    TxHeaderPos.DataLength = MoteusStm32FdCan::lenToDlc(pos_frame.size);
    TxHeaderPos.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    TxHeaderPos.BitRateSwitch = FDCAN_BRS_ON;                 // <--- HIER schaltet der STM32 auf 5 Mbit/s!
    TxHeaderPos.FDFormat = FDCAN_FD_CAN;                      // Es ist ein CAN FD Frame
    TxHeaderPos.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    TxHeaderPos.MessageMarker = 0;

    ::memcpy(TxDataPos, &pos_frame.data[0], pos_frame.size);

    send_canfd(status, TxHeaderStop, TxDataStop);

    // 3. Die Sende-Schleife
    while (1) 
    {
        
        send_canfd(status, TxHeaderPos, TxDataPos);
        // 3. Etwas mehr Zeit lassen (50ms), damit man die "Bursts" auf dem Oszi schön jagen kann
        HAL_Delay(50); 
    }
    
}