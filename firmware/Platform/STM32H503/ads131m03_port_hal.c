#include "ads131m03_port.h"

#include "main.h"
#include "spi.h"

/* SPI1 full-duplex DMA; PB8 is the active-low GPIO chip select. */
bool ads131m03_port_init(void) {
    ads131m03_port_cs_deassert();
    return true;
}

bool ads131m03_port_spi_dma_start(const uint8_t* tx, uint8_t* rx, size_t len) {
    return HAL_SPI_TransmitReceive_DMA(&hspi1, (uint8_t*)tx, rx, (uint16_t)len) == HAL_OK;
}

bool ads131m03_port_spi_dma_busy(void) {
    return HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY;
}

void ads131m03_port_cs_assert(void) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
}

void ads131m03_port_cs_deassert(void) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
}

/* Board DRDY=PC6 and active-low SYNC/RESET=PC9. */
void ads131m03_port_reset(bool asserted) {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, asserted ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
bool ads131m03_port_wait_ready(uint32_t timeout_ms) {
    /* After reset, TI specifies low->high DRDY as SPI-ready. Polling is intentional for bring-up. */
    uint32_t t0 = HAL_GetTick();
    GPIO_PinState old = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6);
    while ((HAL_GetTick() - t0) < timeout_ms) {
        GPIO_PinState now = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6);
        if (old == GPIO_PIN_RESET && now == GPIO_PIN_SET)
            return true;
        old = now;
    }
    return false;
}
bool ads131m03_port_transfer(const uint8_t* tx, uint8_t* rx, size_t n) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi1, (uint8_t*)tx, rx, (uint16_t)n, 100);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
    return st == HAL_OK;
}
void ads131m03_port_delay_ms(uint32_t ms) {
    HAL_Delay(ms);
}

/* Replace this body with your VCP/UART/USB-CDC transmit function. */
__weak void ads131m03_port_log(const char* s) {
    (void)s;
}
