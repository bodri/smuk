#include "smu_watchdog.h"
#include "smu_log.h"
#include "smu_port.h"
#include "smu_watchdog_port.h"

#define REFRESH_INTERVAL_MS 100u
static bool active, was_reset, blocked;
static uint32_t last_refresh;

static bool refresh(void) {
    if (!smu_watchdog_port_refresh()) {
        blocked = true;
        return false;
    }
    last_refresh = smu_port_millis();
    return true;
}

bool smu_watchdog_start(void) {
    if (active)
        return !blocked;
    was_reset = smu_watchdog_port_was_reset();
    blocked = false;
    if (!smu_watchdog_port_start()) {
        blocked = true;
        return false;
    }
    active = true;
    last_refresh = smu_port_millis();
    (void)smu_log_printf("SMU IWDG started nominal_ms=2000 reset=%u\r\n", was_reset);
    return true;
}

bool smu_watchdog_service(smu_state_t state, bool pa_requested, bool input_10m) {
    if (!active || blocked)
        return false;
    const bool allowed = state == SMU_STATE_NORMAL || state == SMU_STATE_CALIBRATION || (state == SMU_STATE_FAULT && !pa_requested && !input_10m);
    if (!allowed) {
        blocked = true;
        return false;
    }
    return smu_port_millis() - last_refresh < REFRESH_INTERVAL_MS || refresh();
}

bool smu_watchdog_before_flash(void) {
    return !blocked && (!active || refresh());
}

bool smu_watchdog_active(void) {
    return active;
}

bool smu_watchdog_was_reset(void) {
    return was_reset;
}
