#!/bin/sh
set -eu
firmware_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -I"$firmware_dir/Drivers/SMU" -I"$firmware_dir/App" \
    "$firmware_dir/tests/test_calibration_state.c" \
    "$firmware_dir/Drivers/SMU/smu_cal_seq.c" \
    "$firmware_dir/Drivers/SMU/smu_measurement.c" \
    "$firmware_dir/Drivers/SMU/smu_calibration.c" \
    -lm -o "$test_dir/test_calibration_state"
"$test_dir/test_calibration_state"
