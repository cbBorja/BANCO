#!/bin/bash

# This script builds and tests the bank application
# with a focus on the balance query communication

# Go to the project root
cd "$(dirname "$0")/.."

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Building banco project ===${NC}"
cd src
gcc -o ../bin/banco banco.c -pthread
gcc -o ../bin/usuario usuario.c -pthread
gcc -o ../bin/fix_eof fix_eof.c
gcc -o ../bin/test_fifo_response test_fifo_response.c
gcc -o ../bin/test_cuenta test_cuenta.c
gcc -o ../bin/check_cuentas check_cuentas.c
gcc -o ../bin/init_cuentas init_cuentas.c

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

echo -e "${GREEN}Build successful!${NC}"
cd ..

# Run initialization if needed
echo -e "${BLUE}=== Checking cuentas.dat file ===${NC}"
if [ ! -f ./data/cuentas.dat ]; then
    echo -e "${YELLOW}cuentas.dat not found, initializing...${NC}"
    ./bin/init_cuentas
else
    echo -e "${GREEN}cuentas.dat exists${NC}"
    ./bin/check_cuentas
fi

# Create test FIFOs
echo -e "${BLUE}=== Creating test FIFOs ===${NC}"
FIFO_DIR="/tmp"
TEST_FIFO_TO_USER="${FIFO_DIR}/test_fifo_to_user"
TEST_FIFO_FROM_USER="${FIFO_DIR}/test_fifo_from_user"

[ -p "$TEST_FIFO_TO_USER" ] || mkfifo "$TEST_FIFO_TO_USER"
[ -p "$TEST_FIFO_FROM_USER" ] || mkfifo "$TEST_FIFO_FROM_USER"

echo -e "${GREEN}Test FIFOs created${NC}"

echo -e "${BLUE}=== Testing direct FIFO communication ===${NC}"
# Start a writer process in the background
echo "Test message from writer" > "$TEST_FIFO_TO_USER" &

# Read from the FIFO
read -t 2 line < "$TEST_FIFO_TO_USER"
if [ $? -eq 0 ]; then
    echo -e "${GREEN}Successfully read from FIFO: '$line'${NC}"
else
    echo -e "${RED}Failed to read from FIFO${NC}"
fi

echo ""
echo -e "${YELLOW}All tests completed. Use these commands to run the application:${NC}"
echo "cd bin"
echo "./banco"
echo "./usuario <account_number> <fifo_to_bank> <fifo_from_bank>"
echo ""
echo -e "${YELLOW}For direct FIFO testing:${NC}"
echo "./fix_eof <fifo_path>"
echo "./test_fifo_response <user_slot> [account_number]"
