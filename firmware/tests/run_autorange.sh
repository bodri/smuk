#!/bin/sh
set -eu
firmware_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -DADS131M03_SAMPLE_RATE_HZ=4000 -Wall -Wextra -Werror \
    -I"$firmware_dir/Drivers/SMU" -I"$firmware_dir/App" \
    "$firmware_dir/tests/test_autorange.c" \
    "$firmware_dir/App/smu_range.c" \
    "$firmware_dir/App/smu_instrument.c" \
    "$firmware_dir/App/smu_fault.c" \
    "$firmware_dir/App/smu_compliance.c" \
    "$firmware_dir/App/smu_measurement.c" \
    "$firmware_dir/App/smu_iforce.c" \
    "$firmware_dir/Drivers/SMU/range_hw.c" \
    "$firmware_dir/Drivers/SMU/safety_hw.c" \
    -lm -o "$test_dir/test_autorange"
"$test_dir/test_autorange"
