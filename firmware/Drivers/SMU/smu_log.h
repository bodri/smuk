#ifndef SMU_LOG_H
#define SMU_LOG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SMU_LOG_MESSAGE_SIZE 192u
/* Foreground only. Output accepts complete messages without blocking. NULL
 * disables output. Registering a sink resets the dropped-message counter. */
typedef bool (*smu_log_output_t)(const char* text, size_t length);
void smu_log_init(smu_log_output_t output);
bool smu_log_write(const char* text);
bool smu_log_printf(const char* format, ...) __attribute__((format(printf, 1, 2)));
uint32_t smu_log_dropped(void);
#endif
