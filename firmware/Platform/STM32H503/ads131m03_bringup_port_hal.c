/* Copy into the STM32CubeH5 application after main.h/spi.h/usart.h exist.
 * Confirm generated handle/header names if your Cube project differs.
 */
#include "ads131m03_bringup_port.h"
#include "main.h"
#include "spi.h"
#include <string.h>

/* Board: SPI1 SCK=PC5, MOSI=PC7, MISO=PA6; CS/NSS=PB8 GPIO; DRDY=PC6; RESET=PC9. */
extern SPI_HandleTypeDef hspi1;

bool ads131m03_bu_port_init(void) {
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8,GPIO_PIN_SET); /* CS inactive */
    return true;
}
void ads131m03_bu_port_reset(bool asserted) {
    HAL_GPIO_WritePin(GPIOC,GPIO_PIN_9,asserted?GPIO_PIN_RESET:GPIO_PIN_SET);
}
bool ads131m03_bu_port_wait_ready(uint32_t timeout_ms) {
    /* After reset, TI specifies low->high DRDY as SPI-ready. Polling is intentional for bring-up. */
    uint32_t t0=HAL_GetTick(); GPIO_PinState old=HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_6);
    while((HAL_GetTick()-t0)<timeout_ms) {
        GPIO_PinState now=HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_6);
        if(old==GPIO_PIN_RESET && now==GPIO_PIN_SET) return true;
        old=now;
    }
    return false;
}
bool ads131m03_bu_port_transfer(const uint8_t *tx,uint8_t *rx,size_t n) {
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8,GPIO_PIN_RESET);
    HAL_StatusTypeDef st=HAL_SPI_TransmitReceive(&hspi1,(uint8_t*)tx,rx,(uint16_t)n,100);
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8,GPIO_PIN_SET);
    return st==HAL_OK;
}
void ads131m03_bu_port_delay_ms(uint32_t ms){HAL_Delay(ms);}

/* Replace this body with your VCP/UART/USB-CDC transmit function. */
__weak void ads131m03_bu_port_log(const char *s){(void)s;}
