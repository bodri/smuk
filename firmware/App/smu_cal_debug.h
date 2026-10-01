#ifndef SMU_CAL_DEBUG_H
#define SMU_CAL_DEBUG_H
#include "ads131m03.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * Debugger-driven voltage calibration (moved unchanged from main.c).
 * Set test_vcal_capture_gnd / _1v5 / _3v0, then test_vcal_commit, from the
 * debugger. Owned and driven by smu.c.
 */
void smu_cal_debug_init(void);
void smu_cal_debug_frame(const ads131m03_dma_frame_t* f);
void smu_cal_debug_process(uint32_t elapsed_ms);
bool smu_cal_debug_active(void);
bool smu_cal_debug_faulted(void);

#endif
