#ifndef AD5686_PORT_H
#define AD5686_PORT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Platform contract. Each write is one SYNC-framed SPI transfer. */
bool ad5686_port_init(void);
bool ad5686_port_write(const uint8_t* tx, size_t len);
void ad5686_port_ldac_pulse(void);
void ad5686_port_reset_pulse(void);

#endif
