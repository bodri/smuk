#ifndef SMU_FAULT_H
#define SMU_FAULT_H
#include "smu_types.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t active, latched, first_latched, count;
} smu_fault_manager_t;

void smu_fault_init(smu_fault_manager_t* f);
void smu_fault_raise(smu_fault_manager_t* f, uint32_t bits);
void smu_fault_set_active(smu_fault_manager_t* f, uint32_t bits, bool active);
bool smu_fault_any(const smu_fault_manager_t* f);
bool smu_fault_clear_latched(smu_fault_manager_t* f);
#endif
