#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}🛰️  Locating ST-LINK GDB Server binaries...${NC}"

# 1. Resolve absolute paths to both the server plugin and programmer plugin bundles
GDB_SERVER_DIR=$(ls -d /Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.stlink-gdb-server.macos64_* 2>/dev/null | head -n 1)
CUBE_PROG_DIR=$(ls -d /Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.macos64_* 2>/dev/null | head -n 1)

# Safety Validation Check
if [ -z "$GDB_SERVER_DIR" ] || [ -z "$CUBE_PROG_DIR" ]; then
    echo -e "${RED}❌ Error: Could not locate internal GDB Server files inside /Applications.${NC}"
    exit 1
fi

SERVER_BIN="$GDB_SERVER_DIR/tools/bin/ST-LINK_gdbserver"
PROG_BIN_DIR="$CUBE_PROG_DIR/tools/bin"

echo -e "${GREEN}✅ Binaries mapped successfully.${NC}"
echo -e "${YELLOW}🔌 Starting background ST-LINK GDB Server listener on Port 61234...${NC}"
echo "-------------------------------------------------------------------"

# 2. Run the official server mapping your parameters directly
"$SERVER_BIN" -p 61234 -l 1 -d -s -cp "$PROG_BIN_DIR" -m 1 -k
