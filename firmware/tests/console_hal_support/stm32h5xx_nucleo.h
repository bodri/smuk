#ifndef CONSOLE_TEST_HAL_H
#define CONSOLE_TEST_HAL_H
#include <stdint.h>

typedef struct {
    unsigned RxState;
} UART_HandleTypeDef;

extern UART_HandleTypeDef hcom_uart[1];
#define COM1 0
#define USART3_IRQn 39
#define HAL_OK 0
#define HAL_UART_STATE_READY 0
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t mask);
void __DMB(void);
void HAL_NVIC_SetPriority(int irq, unsigned priority, unsigned subpriority);
void HAL_NVIC_EnableIRQ(int irq);
int HAL_UART_Receive_IT(UART_HandleTypeDef* uart, uint8_t* data, uint16_t length);
int HAL_UART_Transmit_IT(UART_HandleTypeDef* uart, uint8_t* data, uint16_t length);
void HAL_UART_IRQHandler(UART_HandleTypeDef* uart);
#endif
