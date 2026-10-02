#include "smu_watchdog_port.h"
#include "stm32h5xx_hal.h"
#include "stm32h5xx_hal_iwdg.h"

static IWDG_HandleTypeDef watchdog;

bool smu_watchdog_port_was_reset(void) {
    const bool reset = __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != 0;
    __HAL_RCC_CLEAR_RESET_FLAGS();
    return reset;
}

bool smu_watchdog_port_start(void) {
    /* Frozen only when the core is halted by a debugger. Free-running with
     * no debugger attached, including firmware built with DEBUG defined. */
    __HAL_DBGMCU_FREEZE_IWDG();
    watchdog.Instance = IWDG;
    watchdog.Init.Prescaler = IWDG_PRESCALER_64;
    watchdog.Init.Reload = 999u;
    watchdog.Init.Window = IWDG_WINDOW_DISABLE;
    watchdog.Init.EWI = IWDG_EWI_DISABLE;
    /* HAL enables IWDG and its independent LSI clock. Application owns start;
     * CubeMX must not also call MX_IWDG_Init automatically. */
    return HAL_IWDG_Init(&watchdog) == HAL_OK;
}

bool smu_watchdog_port_refresh(void) {
    return HAL_IWDG_Refresh(&watchdog) == HAL_OK;
}
