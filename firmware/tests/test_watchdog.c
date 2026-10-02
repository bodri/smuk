#include "smu_log.h"
#include "smu_watchdog.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t now;
static unsigned starts, refreshes, reset_reads;
static bool fail_start, fail_refresh;

uint32_t smu_port_millis(void) {
    return now;
}

bool smu_watchdog_port_was_reset(void) {
    ++reset_reads;
    return true;
}

bool smu_watchdog_port_start(void) {
    ++starts;
    return !fail_start;
}

bool smu_watchdog_port_refresh(void) {
    ++refreshes;
    return !fail_refresh;
}

int main(int argc, char** argv) {
    assert(argc == 2);
    assert(!smu_watchdog_active());
    assert(smu_watchdog_before_flash() && refreshes == 0);
    assert(!smu_watchdog_service(SMU_STATE_NORMAL, false, true));
    now = UINT32_MAX - 50u;
    if (!strcmp(argv[1], "start_failure")) {
        fail_start = true;
        assert(!smu_watchdog_start() && !smu_watchdog_active());
        assert(!smu_watchdog_before_flash());
        return 0;
    }
    assert(smu_watchdog_start() && smu_watchdog_active() && smu_watchdog_was_reset());
    assert(smu_watchdog_start() && starts == 1 && reset_reads == 1);
    assert(smu_watchdog_service(SMU_STATE_NORMAL, false, true) && refreshes == 0);
    now = 48; /* 99 ms across tick wrap */
    assert(smu_watchdog_service(SMU_STATE_NORMAL, false, true) && refreshes == 0);
    now = 49;
    assert(smu_watchdog_service(SMU_STATE_NORMAL, false, true) && refreshes == 1);
    if (!strcmp(argv[1], "unsafe_fault")) {
        assert(!smu_watchdog_service(SMU_STATE_FAULT, true, false));
    } else if (!strcmp(argv[1], "input_fault")) {
        assert(!smu_watchdog_service(SMU_STATE_FAULT, false, true));
    } else if (!strcmp(argv[1], "invalid_state")) {
        assert(!smu_watchdog_service(SMU_STATE_POWER_UP, false, false));
    } else if (!strcmp(argv[1], "refresh_failure")) {
        fail_refresh = true;
        now += 100;
        assert(!smu_watchdog_service(SMU_STATE_NORMAL, false, true));
    } else if (!strcmp(argv[1], "flash_failure")) {
        fail_refresh = true;
        assert(!smu_watchdog_before_flash());
    } else {
        assert(!strcmp(argv[1], "normal"));
        now += 100;
        assert(smu_watchdog_service(SMU_STATE_CALIBRATION, false, false));
        now += 100;
        assert(smu_watchdog_service(SMU_STATE_FAULT, false, false));
        assert(refreshes == 3);
        now += 20;
        assert(smu_watchdog_before_flash() && refreshes == 4);
        now += 1500; /* synchronous, expected Flash interruption */
        assert(smu_watchdog_service(SMU_STATE_NORMAL, false, true) && refreshes == 5);
        puts("Watchdog foreground policy: all tests passed.");
        return 0;
    }
    unsigned after = refreshes;
    now += 100;
    assert(!smu_watchdog_service(SMU_STATE_NORMAL, false, false));
    assert(!smu_watchdog_before_flash() && refreshes == after);
}
