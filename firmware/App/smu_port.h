#ifndef SMU_PORT_H
#define SMU_PORT_H
#include <stdint.h>

/* Platform contract for the top-level SMU: free-running millisecond clock. */
uint32_t smu_port_millis(void);

#endif
