#!/bin/sh
set -eu
firmware_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$firmware_dir/Drivers/SMU" \
    "$firmware_dir/tests/test_log.c" "$firmware_dir/Drivers/SMU/smu_log.c" -o "$test_dir/test_log"
"$test_dir/test_log"
