#include "ads131m03_port.h"
static bool busy;
bool ads131m03_port_init(void) { busy = false; return true; }
bool ads131m03_port_spi_dma_start(const uint8_t *tx, uint8_t *rx, size_t len)
{ (void)tx; (void)rx; (void)len; busy = true; return true; }
bool ads131m03_port_spi_dma_busy(void) { return busy; }
void ads131m03_port_cs_assert(void) {}
void ads131m03_port_cs_deassert(void) { busy = false; }
