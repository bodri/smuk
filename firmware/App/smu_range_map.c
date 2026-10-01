#include "smu_range_map.h"
smu_range_line_t smu_range_to_line(smu_current_range_t r) {
    switch (r) {
    case SMU_RANGE_1P5A:
        return SMU_RANGE_LINE_IR2A;
    case SMU_RANGE_100MA:
        return SMU_RANGE_LINE_IR100MA;
    case SMU_RANGE_10MA:
        return SMU_RANGE_LINE_IR10MA;
    case SMU_RANGE_1MA:
        return SMU_RANGE_LINE_IR1MA;
    case SMU_RANGE_100UA:
        return SMU_RANGE_LINE_IR100UA;
    default:
        return SMU_RANGE_LINE_COUNT;
    }
}
