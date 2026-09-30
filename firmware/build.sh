#!/bin/bash
set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}🔍 Running Code Integrity Analysis...${NC}"

# 1. Formatting Check (clang-format)
if command -v clang-format &> /dev/null; then
    echo "  -> Verifying code formatting parameters..."
    # Find application files while ignoring vendor drivers
    find Core/Src Core/Inc -name "*.c" -o -name "*.h" | xargs clang-format --dry-run --Werror
    echo -e "${GREEN}  [OK] Formatting rules passed.${NC}"
else
    echo -e "${YELLOW}  [WARN] clang-format missing. Skipping styling validation.${NC}"
fi

# 2. Static Code Analysis (cppcheck)
if command -v cppcheck &> /dev/null; then
    echo "  -> Scanning for embedded hardware memory bugs..."
    cppcheck --enable=warning,performance,portability \
             --error-exitcode=1 \
             --inline-suppr \
             --suppressions-list=.cppcheck_ignore \
             Core/
    echo -e "${GREEN}  [OK] Static safety validation checks passed.${NC}"
else
    echo -e "${YELLOW}  [WARN] cppcheck missing. Skipping static analysis.${NC}"
fi

# 3. Proceed to full compilation
BUILD_DIR="build"
mkdir -p "$BUILD_DIR" && cd "$BUILD_DIR"
echo -e "${YELLOW}⚙️  Configuring build environment...${NC}"
cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-gcc.cmake ..
ninja

# 4. Success Check and Verification
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Build Completed Successfully!${NC}"

    # 💾 Trigger Python Validator to check constraints
    python3 ./check_limits.py "build/stm32h5-firmware.elf"
else
    echo -e "${RED}❌ Compilation failed.${NC}"
    exit 1
fi
