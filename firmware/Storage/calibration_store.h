#ifndef CALIBRATION_STORE_H
#define CALIBRATION_STORE_H
#include "smu_measurement.h"
#include <stdbool.h>
#include <stdint.h>

#define SMU_CAL_MAGIC 0x534D5543u /* 'SMUC' */
#define SMU_CAL_VERSION 1u
#define SMU_CAL_IFORCE_RANGES 5u

typedef struct {
    float gain;
    float offset;
} smu_source_cal_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t sequence;
    int16_t calibration_temp_centiC;
    uint16_t flags;
    smu_measurement_cal_t measurement;
    smu_source_cal_t vforce;
    smu_source_cal_t iforce[SMU_CAL_IFORCE_RANGES];
    uint32_t crc32;
} smu_cal_record_t;

typedef enum { SMU_CAL_LOAD_OK = 0, SMU_CAL_LOAD_DEFAULTS, SMU_CAL_LOAD_INVALID } smu_cal_load_result_t;

void smu_cal_record_defaults(smu_cal_record_t* rec);
void smu_cal_record_finalize(smu_cal_record_t* rec);
bool smu_cal_record_validate(const smu_cal_record_t* rec);

// Storage backend
void smu_cal_store_init(void);
smu_cal_load_result_t smu_cal_store_load(smu_cal_record_t* out);
bool smu_cal_store_save(const smu_cal_record_t* rec);
void smu_cal_store_invalidate_all(void);

#endif
