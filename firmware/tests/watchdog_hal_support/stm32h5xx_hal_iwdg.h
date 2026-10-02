#ifndef TEST_IWDG_HAL_H
#define TEST_IWDG_HAL_H
#include "stm32h5xx_hal.h"
#define IWDG_PRESCALER_64 4u
#define IWDG_WINDOW_DISABLE 4095u
#define IWDG_EWI_DISABLE 0u

typedef struct {
    uint32_t Prescaler, Reload, Window, EWI;
} IWDG_InitTypeDef;

typedef struct {
    void* Instance;
    IWDG_InitTypeDef Init;
} IWDG_HandleTypeDef;

int HAL_IWDG_Init(IWDG_HandleTypeDef* handle);
int HAL_IWDG_Refresh(IWDG_HandleTypeDef* handle);
#endif
