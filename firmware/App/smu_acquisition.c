#include "smu_acquisition.h"

static uint32_t saturating_add(uint32_t a, uint32_t b) {
    return b > UINT32_MAX - a ? UINT32_MAX : a + b;
}

void smu_acquisition_init(smu_acquisition_t* s, uint32_t now, smu_acquisition_counters_t counters) {
    *s = (smu_acquisition_t){0};
    s->cfg = (smu_acquisition_config_t){.stale_ms = 20u, .stopped_fault_ms = 1000u, .error_window_ms = 1000u, .error_limit = 10u};
    s->previous = counters;
    s->last_frame_ms = s->last_poll_ms = s->window_start_ms = now;
    s->stale = true;
}

void smu_acquisition_resume(smu_acquisition_t* s, uint32_t now, smu_acquisition_counters_t counters) {
    s->previous = counters;
    s->last_frame_ms = s->last_poll_ms = now;
    s->age_ms = 0;
    s->seen_frame = false;
    s->stale = true;
    s->gap = false;
    if (now - s->window_start_ms >= s->cfg.error_window_ms) {
        s->window_start_ms = now;
        s->window_errors = 0;
    }
}

void smu_acquisition_update(smu_acquisition_t* s, uint32_t now, smu_acquisition_counters_t counters) {
    uint32_t errors = counters.crc_errors - s->previous.crc_errors;
    errors = saturating_add(errors, counters.spi_errors - s->previous.spi_errors);
    /* Busy counts skipped DRDY triggers while DMA is active. It does not
     * invalidate CRC-checked frames already delivered by the driver. Keep
     * it diagnostic; stopped acquisition is detected by frame progress. */
    errors = saturating_add(errors, counters.overruns - s->previous.overruns);
    const bool delayed = now - s->last_poll_ms >= s->cfg.stale_ms;
    const bool was_stale = s->stale;
    s->gap = errors != 0 || delayed;
    if (now - s->window_start_ms >= s->cfg.error_window_ms) {
        s->window_start_ms = now;
        s->window_errors = 0;
    }
    s->window_errors = saturating_add(s->window_errors, errors);
    if (s->gap)
        s->seen_frame = false;
    else if (counters.frames != s->previous.frames) {
        s->last_frame_ms = now;
        s->seen_frame = true;
    }
    s->age_ms = now - s->last_frame_ms;
    s->stale = !s->seen_frame || s->age_ms >= s->cfg.stale_ms;
    if (s->stale && !was_stale)
        s->gap = true;
    if (s->gap)
        s->gap_count = saturating_add(s->gap_count, 1u);
    if (s->age_ms >= s->cfg.stopped_fault_ms || s->window_errors >= s->cfg.error_limit)
        s->fault = true;
    s->previous = counters;
    s->last_poll_ms = now;
}
