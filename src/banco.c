#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>

#define CONFIG_FILE "config/config.txt"
#define LOG_FILE "transacciones.log"
#define CUENTAS_FILE "cuentas.dat"

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

void leer_configuracion(const char *filename, Config *config) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error al abrir el archivo de configuración");
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "LIMITE_RETIRO = %d", &config->limite_retiro) == 1) continue;
        if (sscanf(line, "LIMITE_TRANSFERENCIA = %d", &config->limite_transferencia) == 1) continue;
        if (sscanf(line, "UMBRAL_RETIROS = %d", &config->umbral_retiros) == 1) continue;
        if (sscanf(line, "UMBRAL_TRANSFERENCIAS = %d", &config->umbral_transferencias) == 1) continue;
        if (sscanf(line, "NUM_HILOS = %d", &config->num_hilos) == 1) continue;
        if (sscanf(line, "ARCHIVO_CUENTAS = %s", config->archivo_cuentas) == 1) continue;
        if (sscanf(line, "ARCHIVO_LOG = %s", config->archivo_log) == 1) continue;
    }

    fclose(file);
}

int main() {
    leer_configuracion(CONFIG_FILE, &config);

    sem_t *sem = sem_open("/cuentas_semaphore", O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("Error al crear el semáforo");
        exit(EXIT_FAILURE);
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("Error al crear la tubería");
        exit(EXIT_FAILURE);
    }

    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file == NULL) {
        perror("Error al abrir el archivo de log");
        exit(EXIT_FAILURE);
    }

    int num_users = 5; // Simulación de número fijo de usuarios
    for (int i = 0; i < num_users; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("Error al crear el proceso hijo");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Código del proceso hijo
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            execl("usuario", "usuario", NULL);
            perror("Error al ejecutar usuario.c");
            exit(EXIT_FAILURE);
        } else {
            // Código del proceso padre
            close(pipefd[0]);
            // Simulación de operaciones de usuario
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "Operación del usuario %d\n", i + 1);
            write(pipefd[1], buffer, strlen(buffer));
            wait(NULL);
        }
    }

    fclose(log_file);
    sem_close(sem);
    sem_unlink("/cuentas_semaphore");

    return 0;
}
