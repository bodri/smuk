#include "smu_fv_port.h"

bool smu_fv_port_init(smu_fv_config_t* c) {
    (void)c;
    return false;
}

bool smu_fv_port_write(uint16_t c) {
    (void)c;
    return false;
}

bool smu_fv_port_pa(bool enabled) {
    return !enabled;
}

bool smu_fv_port_relay(bool enabled) {
    return !enabled;
}
