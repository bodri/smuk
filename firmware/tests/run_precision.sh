#!/bin/sh
set -eu
firmware_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
for rate in 4000 32000; do
"${CC:-cc}" -std=c11 -O2 -DADS131M03_SAMPLE_RATE_HZ="$rate" -Wall -Wextra -Werror \
    -I"$firmware_dir/Drivers/SMU" -I"$firmware_dir/App" \
    "$firmware_dir/tests/test_precision.c" "$firmware_dir/App/smu_measurement.c" \
    -lm -o "$test_dir/test_precision"
"$test_dir/test_precision"
done
