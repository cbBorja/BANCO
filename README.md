# BANCO
# Limites de Operaciones
LIMITE_RETIRO = 5000
LIMITE_TRANSFERENCIA = 10000
# Umbrales de Detección de Anomalias
UMBRAL_RETIROS = 3
UMBRAL_TRANSFERENCIAS = 5
# Parámetros de Ejecución
NUM_HILOS = 4
ARCHIVO_CUENTAS = cuentas.dat
ARCHIVO_LOG = transacciones.log



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFFER_SIZE 256

int main() {
    char buffer[BUFFER_SIZE];

    while (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
        printf("Usuario recibió: %s", buffer);
        // Aquí se manejarían las solicitudes del usuario y se comunicaría con el proceso padre

        // Simulación de manejo de solicitud
        if (strncmp(buffer, "Operación", 9) == 0) {
            // Enviar respuesta al proceso padre
            int log_fd = open("transacciones.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
            if (log_fd == -1) {
                perror("Error al abrir el archivo de log");
                exit(EXIT_FAILURE);
            }
            write(log_fd, buffer, strlen(buffer));
            close(log_fd);
        }
    }

    return 0;
}