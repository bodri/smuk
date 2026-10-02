#ifndef SMU_CONSOLE_PORT_H
#define SMU_CONSOLE_PORT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool smu_console_port_init(void);
bool smu_console_port_read(uint8_t* byte);
bool smu_console_port_write(const char* text, size_t length);
/* Sticky receive loss indication; caller discards input through next newline. */
bool smu_console_port_rx_lost(void);
#endif
