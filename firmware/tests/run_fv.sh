#!/bin/sh
set -eu
firmware_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$firmware_dir/App" -I"$firmware_dir/Drivers/SMU" -I"$firmware_dir/Storage" \
    "$firmware_dir/tests/test_fv.c" "$firmware_dir/App/smu_fv.c" -lm -o "$test_dir/test_fv"
"$test_dir/test_fv"
