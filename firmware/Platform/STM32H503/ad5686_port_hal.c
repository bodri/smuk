#include "ad5686_port.h"

/* AD5686 bus, SYNC, LDAC and RESET pins are not frozen yet (see
 * smu_pin_contract.md). Until they are, every transfer reports failure so no
 * caller can believe the DAC was updated. */
bool ad5686_port_init(void) {
    return false;
}

bool ad5686_port_write(const uint8_t* tx, size_t len) {
    (void)tx;
    (void)len;
    return false;
}

void ad5686_port_ldac_pulse(void) {
}

void ad5686_port_reset_pulse(void) {
}
