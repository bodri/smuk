#include "smu_fault.h"

void smu_fault_init(smu_fault_manager_t* f) {
    *f = (smu_fault_manager_t){0};
}

void smu_fault_raise(smu_fault_manager_t* f, uint32_t b) {
    if (!b)
        return;
    if (!f->latched)
        f->first_latched = b;
    f->active |= b;
    f->latched |= b;
    f->count++;
}

void smu_fault_set_active(smu_fault_manager_t* f, uint32_t b, bool a) {
    if (a)
        smu_fault_raise(f, b);
    else
        f->active &= ~b;
}

bool smu_fault_any(const smu_fault_manager_t* f) {
    return f->latched != 0u;
}

bool smu_fault_clear_latched(smu_fault_manager_t* f) {
    if (f->active)
        return false;
    f->latched = 0;
    f->first_latched = 0;
    return true;
}
