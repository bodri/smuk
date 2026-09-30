#ifndef ADS131M03_PORT_H
#define ADS131M03_PORT_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Platform contract. STM32 implementation will bind these to SPI + DMA + GPIO.
 * The transfer is full-duplex. CS must remain asserted for the entire 5-word frame. */
bool ads131m03_port_init(void);
bool ads131m03_port_spi_dma_start(const uint8_t *tx, uint8_t *rx, size_t len);
bool ads131m03_port_spi_dma_busy(void);
void ads131m03_port_cs_assert(void);
void ads131m03_port_cs_deassert(void);

#endif
