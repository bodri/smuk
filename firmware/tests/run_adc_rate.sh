#!/bin/sh
set -eu
firmware_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
for rate in 4000 32000; do
    "${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -DADS131M03_SAMPLE_RATE_HZ="$rate" \
        -I"$firmware_dir/Drivers/SMU" -I"$firmware_dir/App" \
        "$firmware_dir/tests/test_adc_rate.c" "$firmware_dir/Drivers/SMU/ads131m03.c" \
        "$firmware_dir/Drivers/SMU/smu_log.c" "$firmware_dir/App/smu_acquisition.c" \
        "$firmware_dir/App/smu_cal_capture.c" "$firmware_dir/App/smu_range.c" \
        "$firmware_dir/App/smu_measurement.c" "$firmware_dir/App/smu_iforce.c" \
        "$firmware_dir/Drivers/SMU/range_hw.c" "$firmware_dir/Drivers/SMU/safety_hw.c" \
        -lm -o "$test_dir/test_adc_rate"
    "$test_dir/test_adc_rate"
done
