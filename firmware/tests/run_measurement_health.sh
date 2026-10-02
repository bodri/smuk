#!/bin/sh
set -eu
firmware_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -I"$firmware_dir/Drivers/SMU" -I"$firmware_dir/App" -I"$firmware_dir/Storage" \
    "$firmware_dir/tests/test_measurement_health.c" \
    "$firmware_dir/App/smu.c" "$firmware_dir/App/smu_acquisition.c" \
    "$firmware_dir/App/smu_measurement.c" "$firmware_dir/App/smu_range.c" \
    "$firmware_dir/App/smu_calibration.c" "$firmware_dir/App/smu_iforce.c" \
    "$firmware_dir/Drivers/SMU/range_hw.c" "$firmware_dir/Drivers/SMU/safety_hw.c" \
    "$firmware_dir/Storage/calibration_record.c" "$firmware_dir/App/smu_calibration_fit.c" \
    "$firmware_dir/Drivers/SMU/smu_log.c" \
    -lm -o "$test_dir/test_measurement_health"
"$test_dir/test_measurement_health"
