#ifndef TEST_WATCHDOG_HAL_H
#define TEST_WATCHDOG_HAL_H
#include <stdint.h>
#define HAL_OK 0
#define HAL_ERROR 1
#define IWDG ((void*)0x40003000u)
#define RCC_FLAG_IWDGRST 29u
extern unsigned freeze_calls, clear_calls;
extern uint32_t reset_flags;
#define __HAL_DBGMCU_FREEZE_IWDG() (++freeze_calls)
#define __HAL_RCC_GET_FLAG(flag) ((reset_flags >> (flag)) & 1u)
#define __HAL_RCC_CLEAR_RESET_FLAGS() (++clear_calls, reset_flags = 0)
#endif
