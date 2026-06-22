#include "Bridge.hpp"
#include "main.h"

#include "stdio.h"
#include <cstring>
#include <inttypes.h>

#include "CInterCoreLink.hpp"

extern FDCAN_HandleTypeDef hfdcan1;


void logger_wrapper(const char* pMessage) 
{
    CInterCoreLink& link = CInterCoreLink::getInstance();
    
    CInterCoreLink::LogMessage msg = {};

    std::strncpy(msg.text, pMessage, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';
    
    link.mSharedMemory.log_msg.push(msg); 
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

    // Definition der Strukturen (sollten vor der Schleife stehen)
    FDCAN_ProtocolStatusTypeDef status = {};
    FDCAN_TxHeaderTypeDef TxHeader;
    uint8_t TxData[64]; // CAN FD erlaubt bis zu 64 Byte Daten

    // 1. Konfiguration des TX-Headers für CAN FD mit BRS
    TxHeader.Identifier = 0x55;                           // Test-ID
    TxHeader.IdType = FDCAN_STANDARD_ID;
    TxHeader.TxFrameType = FDCAN_DATA_FRAME;
    TxHeader.DataLength = FDCAN_DLC_BYTES_64;              // Maximale CAN FD Länge (64 Bytes)
    TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    TxHeader.BitRateSwitch = FDCAN_BRS_ON;                 // <--- HIER schaltet der STM32 auf 5 Mbit/s!
    TxHeader.FDFormat = FDCAN_FD_CAN;                      // Es ist ein CAN FD Frame
    TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    TxHeader.MessageMarker = 0;

    // 2. Füllen des Daten-Arrays mit abwechselnden Bits (0x55 = 01010101b)
    for(int i = 0; i < 64; i++) {
        TxData[i] = 0x55; 
    }

    // 3. Die Sende-Schleife
    while (1) 
    {
        // 1. VOR dem Senden prüfen: Sind wir im Bus-Off?
        HAL_FDCAN_GetProtocolStatus(&hfdcan1, &status);
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
            HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHeader, TxData);
        }

        // 3. Etwas mehr Zeit lassen (50ms), damit man die "Bursts" auf dem Oszi schön jagen kann
        HAL_Delay(50); 
    }
    
}