#ifndef CAL_HW_H
#define CAL_HW_H

#include <stdbool.h>

typedef enum { CALBUS_0V = 0, CALBUS_P1V5, CALBUS_P3V, CALBUS_N3V } calbus_sel_t;

void cal_hw_select_bus(calbus_sel_t s);
void cal_hw_voltage_relay(bool on);
void cal_hw_current_inject_relay(bool on);
bool cal_hw_pa_interlock_ok(void);

#endif
