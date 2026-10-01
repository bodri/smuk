#include "smu_compliance.h"

void smu_compliance_init(smu_compliance_t* c, uint16_t n) {
    *c = (smu_compliance_t){0};
    c->clear_frames_required = n ? n : 1u;
}

void smu_compliance_update(smu_compliance_t* c, bool hw_active, bool measurement_valid, bool range_busy) {
    if (hw_active) {
        if (!c->active)
            c->entries++;
        c->active = true;
        c->clear_frames = 0;
        c->servo_allowed = false;
        return;
    }

    if (c->active) {
        if (++c->clear_frames >= c->clear_frames_required) {
            c->active = false;
            c->clear_frames = 0;
        }
    }

    c->servo_allowed = (!c->active && measurement_valid && !range_busy);
}
