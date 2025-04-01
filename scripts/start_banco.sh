#!/bin/bash

# Directorio base
BASE_DIR="/home/admin/PracticaFinal/BANCO"
BIN_DIR="$BASE_DIR/bin"
DATA_DIR="$BASE_DIR/data"
CONFIG_DIR="$BASE_DIR/config"

# Verificar que los directorios existen
mkdir -p "$DATA_DIR"
mkdir -p "$BIN_DIR"
mkdir -p "$CONFIG_DIR"

echo "=== Sistema Bancario - Inicialización ==="

# Verificar si existe cuentas.dat
if [ ! -f "$DATA_DIR/cuentas.dat" ]; then
    echo "Inicializando cuentas..."
    cd "$BIN_DIR" && ./init_cuentas
else
    echo "El archivo de cuentas ya existe."
fi

# Limpiar FIFOs antiguos
echo "Limpiando FIFOs antiguos..."
rm -f /tmp/banco_fifo_*

# Iniciar banco
echo "Iniciando el proceso banco..."
cd "$BIN_DIR" && ./banco

echo "Fin del script de inicialización."
