#!/bin/sh
set -eu
firmware_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -I"$firmware_dir/Drivers/SMU" -I"$firmware_dir/App" \
    "$firmware_dir/tests/test_precision.c" "$firmware_dir/App/smu_measurement.c" \
    -lm -o "$test_dir/test_precision"
"$test_dir/test_precision"
