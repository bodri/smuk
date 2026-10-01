/*
 * range_hw.c
 *
 *  Created on: 29 Sept 2026
 *      Author: bodri
 */

#include "range_hw.h"
#include "range_hw_port.h"

static bool valid_current_range(smu_current_range_t r) {
    return r >= SMU_RANGE_1P5A && r <= SMU_RANGE_100UA;
}

static unsigned closed_gate_count(void) {
    unsigned n = 0;
    for (int r = SMU_RANGE_1P5A; r <= SMU_RANGE_100UA; r++)
        if (range_hw_port_current_gate_is_on((smu_current_range_t)r))
            n++;
    return n;
}

void range_hw_all_off(void) {
    for (int r = SMU_RANGE_1P5A; r <= SMU_RANGE_100UA; r++)
        range_hw_port_current_gate((smu_current_range_t)r, false);
}

/* Make-before-break: close the new shunt first so the output current path
 * never opens, then open every other shunt. SMU_RANGE_NONE opens all. */
void range_hw_select(smu_current_range_t range) {
    if (!valid_current_range(range)) {
        range_hw_all_off();
        return;
    }
    range_hw_port_current_gate(range, true);
    for (int r = SMU_RANGE_1P5A; r <= SMU_RANGE_100UA; r++)
        if (r != (int)range)
            range_hw_port_current_gate((smu_current_range_t)r, false);
}

/* True only if exactly the expected gate is closed (or none for NONE). */
bool range_hw_is_selected(smu_current_range_t range) {
    unsigned n = closed_gate_count();
    if (!valid_current_range(range))
        return n == 0u;
    return n == 1u && range_hw_port_current_gate_is_on(range);
}

void range_hw_begin_transition(smu_current_range_t old_range, smu_current_range_t new_range) {
    (void)old_range;
    range_hw_select(new_range);
}

bool range_hw_gate_state_valid(smu_current_range_t expected) {
    return range_hw_is_selected(expected);
}

bool range_hw_gate_state_invalid(void) {
    return closed_gate_count() > 1u;
}

/* No gate-voltage readback on Rev-A yet; plausibility cannot be checked. */
bool range_hw_current_plausible(smu_current_range_t expected) {
    (void)expected;
    return true;
}

void range_hw_select_voltage(smu_voltage_range_t range) {
    range_hw_port_voltage_6v(range == SMU_VRANGE_6V);
}

smu_voltage_range_t range_hw_voltage_selected(void) {
    return range_hw_port_voltage_6v_is_on() ? SMU_VRANGE_6V : SMU_VRANGE_15V;
}
