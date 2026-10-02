#ifndef SMU_STM32H503_PORT_H
#define SMU_STM32H503_PORT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { SMU_SUP_P18, SMU_SUP_N18, SMU_SUP_P9, SMU_SUP_N9, SMU_SUP_3V3, SMU_SUP_RANGE_P9, SMU_SUP_PA_TEMP, SMU_SUP_AFE_TEMP, SMU_SUP_COUNT } smu_sup_ch_t;

bool smu_stm32_platform_init(void);
uint32_t smu_stm32_millis(void);
bool smu_ads_spi_dma_start(const uint8_t*, uint8_t*, size_t);
bool smu_ads_spi_busy(void);
void smu_ads_cs(bool);
bool smu_dac_spi_write(const uint8_t*, size_t);
void smu_dac_cs(bool);
void smu_dac_ldac_pulse(void);
void smu_dac_reset_pulse(void);
void smu_range_gate_write(unsigned, bool);
float smu_range_gate_read_voltage(unsigned);
bool smu_compliance_active_read(void);
void smu_pa_request_write(bool);
bool smu_pa_hw_interlock_read(void);
void smu_watchdog_heartbeat_toggle(void);
bool smu_supervisory_read(smu_sup_ch_t, float*);
void smu_stm32_tick_1khz_isr(void);
void smu_stm32_ads_drdy_isr(void);
void smu_stm32_ads_dma_complete_isr(void);
#endif
