#!/bin/bash

# Colors for terminal output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}====================================================${NC}"
echo -e "${BLUE}        MONITOR DE COMUNICACIÓN BANCO-USUARIO        ${NC}"
echo -e "${BLUE}====================================================${NC}"

# Check for FIFO files
echo -e "\n${YELLOW}Comprobando FIFOs activos:${NC}"
ls -la /tmp/banco_fifo_* 2>/dev/null

if [ $? -ne 0 ]; then
    echo -e "${RED}No se encontraron FIFOs activos${NC}"
fi

# Monitor active processes
echo -e "\n${YELLOW}Procesos de banco activos:${NC}"
ps aux | grep -E "banco|usuario" | grep -v "grep" | grep -v "monitor"

# Check log file
echo -e "\n${YELLOW}Últimas líneas del log de transacciones:${NC}"
tail -n 10 ../data/transacciones.log

# Show current database accounts
echo -e "\n${YELLOW}Cuentas activas en la base de datos:${NC}"
../bin/check_cuentas 2>/dev/null

if [ $? -ne 0 ]; then
    echo -e "${RED}No se pudo leer la base de datos de cuentas${NC}"
    echo -e "Ejecute: cd ../bin && ./init_cuentas\n"
fi

# Interactive monitoring mode
echo -e "\n${GREEN}Iniciando monitorización interactiva (Ctrl+C para salir):${NC}"
echo -e "${YELLOW}Monitorizando mensajes del banco...${NC}\n"

tail -f ../data/transacciones.log | grep --color=always -E 'SALDO|ERROR|conectado|desconectado'
