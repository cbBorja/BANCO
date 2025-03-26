#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>

#define CONFIG_FILE "../config/config.txt"
#define LOG_FILE "../data/transacciones.log"

typedef struct {
    int limite_retiro;
    int limite_transferencia;
    int umbral_retiros;
    int umbral_transferencias;
    int num_hilos;
    char archivo_cuentas[256];
    char archivo_log[256];
} Config;

Config config;

/* Función para leer la configuración desde el archivo.
   Se espera que cada línea siga el formato clave=valor sin espacios extra. */
void leer_configuracion(const char *filename, Config *config) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error al abrir el archivo de configuración");
        exit(EXIT_FAILURE);
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Eliminar salto de línea
        line[strcspn(line, "\n")] = 0;
        if (strncmp(line, "LIMITE_RETIRO=", 14) == 0) {
            config->limite_retiro = atoi(line + 14);
        } else if (strncmp(line, "LIMITE_TRANSFERENCIA=", 21) == 0) {
            config->limite_transferencia = atoi(line + 21);
        } else if (strncmp(line, "UMBRAL_RETIROS=", 15) == 0) {
            config->umbral_retiros = atoi(line + 15);
        } else if (strncmp(line, "UMBRAL_TRANSFERENCIAS=", 22) == 0) {
            config->umbral_transferencias = atoi(line + 22);
        } else if (strncmp(line, "NUM_HILOS=", 10) == 0) {
            config->num_hilos = atoi(line + 10);
        } else if (strncmp(line, "ARCHIVO_CUENTAS=", 16) == 0) {
            sscanf(line + 16, "%s", config->archivo_cuentas);
        } else if (strncmp(line, "ARCHIVO_LOG=", 12) == 0) {
            sscanf(line + 12, "%s", config->archivo_log);
        }
    }
    fclose(file);
}

int main() {
    // Leer el fichero de configuración.
    leer_configuracion(CONFIG_FILE, &config);

    // Crear un semáforo nombrado para controlar el acceso al archivo de cuentas.
    sem_t *sem = sem_open("/cuentas_semaphore", O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("Error al crear el semáforo");
        exit(EXIT_FAILURE);
    }

    // Abrir el archivo de log. Si en la configuración se especifica otro nombre, se usará.
    const char *log_filename = strlen(config.archivo_log) > 0 ? config.archivo_log : LOG_FILE;
    FILE *log_file = fopen(log_filename, "a");
    if (log_file == NULL) {
        perror("Error al abrir el archivo de log");
        exit(EXIT_FAILURE);
    }

    // Simulación de usuarios: se crea una tubería y se lanza un proceso hijo para cada usuario.
    int num_users = 5; // Número fijo de usuarios para la simulación.
    for (int i = 0; i < num_users; i++) {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("Error al crear la tubería");
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("Error al crear el proceso hijo");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Proceso hijo: cierra el extremo de lectura de la tubería.
            close(pipefd[0]);
            /* Redirige la salida estándar al extremo de escritura de la tubería.
               Así, lo que se imprima en el proceso usuario se enviará al proceso padre. */
            if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                perror("Error al redirigir la salida estándar");
                exit(EXIT_FAILURE);
            }
            close(pipefd[1]);

            // Ejecuta el programa usuario. Se asume que el ejecutable "usuario" se encuentra en el directorio actual.
            execl("./usuario", "usuario", NULL);
            perror("Error al ejecutar el programa usuario");
            exit(EXIT_FAILURE);
        } else {
            // Proceso padre: cierra el extremo de escritura de la tubería.
            close(pipefd[1]);
            char buffer[256];
            ssize_t nbytes;

            // Lee los mensajes enviados por el proceso hijo (usuario).
            while ((nbytes = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
                buffer[nbytes] = '\0';
                // Registra la operación en el archivo de log.

                fprintf(log_file, "Usuario %d: %s", i + 1, buffer);
                fflush(log_file);
            }
            close(pipefd[0]);

            // Espera a que el proceso hijo finalice antes de continuar con el siguiente.
            wait(NULL);
        }
    }

    // Cierre de recursos.
    fclose(log_file);
    sem_close(sem);
    sem_unlink("/cuentas_semaphore");

    return EXIT_SUCCESS;
}
