#include "smu_scheduler.h"

static bool take(bool* p) {
    bool v = *p;
    *p = false;
    return v;
}

void smu_scheduler_init(smu_scheduler_t* s) {
    *s = (smu_scheduler_t){0};
    s->required = SMU_HEALTH_ADC | SMU_HEALTH_STATE | SMU_HEALTH_POWER;
}

void smu_scheduler_tick_1ms(smu_scheduler_t* s) {
    s->ms++;
    s->f1k = true;
    if (!(s->ms % 10))
        s->f100 = true;
    if (!(s->ms % 100))
        s->f10 = true;
}

void smu_scheduler_report_health(smu_scheduler_t* s, uint16_t b) {
    s->healthy |= b;
}

bool smu_scheduler_take_1khz(smu_scheduler_t* s) {
    return take(&s->f1k);
}

bool smu_scheduler_take_100hz(smu_scheduler_t* s) {
    return take(&s->f100);
}

bool smu_scheduler_take_10hz(smu_scheduler_t* s) {
    return take(&s->f10);
}

bool smu_scheduler_watchdog_checkpoint(smu_scheduler_t* s) {
    bool ok = (s->healthy & s->required) == s->required;
    s->healthy = 0;
    return ok;
}
