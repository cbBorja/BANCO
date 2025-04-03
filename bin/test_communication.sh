#!/bin/bash

# Script para probar la comunicación entre procesos

echo "Creando FIFOs temporales..."
FIFO_A="/tmp/test_fifo_a"
FIFO_B="/tmp/test_fifo_b"

# Eliminar FIFOs existentes
rm -f $FIFO_A $FIFO_B

# Crear nuevos FIFOs
mkfifo $FIFO_A
mkfifo $FIFO_B

echo "FIFOs creados: $FIFO_A y $FIFO_B"
echo "Iniciando prueba de comunicación..."

# Probar escritura y lectura en ambas direcciones
echo "Probando la dirección A -> B..."
(echo "Mensaje de prueba A->B" > $FIFO_A) &
WRITER_PID=$!
read LINE < $FIFO_A
echo "Leído desde A: '$LINE'"
wait $WRITER_PID

echo "Probando la dirección B -> A..."
(echo "Mensaje de prueba B->A" > $FIFO_B) &
WRITER_PID=$!
read LINE < $FIFO_B
echo "Leído desde B: '$LINE'"
wait $WRITER_PID

echo "Limpiando FIFOs temporales..."
rm -f $FIFO_A $FIFO_B

echo "Prueba completada."
