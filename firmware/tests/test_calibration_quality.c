#include "smu_cal_capture.h"
#include "smu_calibration.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static smu_cal_record_t stored;
static smu_cal_load_result_t load_kind;
static bool save_failure, readback_failure;
static unsigned writes, begins, ends;
static bool reject_begin;

static bool begin_save(void) {
    ++begins;
    return !reject_begin;
}

static void end_save(void) {
    ++ends;
}

smu_cal_load_result_t smu_cal_store_load(smu_cal_record_t* out) {
    *out = stored;
    if (readback_failure)
        return SMU_CAL_LOAD_INVALID;
    return load_kind;
}

bool smu_cal_store_save(const smu_cal_record_t* candidate) {
    ++writes;
    if (save_failure)
        return false;
    stored = *candidate;
    ++stored.sequence;
    smu_cal_record_finalize(&stored);
    load_kind = SMU_CAL_LOAD_OK;
    return true;
}

static void test_record_validation(void) {
    smu_cal_record_t good;
    smu_cal_record_defaults(&good);
    assert(good.version == 1 && smu_cal_record_validate(&good));
    smu_cal_record_t bad = good;
    bad.sequence ^= 1;
    assert(!smu_cal_record_validate(&bad));
    const float invalid[] = {NAN, INFINITY, -1, 0};
    for (unsigned i = 0; i < 4; ++i) {
        bad = good;
        bad.measurement.current[3].gain = invalid[i];
        smu_cal_record_finalize(&bad);
        assert(!smu_cal_record_validate(&bad));
        bad = good;
        bad.iforce[2].gain = invalid[i];
        smu_cal_record_finalize(&bad);
        assert(!smu_cal_record_validate(&bad));
    }
    bad = good;
    bad.measurement.voltage[1].offset = NAN;
    smu_cal_record_finalize(&bad);
    assert(!smu_cal_record_validate(&bad));
    bad = good;
    bad.vforce.offset = INFINITY;
    smu_cal_record_finalize(&bad);
    assert(!smu_cal_record_validate(&bad));
    bad = good;
    bad.vforce.gain = 1e-40f;
    smu_cal_record_finalize(&bad);
    assert(!smu_cal_record_validate(&bad));
    bad = good;
    bad.measurement.voltage[0].gain = 1e38f;
    smu_cal_record_finalize(&bad);
    assert(!smu_cal_record_validate(&bad));
    bad = good;
    bad.measurement.voltage[0].gain = 1.05f;
    smu_cal_record_finalize(&bad);
    assert(smu_cal_record_validate(&bad));
}

static void test_provenance_and_commit(void) {
    smu_cal_record_defaults(&stored);
    load_kind = SMU_CAL_LOAD_DEFAULTS;
    smu_calibration_init();
    assert(smu_calibration_load_result() == SMU_CAL_LOAD_DEFAULTS);
    stored.measurement.voltage[0].gain = NAN;
    smu_cal_record_finalize(&stored);
    load_kind = SMU_CAL_LOAD_OK;
    smu_calibration_init();
    assert(smu_calibration_load_result() == SMU_CAL_LOAD_DEFAULTS);
    assert(smu_calibration_get()->measurement.voltage[0].gain == 1);
    smu_cal_record_defaults(&stored);
    stored.sequence = 7;
    smu_cal_record_finalize(&stored);
    smu_calibration_init();
    assert(smu_calibration_load_result() == SMU_CAL_LOAD_OK);
    smu_cal_record_t candidate = *smu_calibration_get();
    candidate.measurement.voltage[0].gain = -1;
    assert(!smu_calibration_commit(&candidate) && writes == 0);
    candidate = *smu_calibration_get();
    candidate.measurement.voltage[0].gain = 1.1f;
    smu_calibration_set_save_hooks(begin_save, end_save);
    reject_begin = true;
    assert(!smu_calibration_commit(&candidate) && begins == 1 && ends == 0 && writes == 0);
    reject_begin = false;
    save_failure = true;
    assert(!smu_calibration_commit(&candidate));
    assert(smu_calibration_get()->measurement.voltage[0].gain == 1);
    save_failure = false;
    readback_failure = true;
    assert(!smu_calibration_commit(&candidate));
    assert(smu_calibration_get()->sequence == 7);
    readback_failure = false;
    assert(smu_calibration_commit(&candidate));
    assert(smu_calibration_get()->measurement.voltage[0].gain == 1.1f);
    assert(begins == 4 && ends == 3);
}

static void test_shared_fit(void) {
    const volatile float x[] = {0, 1, 2}, y[] = {0.1f, 1.2f, 2.3f};
    float gain = 77, offset = 88, residual[3] = {99, 99, 99};
    assert(smu_calibration_vcal_fit(x, y, &gain, &offset, residual));
    assert(fabsf(gain - 1.1f) < 1e-6f && fabsf(offset - 0.1f) < 1e-6f);
    const volatile float negative[] = {2, 1, 0}, nan[] = {0, NAN, 2}, repeated[] = {1, 1, 1};
    gain = 77;
    offset = 88;
    residual[0] = 99;
    assert(!smu_calibration_vcal_fit(x, negative, &gain, &offset, residual));
    assert(!smu_calibration_vcal_fit(x, nan, &gain, &offset, residual));
    assert(!smu_calibration_vcal_fit(repeated, y, &gain, &offset, residual));
    assert(gain == 77 && offset == 88 && residual[0] == 99);
}

static void test_capture_statistics(void) {
    smu_cal_capture_t capture;
    smu_cal_capture_quality_t q;
    smu_cal_capture_config_t cfg = smu_cal_capture_default_config();
    assert(!smu_cal_capture_init(&capture, 1025));
    assert(smu_cal_capture_init(&capture, 256));
    for (unsigned i = 0; i < 256; ++i)
        assert(smu_cal_capture_add(&capture, 7000000 + (i % 2 ? 2 : -2)));
    assert(smu_cal_capture_finish(&capture, &cfg, &q));
    assert(q.mean_code == 7000000 && q.noise_codes == 2 && q.drift_codes == 0 && q.span_codes == 4);
    assert(smu_cal_capture_init(&capture, 256));
    for (unsigned i = 0; i < 256; ++i)
        assert(smu_cal_capture_add(&capture, i % 2 ? 100000 : -100000));
    assert(!smu_cal_capture_finish(&capture, &cfg, &q) && q.noise_codes > cfg.max_noise_codes);
    assert(smu_cal_capture_init(&capture, 256));
    cfg.max_noise_codes = 1e6;
    for (unsigned i = 0; i < 256; ++i)
        assert(smu_cal_capture_add(&capture, i < 128 ? 1000000 : 1020000));
    assert(!smu_cal_capture_finish(&capture, &cfg, &q) && q.drift_codes == 20000);
    assert(smu_cal_capture_init(&capture, 256));
    assert(!smu_cal_capture_add(&capture, 8220000));
    assert(!smu_cal_capture_add(&capture, -8220000));
    assert(!smu_cal_capture_finish(&capture, &cfg, &q));
}

int main(void) {
    test_record_validation();
    test_provenance_and_commit();
    test_shared_fit();
    test_capture_statistics();
    puts("Calibration semantics, provenance, fitting and capture-quality tests passed.");
}
