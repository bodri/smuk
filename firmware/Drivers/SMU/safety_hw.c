/*
 * safety_hw.c
 *
 *  Created on: 29 Sept 2026
 *      Author: bodri
 */

#include "safety_hw.h"
#include "range_hw.h"
#include <string.h>

// static bool gates[6];
static bool pa = false;

void safety_hw_init_safe(void) {
    pa = false;
    //	memset(gates, 0, sizeof(gates));
}

void safety_hw_disable_pa(void) {
    pa = false;
}

bool safety_hw_request_pa_enable(void) {
    if (range_hw_input_10m_is_on())
        return false;
    pa = true;
    return true;
}

bool safety_hw_pa_requested(void) {
    return pa;
}

bool safety_hw_power_good(void) {
    return true;
}

bool safety_hw_compliance_active(void) {
    return false;
}

void safety_hw_watchdog_heartbeat(void) {
}

void safety_hw_enable_pa_request(void) {
    (void)safety_hw_request_pa_enable();
}
