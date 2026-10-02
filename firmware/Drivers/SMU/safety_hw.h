#ifndef SAFETY_HW_H
#define SAFETY_HW_H

#include <stdbool.h>

void safety_hw_init_safe(void);
void safety_hw_disable_pa(void);
/* Caller must disconnect input loading and wait for range settling first. */
bool safety_hw_request_pa_enable(void);
/* Software request state only; PA hardware feedback is not implemented yet. */
bool safety_hw_pa_requested(void);
bool safety_hw_power_good(void);
bool safety_hw_compliance_active(void);
void safety_hw_watchdog_heartbeat(void);
void safety_hw_enable_pa_request(void);

#endif
