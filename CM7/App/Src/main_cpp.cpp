#include "Bridge.hpp"
#include "main.h"

#include "stdio.h"
#include <cstring>
#include <inttypes.h>

#include "CInterCoreLink.hpp"
#include "MoteusStm32Fdcan.h"

extern FDCAN_HandleTypeDef hfdcan1;

uint32_t moteus_micros() {
  // HAL_GetTick() returns milliseconds.  For true microsecond resolution,
  // use the DWT cycle counter or a dedicated hardware timer.
  return HAL_GetTick() * 1000;
}

void moteus_delay_ms(uint32_t ms) {
  HAL_Delay(ms);
}

MoteusStm32FdCan canBus = MoteusStm32FdCan(&hfdcan1);

// Options must be set before constructing the controller.
static auto make_options() {
  MoteusController<MoteusStm32FdCan>::Options options;
  options.id = 1;
  options.disable_brs = false;
  return options;
}

MoteusController<MoteusStm32FdCan> moteus1(canBus, make_options());

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
    HAL_Delay(10000);
    if (!moteus1.SetStop()) {
        logger_wrapper("reset motor failed!");
        FDCAN_ProtocolStatusTypeDef status = {};
        HAL_FDCAN_GetProtocolStatus(&hfdcan1, &status);
        FDCAN_ErrorCountersTypeDef err = {};
        HAL_FDCAN_GetErrorCounters(&hfdcan1, &err);

        char buf[160];
        snprintf(buf, sizeof(buf),
                "LEC=%" PRIu32 " DLEC=%" PRIu32 " BO=%" PRIu32 " EP=%" PRIu32 " TEC=%" PRIu32 " REC=%" PRIu32,
                status.LastErrorCode, status.DataLastErrorCode,
                status.BusOff, status.ErrorPassive,
                err.TxErrorCnt, err.RxErrorCnt);
        logger_wrapper(buf);
        Error_Handler();
    }
    logger_wrapper("reset motor");

    uint32_t next_send = HAL_GetTick();

    while (true) {
        const uint32_t now = HAL_GetTick();
        if (now < next_send) { continue; }
        next_send += 20;  // 50 Hz

        MoteusController<MoteusStm32FdCan>::PositionMode::Command cmd;
        cmd.position = NaN;
        cmd.velocity = 0.0;

        if (!moteus1.SetPosition(cmd)) {
            logger_wrapper("set position failed!");
            FDCAN_ProtocolStatusTypeDef status = {};
            HAL_FDCAN_GetProtocolStatus(&hfdcan1, &status);

            char buf[128];
            snprintf(buf, sizeof(buf),
                "LEC=%" PRIu32 " DLEC=%" PRIu32 " BO=%" PRIu32 " EP=%" PRIu32 " EW=%" PRIu32 " TxFifoFree=%" PRIu32,
                status.LastErrorCode, 
                status.DataLastErrorCode,
                status.BusOff, 
                status.ErrorPassive, 
                status.Warning,
                HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1));
            logger_wrapper(buf);
            Error_Handler();
        }

        // Access the most recent query result:
        // const auto& result = moteus1.last_result().values;
        // result.position, result.velocity, result.torque, etc.
    }
    
}