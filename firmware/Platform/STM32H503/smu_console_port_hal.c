#include "smu_console_port.h"
#include "stm32h5xx_nucleo.h"
#include <string.h>
#define RX_SIZE 256u
#define TX_SIZE 4096u
static uint8_t rx_byte, rx[RX_SIZE], tx[TX_SIZE];
static volatile uint32_t head, tail;
static volatile bool lost;
static bool initialized;
static uint32_t tx_head;
static volatile uint32_t tx_tail;
static volatile uint16_t transmitting;

bool smu_console_port_init(void) {
    head = tail = tx_head = tx_tail = 0;
    transmitting = 0;
    lost = false;
    HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
    initialized = true;
    return HAL_UART_Receive_IT(&hcom_uart[COM1], &rx_byte, 1) == HAL_OK;
}
void USART3_IRQHandler(void) {
    HAL_UART_IRQHandler(&hcom_uart[COM1]);
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef* uart) {
    if (uart != &hcom_uart[COM1] || !initialized)
        return;
    uint32_t next = (head + 1u) % RX_SIZE;
    if (next == tail)
        lost = true;
    else {
        rx[head] = rx_byte;
        __DMB();
        head = next;
    }
    (void)HAL_UART_Receive_IT(uart, &rx_byte, 1);
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef* uart) {
    if (uart != &hcom_uart[COM1] || !initialized)
        return;
    lost = true;
    if (uart->RxState == HAL_UART_STATE_READY)
        (void)HAL_UART_Receive_IT(uart, &rx_byte, 1);
}
bool smu_console_port_rx_lost(void) {
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    bool result = lost;
    if (result)
        tail = head;
    lost = false;
    __set_PRIMASK(mask);
    return result;
}
bool smu_console_port_read(uint8_t* byte) {
    (void)smu_console_port_write("", 0);
    if (lost || tail == head)
        return false;
    __DMB();
    *byte = rx[tail];
    __DMB();
    tail = (tail + 1u) % RX_SIZE;
    return true;
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef* uart) {
    if (uart == &hcom_uart[COM1]) {
        tx_tail = (tx_tail + transmitting) % TX_SIZE;
        transmitting = 0;
    }
}
bool smu_console_port_write(const char* text, size_t length) {
    /* All writes are foreground-only; the ISR only releases completed bytes. */
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    size_t free_bytes = (tx_tail + TX_SIZE - tx_head - 1u) % TX_SIZE;
    __set_PRIMASK(mask);
    if (length > free_bytes)
        return false;
    for (size_t i = 0; i < length; ++i) {
        tx[tx_head] = (uint8_t)text[i];
        tx_head = (tx_head + 1u) % TX_SIZE;
    }
    mask = __get_PRIMASK();
    __disable_irq();
    if (!transmitting && tx_head != tx_tail) {
        transmitting = (uint16_t)(tx_head > tx_tail ? tx_head - tx_tail : TX_SIZE - tx_tail);
        if (HAL_UART_Transmit_IT(&hcom_uart[COM1], &tx[tx_tail], transmitting) != HAL_OK)
            transmitting = 0;
    }
    __set_PRIMASK(mask);
    return true;
}
