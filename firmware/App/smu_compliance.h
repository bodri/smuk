#ifndef SMU_COMPLIANCE_H
#define SMU_COMPLIANCE_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool active;
    bool servo_allowed;
    uint16_t clear_frames_required;
    uint16_t clear_frames;
    uint32_t entries;
} smu_compliance_t;

void smu_compliance_init(smu_compliance_t* c, uint16_t clear_frames_required);
void smu_compliance_update(smu_compliance_t* c, bool hw_active, bool measurement_valid, bool range_busy);
#endif
