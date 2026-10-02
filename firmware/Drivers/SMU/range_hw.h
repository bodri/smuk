#ifndef RANGE_HW_H
#define RANGE_HW_H

#include "smu_types.h"
#include <stdbool.h>

void range_hw_all_off(void);
void range_hw_select(smu_current_range_t range);
bool range_hw_is_selected(smu_current_range_t range);

/* Phase-4 transaction/readback interface. */
void range_hw_begin_transition(smu_current_range_t old_range, smu_current_range_t new_range);
bool range_hw_gate_state_valid(smu_current_range_t expected);
bool range_hw_gate_state_invalid(void);
bool range_hw_current_plausible(smu_current_range_t expected);

void range_hw_input_10m(bool on);
bool range_hw_input_10m_is_on(void);

/* Voltage sense divider range (VRANGE). */
void range_hw_select_voltage(smu_voltage_range_t range);
smu_voltage_range_t range_hw_voltage_selected(void);

/* Host-test controls; real STM32 port need not implement these publicly. */
void range_hw_mock_set_gate_delay_ms(unsigned ms);
void range_hw_mock_force_invalid(bool invalid);
void range_hw_mock_force_implausible(bool implausible);
void range_hw_mock_tick_1ms(void);

#endif
