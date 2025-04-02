#!/bin/bash

# Colors for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Function to check a specific path
check_file() {
  local path=$1
  local status="NO EXISTE"
  local permissions=""
  
  if [ -f "$path" ]; then
    status="EXISTE"
    permissions=$(stat -c "%A" "$path" 2>/dev/null)
    size=$(stat -c "%s" "$path" 2>/dev/null)
    
    echo -e "${BLUE}┌─────────────────────────────────────────────────────────┐${NC}"
    echo -e "${BLUE}│${NC} ${GREEN}✓${NC} ${CYAN}$path${NC}"
    echo -e "${BLUE}├─────────────────────────────────────────────────────────┤${NC}"
    echo -e "${BLUE}│${NC} Estado: ${GREEN}$status${NC}"
    echo -e "${BLUE}│${NC} Permisos: $permissions"
    echo -e "${BLUE}│${NC} Tamaño: $size bytes"
    
    # Check if it's a valid cuentas.dat file
    if [ "$size" -gt 0 ]; then
      accounts=$(($size / 62)) # 62 bytes per account record
      if [ "$accounts" -gt 0 ]; then
        echo -e "${BLUE}│${NC} Cuentas aproximadas: $accounts"
        echo -e "${BLUE}│${NC} Validación: ${GREEN}ARCHIVO VÁLIDO${NC}"
      else
        echo -e "${BLUE}│${NC} Validación: ${RED}ARCHIVO VACÍO O DAÑADO${NC}"
      fi
    else
      echo -e "${BLUE}│${NC} Validación: ${RED}ARCHIVO VACÍO${NC}"
    fi
    
    echo -e "${BLUE}└─────────────────────────────────────────────────────────┘${NC}"
  else
    echo -e "${BLUE}┌─────────────────────────────────────────────────────────┐${NC}"
    echo -e "${BLUE}│${NC} ${RED}✗${NC} ${CYAN}$path${NC}"
    echo -e "${BLUE}├─────────────────────────────────────────────────────────┤${NC}"
    echo -e "${BLUE}│${NC} Estado: ${RED}$status${NC}"
    echo -e "${BLUE}│${NC} Validación: ${RED}ARCHIVO NO ENCONTRADO${NC}"
    echo -e "${BLUE}└─────────────────────────────────────────────────────────┘${NC}"
  fi
  
  echo ""
}

# Title
echo -e "${MAGENTA}=======================================================${NC}"
echo -e "${MAGENTA}     VERIFICACIÓN DE BASE DE DATOS BANCO - CUENTAS     ${NC}"
echo -e "${MAGENTA}=======================================================${NC}"
echo ""

# Check standard paths
check_file "../data/cuentas.dat"
check_file "./data/cuentas.dat"
check_file "/home/admin/PracticaFinal/BANCO/data/cuentas.dat"

# Check if there's a config-specified path
config_file="../config/config.txt"
if [ -f "$config_file" ]; then
  custom_path=$(grep "ARCHIVO_CUENTAS" "$config_file" | cut -d= -f2)
  if [ -n "$custom_path" ]; then
    echo -e "${YELLOW}Comprobando ruta especificada en config: $custom_path${NC}"
    check_file "$custom_path"
  fi
fi

echo -e "${YELLOW}Si necesita crear un archivo de base de datos, ejecute:${NC}"
echo -e "  cd ../bin && ./init_cuentas"
echo ""
