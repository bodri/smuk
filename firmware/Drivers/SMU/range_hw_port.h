#ifndef RANGE_HW_PORT_H
#define RANGE_HW_PORT_H
#include "smu_types.h"
#include <stdbool.h>

/* Platform contract for the range switches. Arguments and return values are
 * logical (true = switch closed / range selected); the port owns the board
 * polarity. Read-back reports the actual pin level, not a RAM shadow. */
void range_hw_port_current_gate(smu_current_range_t range, bool on);
bool range_hw_port_current_gate_is_on(smu_current_range_t range);
void range_hw_port_voltage_6v(bool on);
bool range_hw_port_voltage_6v_is_on(void);

#endif
