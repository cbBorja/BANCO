#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/select.h> 
#include <sys/time.h>
#include <errno.h>

#define CONFIG_FILE "../config/config.txt"
#define LOG_FILE "../data/transacciones.log"
#define MAX_USUARIOS_SIMULTANEOS 10
#define FIFO_BASE_PATH "/tmp/banco_fifo_"

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
int continuar_ejecucion = 1;  // Flag para controlar el bucle principal

// Estructura para mantener información de usuarios activos
typedef struct {
    pid_t pid;               // PID del proceso usuario
    int cuenta;              // Número de cuenta
    int fifo_lectura_fd;     // Descriptor para leer del usuario
    char fifo_lectura[100];  // Ruta al FIFO para leer del usuario
    char fifo_escritura[100]; // Ruta al FIFO para escribir al usuario
} InfoUsuario;

InfoUsuario usuarios[MAX_USUARIOS_SIMULTANEOS];

/* Función para manejar señales y terminar adecuadamente */
void manejador_senales(int sig) {
    printf("\nSeñal recibida (%d). Terminando proceso banco...\n", sig);
    continuar_ejecucion = 0;
}

/* Función para leer la configuración desde el archivo.
   Se espera que cada línea siga el formato clave=valor sin espacios extra. */
void leer_configuracion(const char *filename, Config *cfg) {
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
            cfg->limite_retiro = atoi(line + 14);
        } else if (strncmp(line, "LIMITE_TRANSFERENCIA=", 21) == 0) {
            cfg->limite_transferencia = atoi(line + 21);
        } else if (strncmp(line, "UMBRAL_RETIROS=", 15) == 0) {
            cfg->umbral_retiros = atoi(line + 15);
        } else if (strncmp(line, "UMBRAL_TRANSFERENCIAS=", 22) == 0) {
            cfg->umbral_transferencias = atoi(line + 22);
        } else if (strncmp(line, "NUM_HILOS=", 10) == 0) {
            cfg->num_hilos = atoi(line + 10);
        } else if (strncmp(line, "ARCHIVO_CUENTAS=", 16) == 0) {
            sscanf(line + 16, "%s", cfg->archivo_cuentas);
        } else if (strncmp(line, "ARCHIVO_LOG=", 12) == 0) {
            sscanf(line + 12, "%s", cfg->archivo_log);
        }
    }
    fclose(file);
}

// Función para crear un FIFO con manejo de errores
int crear_fifo(const char *path) {
    if (mkfifo(path, 0666) == -1) {
        if (errno != EEXIST) {
            perror("Error al crear FIFO");
            return -1;
        }
    }
    return 0;
}

// Función para limpiar recursos de un usuario
void limpiar_recursos_usuario(int idx) {
    if (idx < 0 || idx >= MAX_USUARIOS_SIMULTANEOS) return;
    
    if (usuarios[idx].fifo_lectura_fd > 0) {
        close(usuarios[idx].fifo_lectura_fd);
        usuarios[idx].fifo_lectura_fd = 0;
    }
    
    // Eliminar los FIFOs
    if (strlen(usuarios[idx].fifo_lectura) > 0) {
        unlink(usuarios[idx].fifo_lectura);
        usuarios[idx].fifo_lectura[0] = '\0';
    }
    
    if (strlen(usuarios[idx].fifo_escritura) > 0) {
        unlink(usuarios[idx].fifo_escritura);
        usuarios[idx].fifo_escritura[0] = '\0';
    }
    
    usuarios[idx].pid = 0;
    usuarios[idx].cuenta = 0;
}

int main() {
    // Inicializar array de usuarios
    for (int i = 0; i < MAX_USUARIOS_SIMULTANEOS; i++) {
        usuarios[i].pid = 0;
        usuarios[i].cuenta = 0;
        usuarios[i].fifo_lectura_fd = 0;
        usuarios[i].fifo_lectura[0] = '\0';
        usuarios[i].fifo_escritura[0] = '\0';
    }

    // Leer el fichero de configuración.
    leer_configuracion(CONFIG_FILE, &config);

    // Configuración de manejadores de señales para terminación adecuada
    signal(SIGINT, manejador_senales);
    signal(SIGTERM, manejador_senales);

    // Crear un semáforo nombrado para controlar el acceso al archivo de cuentas.
    sem_t *sem = sem_open("/cuentas_semaphore", O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("Error al crear el semáforo");
        exit(EXIT_FAILURE);
    }

    // Abrir el archivo de log.
    const char *log_filename = strlen(config.archivo_log) > 0 ? config.archivo_log : LOG_FILE;
    FILE *log_file = fopen(log_filename, "a");
    if (log_file == NULL) {
        perror("Error al abrir el archivo de log");
        exit(EXIT_FAILURE);
    }

    printf("Banco iniciado. Esperando conexiones de usuario...\n");
    printf("Presione Ctrl+C para terminar.\n\n");

    // Bucle principal para aceptar conexiones de usuarios
    while (continuar_ejecucion) {
        int cuenta_usuario;
        int slot_disponible = -1;

        // Buscar un slot disponible para un nuevo usuario
        for (int i = 0; i < MAX_USUARIOS_SIMULTANEOS; i++) {
            if (usuarios[i].pid == 0) {
                slot_disponible = i;
                break;
            }
        }

        if (slot_disponible == -1) {
            printf("Se ha alcanzado el límite de usuarios simultáneos. Espere...\n");
            sleep(1);
            continue;
        }

        // Preguntar por el número de cuenta
        printf("Ingrese el número de cuenta (o 0 para salir): ");
        if (scanf("%d", &cuenta_usuario) != 1) {
            printf("Entrada inválida. Intente de nuevo.\n");
            while (getchar() != '\n'); // Limpiar buffer
            continue;
        }

        if (cuenta_usuario == 0) {
            printf("Solicitud de cierre recibida.\n");
            continuar_ejecucion = 0;
            continue;
        }

        // Crear dos FIFOs para este usuario: banco->usuario y usuario->banco
        char fifo_to_usuario[100], fifo_from_usuario[100];
        sprintf(fifo_to_usuario, "%s%d_to_user", FIFO_BASE_PATH, slot_disponible);
        sprintf(fifo_from_usuario, "%s%d_from_user", FIFO_BASE_PATH, slot_disponible);
        
        // Guardar las rutas de los FIFOs en la estructura del usuario
        strcpy(usuarios[slot_disponible].fifo_escritura, fifo_to_usuario);
        strcpy(usuarios[slot_disponible].fifo_lectura, fifo_from_usuario);
        
        // Crear los FIFOs
        if (crear_fifo(fifo_to_usuario) < 0 || crear_fifo(fifo_from_usuario) < 0) {
            fprintf(stderr, "Error al crear FIFOs para el usuario %d\n", cuenta_usuario);
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("Error al crear el proceso hijo");
            continue;
        } else if (pid == 0) {
            // Proceso hijo
            
            // Convertir cuenta_usuario a string y preparar argumentos
            char cuenta_str[20];
            sprintf(cuenta_str, "%d", cuenta_usuario);
            
            // Crear el comando para ejecutar en la nueva terminal
            char command[512];
            sprintf(command, "xterm -T \"Usuario Banco - Cuenta %d\" -e \"./usuario %s %s %s\"",
                    cuenta_usuario, cuenta_str, fifo_from_usuario, fifo_to_usuario);
                    
            // Si xterm no está disponible, intentar con gnome-terminal
            if (system("which xterm > /dev/null 2>&1") != 0) {
                sprintf(command, "gnome-terminal -- ./usuario %s %s %s",
                        cuenta_str, fifo_from_usuario, fifo_to_usuario);
            }
            
            // Ejecutar el comando
            system(command);
            exit(EXIT_SUCCESS);
        } else {
            // Proceso padre
            usuarios[slot_disponible].pid = pid;
            usuarios[slot_disponible].cuenta = cuenta_usuario;
            
            // Abrir el FIFO para lectura (mensajes del usuario)
            usuarios[slot_disponible].fifo_lectura_fd = open(fifo_from_usuario, O_RDONLY | O_NONBLOCK);
            if (usuarios[slot_disponible].fifo_lectura_fd < 0) {
                perror("Error al abrir FIFO para lectura");
                limpiar_recursos_usuario(slot_disponible);
                continue;
            }
            
            // También abrimos el FIFO para escritura, pero no guardamos el descriptor
            // porque lo abriremos cada vez que necesitemos escribir
            int fifo_escritura_fd = open(fifo_to_usuario, O_WRONLY | O_NONBLOCK);
            if (fifo_escritura_fd < 0) {
                perror("Error al abrir FIFO para escritura");
                limpiar_recursos_usuario(slot_disponible);
                continue;
            }
            close(fifo_escritura_fd); // Lo cerraremos y abriremos según necesidad
            
            printf("Usuario con cuenta %d conectado (PID: %d)\n", cuenta_usuario, pid);
            fprintf(log_file, "Usuario conectado: Cuenta %d (PID: %d)\n", cuenta_usuario, pid);
            fflush(log_file);
        }

        // Verificar si hay procesos hijo que han terminado
        for (int i = 0; i < MAX_USUARIOS_SIMULTANEOS; i++) {
            if (usuarios[i].pid > 0) {
                int status;
                pid_t result = waitpid(usuarios[i].pid, &status, WNOHANG);
                
                if (result > 0) { // El proceso ha terminado
                    printf("Usuario (PID: %d) desconectado.\n", usuarios[i].pid);
                    fprintf(log_file, "Usuario desconectado: PID %d\n", usuarios[i].pid);
                    fflush(log_file);
                    
                    limpiar_recursos_usuario(i);
                }
                else if (result < 0) {
                    perror("Error en waitpid");
                }
            }
        }

        // Procesar los mensajes de los usuarios activos (no bloqueante)
        for (int i = 0; i < MAX_USUARIOS_SIMULTANEOS; i++) {
            if (usuarios[i].fifo_lectura_fd > 0) {
                char buffer[256];
                fd_set set;
                struct timeval timeout;
                
                FD_ZERO(&set);
                FD_SET(usuarios[i].fifo_lectura_fd, &set);
                
                timeout.tv_sec = 0;
                timeout.tv_usec = 1000; // 1ms timeout para no bloquearse
                
                int ready = select(usuarios[i].fifo_lectura_fd + 1, &set, NULL, NULL, &timeout);
                
                if (ready > 0 && FD_ISSET(usuarios[i].fifo_lectura_fd, &set)) {
                    ssize_t nbytes = read(usuarios[i].fifo_lectura_fd, buffer, sizeof(buffer) - 1);
                    
                    if (nbytes > 0) {
                        buffer[nbytes] = '\0';
                        fprintf(log_file, "Usuario (Cuenta %d): %s", usuarios[i].cuenta, buffer);
                        fflush(log_file);
                        printf("Mensaje de usuario %d (Cuenta %d): %s", 
                               i, usuarios[i].cuenta, buffer);
                    }
                    else if (nbytes == 0) { // EOF - el FIFO se cerró
                        close(usuarios[i].fifo_lectura_fd);
                        usuarios[i].fifo_lectura_fd = 0;
                    }
                }
            }
        }
        
        // Pequeña pausa para no saturar la CPU
        usleep(100000); // 100ms
    }

    // Esperar a que todos los procesos hijos terminen
    printf("Finalizando todos los procesos de usuario...\n");
    for (int i = 0; i < MAX_USUARIOS_SIMULTANEOS; i++) {
        if (usuarios[i].pid > 0) {
            kill(usuarios[i].pid, SIGTERM);
            waitpid(usuarios[i].pid, NULL, 0);
            limpiar_recursos_usuario(i);
        }
    }

    // Cierre de recursos.
    fclose(log_file);
    sem_close(sem);
    sem_unlink("/cuentas_semaphore");

    printf("Proceso del banco finalizado correctamente.\n");
    return EXIT_SUCCESS;
}
