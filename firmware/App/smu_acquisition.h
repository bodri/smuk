#ifndef SMU_ACQUISITION_H
#define SMU_ACQUISITION_H
#include <stdbool.h>
#include <stdint.h>

/* Foreground health monitoring; no changes to ADC/SPI interrupt behavior. */
typedef struct {
    uint32_t frames, crc_errors, spi_errors, busy, overruns;
} smu_acquisition_counters_t;

typedef struct {
    uint32_t stale_ms, stopped_fault_ms, error_window_ms, error_limit;
} smu_acquisition_config_t;

typedef struct {
    smu_acquisition_config_t cfg;
    smu_acquisition_counters_t previous;
    uint32_t last_frame_ms, last_poll_ms, window_start_ms, window_errors;
    uint32_t age_ms, gap_count;
    bool seen_frame, stale, gap, fault;
} smu_acquisition_t;

void smu_acquisition_init(smu_acquisition_t* s, uint32_t now, smu_acquisition_counters_t counters);
/* Expected foreground interruption: preserve configuration, diagnostics and
 * latched faults, restart freshness timeout without counting a transport gap. */
void smu_acquisition_resume(smu_acquisition_t* s, uint32_t now, smu_acquisition_counters_t counters);
void smu_acquisition_update(smu_acquisition_t* s, uint32_t now, smu_acquisition_counters_t counters);
#endif
