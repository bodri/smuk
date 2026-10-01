#ifndef ADS131M03_PORT_H
#define ADS131M03_PORT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Platform contract. STM32 implementation will bind these to SPI + DMA + GPIO.
 * The transfer is full-duplex. CS must remain asserted for the entire 5-word frame. */
bool ads131m03_port_init(void);
bool ads131m03_port_spi_dma_start(const uint8_t* tx, uint8_t* rx, size_t len);
bool ads131m03_port_spi_dma_busy(void);
void ads131m03_port_cs_assert(void);
void ads131m03_port_cs_deassert(void);

/* Blocking bring-up services. Acquisition must be disabled while these run. */
void ads131m03_port_reset(bool asserted);
bool ads131m03_port_wait_ready(uint32_t timeout_ms);
bool ads131m03_port_transfer(const uint8_t* tx, uint8_t* rx, size_t n);
void ads131m03_port_delay_ms(uint32_t ms);
void ads131m03_port_log(const char* s);

#endif
