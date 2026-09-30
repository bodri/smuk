#!/usr/bin/env python3
import sys
import subprocess
import os

# ---------------------------------------------------------
# 🎛️ Hard Hardware Allocation Constraints
# ---------------------------------------------------------
MAX_FLASH_BYTES = 112 * 1024  # 112 KB limit (Rest reserved for calibration)

# Terminal coloration formatting
GREEN = '\033[0;32m'
RED = '\033[0;31m'
YELLOW = '\033[1;33m'
NC = '\033[0m'

def get_memory_sizes(elf_file):
    if not os.path.exists(elf_file):
        print(f"{RED}❌ Error: Target file '{elf_file}' not found.{NC}")
        sys.exit(1)

    try:
        # Run arm-none-eabi-size to capture sector dimensions
        output = subprocess.check_output(['arm-none-eabi-size', elf_file]).decode('utf-8')
        lines = output.strip().split('\n')
        if len(lines) < 2:
            raise ValueError("Malformed output format from size utility.")

        # Parse numbers from the second row
        data_fields = lines[1].split()
        text_size = int(data_fields[0])
        data_size = int(data_fields[1])
        bss_size = int(data_fields[2])

        return text_size, data_size, bss_size
    except Exception as e:
        print(f"{RED}❌ Failed to execute arm-none-eabi-size parsing: {e}{NC}")
        sys.exit(1)

def print_progress_bar(percentage):
    bar_length = 30
    filled_length = int(round(bar_length * (percentage / 100)))
    if filled_length > bar_length:
        filled_length = bar_length

    color = GREEN
    if percentage >= 95.0:
        color = RED
    elif percentage >= 80.0:
        color = YELLOW

    bar = '█' * filled_length + '-' * (bar_length - filled_length)
    return f"{color}[{bar}] {percentage:.2f}%{NC}"

if __name__ == '__main__':
    # Default to the expected build output path if no arg provided
    target_elf = sys.argv[1] if len(sys.argv) > 1 else "build/stm32h5-firmware.elf"

    text, data, bss = get_memory_sizes(target_elf)
    total_flash_used = text + data

    flash_percentage = (total_flash_used / MAX_FLASH_BYTES) * 100

    print(f"\n{YELLOW}📊 STM32H5 Flash Ceiling Analysis:{NC}")
    print(f"  Ceiling Limit   : {MAX_FLASH_BYTES / 1024:.1f} KB (Protected Layout)")
    print(f"  Flash Footprint : {total_flash_used} bytes ({total_flash_used / 1024:.2f} KB)")
    print(f"  Allocation Bar  : {print_progress_bar(flash_percentage)}")
    print(f"  ↳ Sector Weight: Code (.text) = {text} B | Init Globals (.data) = {data} B\n")

    # Assert constraints enforcement
    if total_flash_used > MAX_FLASH_BYTES:
        overflow = total_flash_used - MAX_FLASH_BYTES
        print(f"{RED}🚨 HARDWARE MEMORY VIOLATION!{NC}")
        print(f"{RED}❌ Code exceeds constraints by {overflow / 1024:.2f} KB!{NC}")
        print(f"{RED}⚠️  Compilation aborted to protect memory calibration zones.{NC}\n")
        sys.exit(1)

    print(f"{GREEN}✅ Memory limits within safe margins.{NC}\n")
    sys.exit(0)
