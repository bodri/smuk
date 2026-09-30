#ifndef ADS131M03_BRINGUP_PORT_H
#define ADS131M03_BRINGUP_PORT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool ads131m03_bu_port_init(void);
void ads131m03_bu_port_reset(bool asserted);
bool ads131m03_bu_port_wait_ready(uint32_t timeout_ms);
bool ads131m03_bu_port_transfer(const uint8_t *tx, uint8_t *rx, size_t n);
void ads131m03_bu_port_delay_ms(uint32_t ms);
void ads131m03_bu_port_log(const char *s);
#endif
