/*
 * range_hw.c
 *
 *  Created on: 29 Sept 2026
 *      Author: bodri
 */

#include "range_hw.h"
#include <string.h>

static bool gates[6];
static smu_current_range_t mock_range_cmd = SMU_RANGE_NONE;
static smu_current_range_t mock_range_fb = SMU_RANGE_NONE;
static unsigned mock_gate_delay = 1u;
static unsigned mock_gate_countdown = 0u;
static bool mock_gate_invalid = false;
static bool mock_current_implausible = false;

void range_hw_all_off(void) {
	memset(gates, 0, sizeof(gates));
}

//void range_hw_command(smu_current_range_t r, bool on) {
//	if (r > SMU_RANGE_NONE && r <= SMU_RANGE_100UA)
//		gates[r] = on;
//}
//bool range_hw_gate_is_on(smu_current_range_t r) {
//	return (r > SMU_RANGE_NONE && r <= SMU_RANGE_100UA) ? gates[r] : false;
//}
//bool range_hw_verify_one_hot(smu_current_range_t expected) {
//	int n = 0, last = 0;
//	for (int i = 1; i <= 5; i++)
//		if (gates[i]) {
//			n++;
//			last = i;
//		}
//	return expected == SMU_RANGE_NONE ?
//			n == 0 : (n == 1 && last == (int) expected);
//}

void range_hw_begin_transition(smu_current_range_t old_range,
		smu_current_range_t new_range) {
	(void) old_range;
	mock_range_cmd = new_range;
	mock_gate_countdown = mock_gate_delay;
	if (mock_gate_countdown == 0u)
		mock_range_fb = new_range;
}

bool range_hw_gate_state_valid(smu_current_range_t expected) {
	return !mock_gate_invalid && mock_range_fb == expected;
}

bool range_hw_gate_state_invalid(void) {
	return mock_gate_invalid;
}

bool range_hw_current_plausible(smu_current_range_t expected) {
	(void) expected;
	return !mock_current_implausible;
}

void range_hw_mock_set_gate_delay_ms(unsigned ms) {
	mock_gate_delay = ms;
}
void range_hw_mock_force_invalid(bool v) {
	mock_gate_invalid = v;
}
void range_hw_mock_force_implausible(bool v) {
	mock_current_implausible = v;
}

void range_hw_mock_tick_1ms(void) {
	if (mock_gate_countdown) {
		mock_gate_countdown--;
		if (mock_gate_countdown == 0u)
			mock_range_fb = mock_range_cmd;
	}
}
