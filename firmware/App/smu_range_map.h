#ifndef SMU_RANGE_MAP_H
#define SMU_RANGE_MAP_H
#include "smu_types.h"
typedef enum { SMU_RANGE_LINE_IR2A = 0, SMU_RANGE_LINE_IR100MA, SMU_RANGE_LINE_IR10MA, SMU_RANGE_LINE_IR1MA, SMU_RANGE_LINE_IR100UA, SMU_RANGE_LINE_COUNT } smu_range_line_t;
smu_range_line_t smu_range_to_line(smu_current_range_t r);
#endif
