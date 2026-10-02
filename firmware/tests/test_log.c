#include "smu_log.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char received[SMU_LOG_MESSAGE_SIZE];
static unsigned writes;
static bool refuse;

static bool output(const char* text, size_t length) {
    ++writes;
    if (refuse)
        return false;
    memcpy(received, text, length);
    received[length] = 0;
    return true;
}

int main(void) {
    assert(!smu_log_write("before initialization"));
    assert(smu_log_dropped() == 1);
    smu_log_init(output);
    assert(smu_log_dropped() == 0);
    assert(smu_log_printf("DAC reg=0x%04X\r\n", 42u));
    assert(!strcmp(received, "DAC reg=0x002A\r\n"));
    char large[SMU_LOG_MESSAGE_SIZE + 1];
    memset(large, 'X', sizeof(large) - 1);
    large[sizeof(large) - 1] = 0;
    unsigned before = writes;
    assert(!smu_log_write(large));
    assert(!smu_log_printf("%s", large));
    assert(!smu_log_write(NULL) && !smu_log_printf(NULL));
    assert(writes == before && smu_log_dropped() == 4);
    large[SMU_LOG_MESSAGE_SIZE - 1] = 0;
    assert(smu_log_printf("%s", large));
    refuse = true;
    assert(!smu_log_write("full queue"));
    assert(smu_log_dropped() == 5);
    smu_log_init(NULL);
    assert(!smu_log_write("disabled"));
    puts("Shared logging: all tests passed.");
}
