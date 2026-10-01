#!/bin/bash
set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Dynamically load Homebrew environment variables for Apple Silicon or Intel Macs
if [ -f /opt/homebrew/bin/brew ]; then
    eval "$(/opt/homebrew/bin/brew shellenv)"
elif [ -f /usr/local/bin/brew ]; then
    eval "$(/usr/local/bin/brew shellenv)"
fi

# Fallback explicit paths just in case
export PATH="/opt/homebrew/bin:/opt/homebrew/sbin:/usr/local/bin:$PATH"

# Get the absolute path to the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE}")" && pwd)"
cd "$SCRIPT_DIR"

echo -e "${YELLOW}🔍 Running Code Integrity Analysis...${NC}"

# 1. Formatting & Auto-Correction (clang-format Option 1)
if command -v clang-format &> /dev/null; then
    echo "  -> Auto-formatting layout to match code guidelines..."
    find Core/Src Core/Inc Platform Drivers/SMU Storage -type f \
        \( -name "*.c" -o -name "*.h" \) -print0 | xargs -0 clang-format -i
    echo -e "${GREEN}  [OK] Formatting rules applied successfully.${NC}"
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
             Core/ Platform/ Drivers/SMU/ Storage/
    echo -e "${GREEN}  [OK] Static safety validation checks passed.${NC}"
else
    echo -e "${YELLOW}  [WARN] cppcheck missing. Skipping static analysis.${NC}"
fi

# 3. Proceed to full compilation
BUILD_DIR="$SCRIPT_DIR/build"

# Clear old configuration cache if it exists to avoid stale path errors
if [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}🧹 Clearing stale build cache...${NC}"
    rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles"
fi

mkdir -p "$BUILD_DIR" && cd "$BUILD_DIR"
echo -e "${YELLOW}⚙️  Configuring build environment...${NC}"

# Configure using absolute paths
cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_DIR/arm-none-eabi-gcc.cmake" "$SCRIPT_DIR"
ninja

# 4. Success Check and Verification
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Build Completed Successfully!${NC}"

    # 💾 Trigger Python Validator to check constraints
    python3 "$SCRIPT_DIR/check_limits.py" "$BUILD_DIR/smuk.elf"
else
    echo -e "${RED}❌ Compilation failed.${NC}"
    exit 1
fi
