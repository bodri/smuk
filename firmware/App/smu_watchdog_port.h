#ifndef SMU_WATCHDOG_PORT_H
#define SMU_WATCHDOG_PORT_H
#include <stdbool.h>
/* Capture the IWDG reset flag before clearing all RCC reset flags. */
bool smu_watchdog_port_was_reset(void);
bool smu_watchdog_port_start(void);
bool smu_watchdog_port_refresh(void);
#endif
