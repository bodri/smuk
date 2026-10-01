#ifndef SMU_SCHEDULER_H
#define SMU_SCHEDULER_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint32_t ms;
    bool f1k, f100, f10;
    uint16_t healthy, required;
} smu_scheduler_t;
enum { SMU_HEALTH_ADC = 1u, SMU_HEALTH_STATE = 2u, SMU_HEALTH_POWER = 4u };
void smu_scheduler_init(smu_scheduler_t*);
void smu_scheduler_tick_1ms(smu_scheduler_t*);
void smu_scheduler_report_health(smu_scheduler_t*, uint16_t);
bool smu_scheduler_take_1khz(smu_scheduler_t*);
bool smu_scheduler_take_100hz(smu_scheduler_t*);
bool smu_scheduler_take_10hz(smu_scheduler_t*);
bool smu_scheduler_watchdog_checkpoint(smu_scheduler_t*);
#endif
