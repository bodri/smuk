#!/bin/bash

# Exit immediately if any command fails
set -e

# Color definitions for output styling
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

BUILD_DIR="build"
CLEAN_ALL=false

# Check for a clean flag passed to the script (e.g., ./build.sh --clean)
if [ "$1" == "--clean" ] || [ "$1" == "-c" ]; then
    CLEAN_ALL=true
fi

echo -e "${YELLOW}🤖 Starting STM32H5 Automated Compilation Loop...${NC}"

# 1. Handle clean build environments
if [ "$CLEAN_ALL" = true ]; then
    echo -e "${YELLOW}🧹 Wiping out previous build artifacts and CMake caches...${NC}"
    rm -rf "$BUILD_DIR"
fi

# 2. Ensure the build output directory exists
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 3. Generate native build environments using the ARM Toolchain
echo -e "${YELLOW}⚙️  Configuring build environment with CMake toolchain...${NC}"
cmake -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-gcc.cmake ..

# 4. Compile the binaries using multi-threaded execution
echo -e "${YELLOW}🔨 Compiling embedded source tree...${NC}"
# Use nproc on Linux, sysctl on macOS to calculate CPU threads dynamically
if [[ "$OSTYPE" == "darwin"* ]]; then
    THREADS=$(sysctl -n hw.ncpu)
else
    THREADS=$(nproc)
fi

make -j"$THREADS"

# 5. Success Check and Verification
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Build Completed Successfully!${NC}"
    echo -e "${GREEN}📦 Binaries generated in: firmware/build/${NC}"
else
    echo -e "${RED}❌ Compilation failed. Check the syntax/linker log details above.${NC}"
    exit 1
fi
N
