#include "smu_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static smu_log_output_t sink;
static uint32_t dropped;

static bool drop(void) {
    if (dropped != UINT32_MAX)
        ++dropped;
    return false;
}

void smu_log_init(smu_log_output_t output) {
    sink = output;
    dropped = 0;
}

bool smu_log_write(const char* text) {
    if (!text)
        return drop();
    size_t length = strlen(text);
    if (!sink || length >= SMU_LOG_MESSAGE_SIZE || !sink(text, length))
        return drop();
    return true;
}

bool smu_log_printf(const char* format, ...) {
    if (!format)
        return drop();
    char text[SMU_LOG_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= sizeof(text))
        return drop();
    return smu_log_write(text);
}

uint32_t smu_log_dropped(void) {
    return dropped;
}
