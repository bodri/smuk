#!/bin/sh
set -eu
firmware_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -I"$firmware_dir/Drivers/SMU" -I"$firmware_dir/App" -I"$firmware_dir/Storage" \
    "$firmware_dir/tests/test_console.c" "$firmware_dir/App/smu_console.c" \
    "$firmware_dir/App/smu_watchdog.c" \
    "$firmware_dir/Drivers/SMU/smu_log.c" \
    "$firmware_dir/App/smu_cal_capture.c" \
    "$firmware_dir/App/smu_measurement.c" "$firmware_dir/App/smu_calibration_fit.c" -lm -o "$test_dir/test_console"
"$test_dir/test_console"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -I"$firmware_dir/tests/console_hal_support" -I"$firmware_dir/App" -I"$firmware_dir/Drivers/SMU" \
    "$firmware_dir/Drivers/SMU/smu_log.c" \
    "$firmware_dir/tests/test_console_port.c" \
    "$firmware_dir/Platform/STM32H503/smu_console_port_hal.c" \
    -o "$test_dir/test_console_port"
"$test_dir/test_console_port"
