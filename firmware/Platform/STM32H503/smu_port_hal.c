#include "smu_port.h"

#include "main.h"

uint32_t smu_port_millis(void) {
    return HAL_GetTick();
}
