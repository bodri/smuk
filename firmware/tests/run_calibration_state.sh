#!/bin/sh
set -eu
firmware_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -DADS131M03_SAMPLE_RATE_HZ=4000 -Wall -Wextra -Werror \
    -I"$firmware_dir/Drivers/SMU" -I"$firmware_dir/App" -I"$firmware_dir/Storage" \
    "$firmware_dir/tests/test_calibration_state.c" \
    "$firmware_dir/App/smu_cal_seq.c" \
    "$firmware_dir/App/smu_cal_capture.c" \
    "$firmware_dir/App/smu_measurement.c" \
    "$firmware_dir/App/smu_calibration.c" \
    "$firmware_dir/Storage/calibration_record.c" "$firmware_dir/App/smu_calibration_fit.c" \
    -lm -o "$test_dir/test_calibration_state"
"$test_dir/test_calibration_state"
