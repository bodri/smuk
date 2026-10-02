#include "smu_console_port.h"
#include "stm32h5xx_nucleo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
UART_HandleTypeDef hcom_uart[1];
static uint8_t *receive, *transmit;
static uint16_t tx_length;
static unsigned rx_arms;
static uint32_t mask;
static bool tx_fail;
uint32_t __get_PRIMASK(void) {
    return mask;
}
void __disable_irq(void) {
    mask = 1;
}
void __set_PRIMASK(uint32_t value) {
    mask = value;
}
void __DMB(void) {
}
void HAL_NVIC_SetPriority(int irq, unsigned priority, unsigned subpriority) {
    assert(irq == USART3_IRQn && priority == 5 && subpriority == 0);
}
void HAL_NVIC_EnableIRQ(int irq) {
    assert(irq == USART3_IRQn);
}
void HAL_UART_IRQHandler(UART_HandleTypeDef* uart) {
    assert(uart == hcom_uart);
}
int HAL_UART_Receive_IT(UART_HandleTypeDef* uart, uint8_t* data, uint16_t length) {
    assert(uart == hcom_uart && length == 1);
    receive = data;
    ++rx_arms;
    return HAL_OK;
}
int HAL_UART_Transmit_IT(UART_HandleTypeDef* uart, uint8_t* data, uint16_t length) {
    assert(uart == hcom_uart);
    if (tx_fail)
        return 1;
    transmit = data;
    tx_length = length;
    return HAL_OK;
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef* uart);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef* uart);
void HAL_UART_ErrorCallback(UART_HandleTypeDef* uart);
static void rx(uint8_t value) {
    *receive = value;
    HAL_UART_RxCpltCallback(hcom_uart);
}
int main(void) {
    assert(smu_console_port_init());
    rx('A');
    rx('B');
    uint8_t byte;
    assert(smu_console_port_read(&byte) && byte == 'A');
    assert(smu_console_port_read(&byte) && byte == 'B');
    assert(!smu_console_port_read(&byte));
    for (unsigned i = 0; i < 256; ++i)
        rx('X');
    assert(smu_console_port_rx_lost() && !smu_console_port_rx_lost());
    assert(!smu_console_port_read(&byte));
    unsigned arms = rx_arms;
    HAL_UART_ErrorCallback(hcom_uart);
    assert(rx_arms == arms + 1 && smu_console_port_rx_lost());
    assert(smu_console_port_write("ABC", 3));
    assert(tx_length == 3 && !memcmp(transmit, "ABC", 3));
    assert(smu_console_port_write("DEF", 3));
    assert(!memcmp(transmit, "ABC", 3));
    HAL_UART_TxCpltCallback(hcom_uart);
    smu_console_port_read(&byte);
    assert(tx_length == 3 && !memcmp(transmit, "DEF", 3));
    HAL_UART_TxCpltCallback(hcom_uart);
    char full[4095];
    memset(full, 'Q', sizeof(full));
    assert(smu_console_port_write(full, sizeof(full)));
    assert(!smu_console_port_write("X", 1));
    HAL_UART_TxCpltCallback(hcom_uart);
    smu_console_port_read(&byte);
    assert(tx_length == 5);
    HAL_UART_TxCpltCallback(hcom_uart);
    tx_fail = true;
    assert(smu_console_port_write("retry", 5));
    tx_fail = false;
    smu_console_port_read(&byte);
    assert(tx_length == 5 && !memcmp(transmit, "retry", 5));
    assert(mask == 0);
    puts("serial UART queues: all tests passed");
}
