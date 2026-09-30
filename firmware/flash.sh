#!/bin/bash
set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 1. Map paths directly to the official STMicroelectronics bundle installations inside /Applications
CUBE_PROG_DIR=$(ls -d /Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.macos64_* 2>/dev/null | head -n 1)

if [ -z "$CUBE_PROG_DIR" ] || [ ! -d "$CUBE_PROG_DIR" ]; then
    echo -e "${RED}❌ Error: Could not locate STM32CubeProgrammer suite layout within /Applications.${NC}"
    exit 1
fi

# Locate the official command-line programmer binary
CLI_PROGRAMMER="$CUBE_PROG_DIR/tools/bin/STM32_Programmer_CLI"

# Get absolute project root paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE}")" && pwd)"
ELF_PATH="$SCRIPT_DIR/build/smuk.elf"

echo -e "${YELLOW}⚡ Launching production hardware flash via STM32_Programmer_CLI...${NC}"

# 2. Fire the hardware flashing instructions natively using your CubeIDE configurations
# connect via SWD port, use Under Reset mode, perform hardware reset, flash, verify, and run
"$CLI_PROGRAMMER" -c port=SWD mode=UR reset=hwRst \
                  -d "$ELF_PATH" \
                  -v \
                  -rst

echo -e "${GREEN}✅ Flash, Verification, and Boot Reset successful!${NC}"
