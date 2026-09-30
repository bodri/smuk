#ifndef SMU_H
#define SMU_H
#include "smu_types.h"
void smu_init(void);
void smu_tick_1khz(void);
void smu_process(void);
smu_status_t smu_output_enable(void);
void smu_output_disable(void);
smu_status_t smu_set_mode(smu_force_mode_t mode);
smu_status_t smu_request_range(smu_current_range_t range);
const smu_context_t *smu_get_context(void);
#endif
