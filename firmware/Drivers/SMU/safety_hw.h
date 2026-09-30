#ifndef SAFETY_HW_H
#define SAFETY_HW_H

#include <stdbool.h>

void safety_hw_init_safe(void);
void safety_hw_disable_pa(void);
bool safety_hw_request_pa_enable(void);
bool safety_hw_power_good(void);
bool safety_hw_compliance_active(void);
void safety_hw_watchdog_heartbeat(void);
void safety_hw_enable_pa_request(void);

#endif
