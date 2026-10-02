#include "smu_stm32h503_port.h"
static uint32_t ticks;

bool smu_stm32_platform_init(void) {
    ticks = 0;
    return true;
}

uint32_t smu_stm32_millis(void) {
    return ticks;
}

bool smu_ads_spi_dma_start(const uint8_t* t, uint8_t* r, size_t n) {
    (void)t;
    (void)r;
    (void)n;
    return false;
}

bool smu_ads_spi_busy(void) {
    return false;
}

void smu_ads_cs(bool x) {
    (void)x;
}

bool smu_dac_spi_write(const uint8_t* t, size_t n) {
    (void)t;
    (void)n;
    return false;
}

void smu_dac_cs(bool x) {
    (void)x;
}

void smu_dac_ldac_pulse(void) {
}

void smu_dac_reset_pulse(void) {
}

void smu_range_gate_write(unsigned i, bool x) {
    (void)i;
    (void)x;
}

float smu_range_gate_read_voltage(unsigned i) {
    (void)i;
    return 0;
}

bool smu_compliance_active_read(void) {
    return true;
}

void smu_pa_request_write(bool x) {
    (void)x;
}

bool smu_pa_hw_interlock_read(void) {
    return false;
}

void smu_watchdog_heartbeat_toggle(void) {
}

bool smu_supervisory_read(smu_sup_ch_t c, float* v) {
    (void)c;
    if (v)
        *v = 0;
    return false;
}

void smu_stm32_tick_1khz_isr(void) {
    ticks++;
}

void smu_stm32_ads_drdy_isr(void) {
}

void smu_stm32_ads_dma_complete_isr(void) {
}
