#include "smu_watchdog_port.h"
#include "stm32h5xx_hal_iwdg.h"
#include <assert.h>
#include <stdio.h>
unsigned freeze_calls, clear_calls;
uint32_t reset_flags;
static int result;
static IWDG_HandleTypeDef* initialized;

int HAL_IWDG_Init(IWDG_HandleTypeDef* h) {
    assert(freeze_calls && h->Instance == IWDG);
    assert(h->Init.Prescaler == IWDG_PRESCALER_64 && h->Init.Reload == 999);
    assert(h->Init.Window == IWDG_WINDOW_DISABLE && h->Init.EWI == IWDG_EWI_DISABLE);
    initialized = h;
    return result;
}

int HAL_IWDG_Refresh(IWDG_HandleTypeDef* h) {
    assert(h == initialized);
    return result;
}

int main(void) {
    reset_flags = (1u << RCC_FLAG_IWDGRST) | (1u << 28);
    assert(smu_watchdog_port_was_reset() && reset_flags == 0 && clear_calls == 1);
    reset_flags = 1u << 28;
    assert(!smu_watchdog_port_was_reset() && clear_calls == 2);
    assert(smu_watchdog_port_start() && smu_watchdog_port_refresh());
    result = HAL_ERROR;
    assert(!smu_watchdog_port_start() && !smu_watchdog_port_refresh());
    puts("Watchdog HAL port: all tests passed.");
}
