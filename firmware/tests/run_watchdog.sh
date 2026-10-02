#!/bin/sh
set -eu
firmware_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$firmware_dir/App" -I"$firmware_dir/Drivers/SMU" \
    "$firmware_dir/tests/test_watchdog.c" "$firmware_dir/App/smu_watchdog.c" \
    "$firmware_dir/Drivers/SMU/smu_log.c" -o "$test_dir/test_watchdog"
for scenario in normal start_failure unsafe_fault input_fault invalid_state refresh_failure flash_failure; do
    "$test_dir/test_watchdog" "$scenario"
done
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$firmware_dir/tests/watchdog_hal_support" -I"$firmware_dir/App" \
    "$firmware_dir/tests/test_watchdog_port.c" "$firmware_dir/Platform/STM32H503/smu_watchdog_port_hal.c" \
    -o "$test_dir/test_watchdog_port"
"$test_dir/test_watchdog_port"
