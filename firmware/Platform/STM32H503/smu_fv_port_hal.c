#include "main.h"
#include "smu_fv_board.h"
#include "smu_fv_port.h"
#include "vendor/stm32h5xx_ll_dac.h"

#if SMU_FV_HW_CONFIRMED
/* GPIO clocks for both control ports must be enabled by CubeMX. */
#if !defined(SMU_FV_REFERENCE_V) || !defined(SMU_FV_MIN_V) || !defined(SMU_FV_MAX_V) || !defined(SMU_FV_SLEW_V_S) || !defined(SMU_FV_MAX_CURRENT_A) || !defined(SMU_FV_EXTERNAL_LIMITS_CONFIRMED)
#error "Confirm FV wiring, analog scaling, bounds and external hardware current limits first"
#endif
#if !SMU_FV_PA_ENABLE_STUB && (!defined(SMU_FV_PA_PORT) || !defined(SMU_FV_PA_PIN) || !defined(SMU_FV_PA_ACTIVE_HIGH))
#error "Define PA ENABLE wiring or explicitly use its stub"
#endif
#if !SMU_FV_RELAY_STUB && (!defined(SMU_FV_RELAY_PORT) || !defined(SMU_FV_RELAY_PIN) || !defined(SMU_FV_RELAY_ACTIVE_HIGH))
#error "Define relay wiring or explicitly use the relay stub"
#endif
#if !SMU_FV_EXTERNAL_LIMITS_CONFIRMED
#error "FV requires independently established source/sink hardware current limits"
#endif
static bool ready;

#if !SMU_FV_PA_ENABLE_STUB || !SMU_FV_RELAY_STUB
static void control(GPIO_TypeDef* port, uint16_t pin, bool high_active, bool enabled) {
    HAL_GPIO_WritePin(port, pin, enabled == high_active ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
#endif
#endif

bool smu_fv_port_pa(bool enabled) {
#if SMU_FV_PA_ENABLE_STUB
    (void)enabled;
    return true;
#elif SMU_FV_HW_CONFIRMED
    control(SMU_FV_PA_PORT, SMU_FV_PA_PIN, SMU_FV_PA_ACTIVE_HIGH, enabled);
    return true;
#else
    return !enabled;
#endif
}

bool smu_fv_port_relay(bool connected) {
#if SMU_FV_RELAY_STUB
    (void)connected;
    return true;
#elif SMU_FV_HW_CONFIRMED
    control(SMU_FV_RELAY_PORT, SMU_FV_RELAY_PIN, SMU_FV_RELAY_ACTIVE_HIGH, connected);
    return true;
#else
    return !connected;
#endif
}

bool smu_fv_port_init(smu_fv_config_t* config) {
    config->pa_enable_stub = SMU_FV_PA_ENABLE_STUB;
    config->relay_stub = SMU_FV_RELAY_STUB;
#if SMU_FV_HW_CONFIRMED
    (void)smu_fv_port_pa(false);
    (void)smu_fv_port_relay(false);
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef pin = {0};
    pin.Pin = GPIO_PIN_5;
    pin.Mode = GPIO_MODE_ANALOG;
    pin.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &pin);
    RCC_PeriphCLKInitTypeDef clock = {0};
    clock.PeriphClockSelection = RCC_PERIPHCLK_ADCDAC;
    clock.AdcDacClockSelection = RCC_ADCDACCLKSOURCE_HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK)
        return false;
    __HAL_RCC_DAC1_CLK_ENABLE();
    __HAL_RCC_DAC1_FORCE_RESET();
    __HAL_RCC_DAC1_RELEASE_RESET();
    const uint32_t hclk = HAL_RCC_GetHCLKFreq();
    LL_DAC_SetHighFrequencyMode(DAC1, hclk > 160000000u ? LL_DAC_HIGH_FREQ_MODE_ABOVE_160MHZ : hclk > 80000000u ? LL_DAC_HIGH_FREQ_MODE_ABOVE_80MHZ : LL_DAC_HIGH_FREQ_MODE_DISABLE);
    LL_DAC_SetMode(DAC1, LL_DAC_CHANNEL_2, LL_DAC_MODE_NORMAL_OPERATION);
    LL_DAC_DisableTrigger(DAC1, LL_DAC_CHANNEL_2);
    LL_DAC_ConfigOutput(DAC1, LL_DAC_CHANNEL_2, LL_DAC_OUTPUT_MODE_NORMAL, SMU_FV_DAC_BUFFERED ? LL_DAC_OUTPUT_BUFFER_ENABLE : LL_DAC_OUTPUT_BUFFER_DISABLE, LL_DAC_OUTPUT_CONNECT_GPIO);
    *config = (smu_fv_config_t){SMU_FV_REFERENCE_V,   SMU_FV_ZERO_DAC_V,   SMU_FV_GAIN,           SMU_FV_MIN_V,      SMU_FV_MAX_V,        SMU_FV_SLEW_V_S,
                                SMU_FV_MAX_CURRENT_A, SMU_FV_DAC_BUFFERED, SMU_FV_PA_ENABLE_STUB, SMU_FV_RELAY_STUB, SMU_FV_ZERO_OFFSET_V};
    ready = true;
    /* Leave the DAC disabled until the first validated neutral code is loaded.
     * Stubbed PA ENABLE cannot provide physical startup protection. */
    return true;
#else
    (void)config;
    return false;
#endif
}

bool smu_fv_port_write(uint16_t code) {
#if SMU_FV_HW_CONFIRMED
    if (!ready || code > 4095u)
        return false;
    LL_DAC_ConvertData12RightAligned(DAC1, LL_DAC_CHANNEL_2, code);
    if (!LL_DAC_IsEnabled(DAC1, LL_DAC_CHANNEL_2)) {
        LL_DAC_Enable(DAC1, LL_DAC_CHANNEL_2);
        HAL_Delay(1);
    }
    return true;
#else
    (void)code;
    return false;
#endif
}
