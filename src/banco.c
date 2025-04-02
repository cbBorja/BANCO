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
#include <stdarg.h>
#include <time.h>

#define CONFIG_FILE "../config/config.txt"
#define LOG_FILE "../data/transacciones.log"
#define MAX_USUARIOS_SIMULTANEOS 10
#define FIFO_BASE_PATH "/tmp/banco_fifo_"
#define BUFFER_SIZE 256

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

// Forward declarations for all functions
void manejador_senales(int sig);
void leer_configuracion(const char *filename, Config *cfg);
int crear_fifo(const char *path);
void limpiar_recursos_usuario(int idx);
double obtener_saldo_cuenta(int cuenta);
void procesar_consulta_saldo(int cuenta, int fifo_escritura_fd);

// Debug function to log with timestamp
void debug_log(const char *format, ...) {
    char timestamp[30];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    va_list args;
    va_start(args, format);
    
    printf("[BANCO %s] ", timestamp);
    vprintf(format, args);
    printf("\n");
    fflush(stdout);
    
    va_end(args);
}

// Estructura para mantener información de usuarios activos
typedef struct {
    pid_t pid;               // PID del proceso usuario
    int cuenta;              // Número de cuenta
    int fifo_lectura_fd;     // Descriptor para leer del usuario
    char fifo_lectura[100];  // Ruta al FIFO para leer del usuario
    char fifo_escritura[100]; // Ruta al FIFO para escribir al usuario
} InfoUsuario;

InfoUsuario usuarios[MAX_USUARIOS_SIMULTANEOS];

// Add this structure to track FIFO connections
typedef struct {
    int usuario_slot;        // Slot del usuario asociado
    int fd;                  // Descriptor de archivo
    int is_open;             // Bandera para saber si está abierto
    char path[256];          // Ruta al FIFO
} FifoConnection;

// Keep track of all open FIFO connections to users
FifoConnection fifo_connections[MAX_USUARIOS_SIMULTANEOS];

// Initialize FIFO connections
void init_fifo_connections() {
    for (int i = 0; i < MAX_USUARIOS_SIMULTANEOS; i++) {
        fifo_connections[i].usuario_slot = -1;
        fifo_connections[i].fd = -1;
        fifo_connections[i].is_open = 0;
        memset(fifo_connections[i].path, 0, sizeof(fifo_connections[i].path));
    }
}

// Function to get or create a FIFO connection to send responses
int get_fifo_connection(int usuario_slot, const char *path) {
    // First check if we already have this connection
    for (int i = 0; i < MAX_USUARIOS_SIMULTANEOS; i++) {
        if (fifo_connections[i].usuario_slot == usuario_slot && fifo_connections[i].is_open) {
            return fifo_connections[i].fd;
        }
    }
    
    // We need to create a new connection
    for (int i = 0; i < MAX_USUARIOS_SIMULTANEOS; i++) {
        if (fifo_connections[i].usuario_slot == -1) {
            // Open the FIFO for writing
            int cuenta = usuarios[usuario_slot].cuenta; // Get the actual account number
            printf("[DEBUG] Opening new persistent FIFO connection to cuenta %d (slot %d): %s\n", 
                    cuenta, usuario_slot, path);
            
            fifo_connections[i].fd = open(path, O_WRONLY);
            if (fifo_connections[i].fd < 0) {
                perror("[ERROR] Failed to open FIFO for persistent connection");
                return -1;
            }
            
            fifo_connections[i].usuario_slot = usuario_slot;
            fifo_connections[i].is_open = 1;
            strcpy(fifo_connections[i].path, path);
            
            printf("[DEBUG] Persistent FIFO connection established for cuenta %d, fd=%d\n", 
                   cuenta, fifo_connections[i].fd);
            return fifo_connections[i].fd;
        }
    }
    
    printf("[ERROR] No slot available for new FIFO connection\n");
    return -1;
}

// Function to close a FIFO connection when a user disconnects
void close_fifo_connection(int usuario_slot) {
    for (int i = 0; i < MAX_USUARIOS_SIMULTANEOS; i++) {
        if (fifo_connections[i].usuario_slot == usuario_slot && fifo_connections[i].is_open) {
            printf("[DEBUG] Closing persistent FIFO connection to user %d\n", usuario_slot);
            close(fifo_connections[i].fd);
            fifo_connections[i].usuario_slot = -1;
            fifo_connections[i].fd = -1;
            fifo_connections[i].is_open = 0;
            memset(fifo_connections[i].path, 0, sizeof(fifo_connections[i].path));
            break;
        }
    }
}

/* Función para manejar señales y terminar adecuadamente */
void manejador_senales(int sig) {
    printf("\nSeñal recibida (%d). Terminando proceso banco...\n", sig);
    
    // Close all persistent FIFO connections
    for (int i = 0; i < MAX_USUARIOS_SIMULTANEOS; i++) {
        if (fifo_connections[i].is_open) {
            printf("Cerrando conexión FIFO para usuario %d\n", fifo_connections[i].usuario_slot);
            close(fifo_connections[i].fd);
            fifo_connections[i].is_open = 0;
        }
    }
    
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
            if (sscanf(line + 16, "%255s", cfg->archivo_cuentas) != 1) {
                printf("Warning: Error reading ARCHIVO_CUENTAS\n");
                cfg->archivo_cuentas[0] = '\0';
            }
        } else if (strncmp(line, "ARCHIVO_LOG=", 12) == 0) {
            if (sscanf(line + 12, "%255s", cfg->archivo_log) != 1) {
                printf("Warning: Error reading ARCHIVO_LOG\n");
                cfg->archivo_log[0] = '\0';
            }
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

    // Close persistent FIFO connection
    close_fifo_connection(idx);
}

// Definición de la función obtener_saldo_cuenta
double obtener_saldo_cuenta(int cuenta) {
    // Definición de la estructura Cuenta
    typedef struct {
        int numero_cuenta;
        char titular[50];
        float saldo;
        int num_transacciones;
    } Cuenta;
    
    // Primero determinar la ruta del archivo de cuentas
    const char* rutas_posibles[] = {
        // Usar la configuración si está disponible
        strlen(config.archivo_cuentas) > 0 ? config.archivo_cuentas : NULL,
        "../data/cuentas.dat",
        "./data/cuentas.dat",
        "/home/admin/PracticaFinal/BANCO/data/cuentas.dat"
    };
    
    FILE *archivo = NULL;
    const char* ruta_usada = NULL;
    
    // Print a clear header for the file access trace
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║           BANCO - ACCESO BASE DE DATOS           ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    
    // Intentar abrir el archivo con las rutas posibles
    for (int i = 0; i < sizeof(rutas_posibles)/sizeof(rutas_posibles[0]); i++) {
        if (rutas_posibles[i] == NULL) continue;
        
        printf("[TRAZA] Intentando abrir archivo de cuentas: %s\n", rutas_posibles[i]);
        archivo = fopen(rutas_posibles[i], "rb");
        if (archivo != NULL) {
            ruta_usada = rutas_posibles[i];
            printf("[ÉXITO] Archivo de cuentas abierto: %s\n", ruta_usada);
            break;
        } else {
            printf("[ERROR] No se pudo abrir %s: %s\n", rutas_posibles[i], strerror(errno));
        }
    }
    
    // Si no se pudo abrir ningún archivo, mostrar mensaje claro y detallado
    if (archivo == NULL) {
        printf("\n");
        printf("┌──────────────────────────────────────────────────┐\n");
        printf("│               ¡¡ ERROR CRÍTICO !!                │\n");
        printf("│       NO SE PUDO ABRIR EL ARCHIVO DE CUENTAS     │\n");
        printf("└──────────────────────────────────────────────────┘\n");
        printf("  Razón: %s\n\n", strerror(errno));
        printf("  Rutas verificadas:\n");
        
        for (int i = 0; i < sizeof(rutas_posibles)/sizeof(rutas_posibles[0]); i++) {
            if (rutas_posibles[i] != NULL) {
                // Check if file exists
                if (access(rutas_posibles[i], F_OK) != -1) {
                    printf("    ✓ %s (archivo existe pero no se puede leer)\n", rutas_posibles[i]);
                } else {
                    printf("    ✗ %s (archivo no existe)\n", rutas_posibles[i]);
                }
            }
        }
        
        printf("\n  Soluciones posibles:\n");
        printf("    1. Cree el archivo ejecutando: ./init_cuentas\n");
        printf("    2. Verifique permisos: chmod 644 ../data/cuentas.dat\n");
        printf("    3. Configure ruta correcta en config/config.txt\n\n");
        
        // Log the error to the transaction log
        FILE *log_file = fopen(LOG_FILE, "a");
        if (log_file != NULL) {
            fprintf(log_file, "[ERROR CRÍTICO] No se pudo abrir el archivo de cuentas: %s\n", strerror(errno));
            fclose(log_file);
        }
        
        // Create a temporary file with a warning that it's only for testing
        printf("[AVISO] Creando archivo temporal para pruebas de emergencia\n");
        printf("        ¡ATENCIÓN! Este archivo es temporal y se perderá al reiniciar\n\n");
        
        archivo = fopen("../data/cuentas_temp.dat", "w+b"); // Archivo físico temporal
        if (archivo == NULL) {
            printf("[ERROR FATAL] No se pudo crear archivo temporal: %s\n", strerror(errno));
            return -1;
        }
        
        // Cuentas predeterminadas para pruebas
        Cuenta cuentas_prueba[] = {
            {1001, "Cliente Uno (TEMP)", 1000.0, 0},
            {1002, "Cliente Dos (TEMP)", 2000.0, 0},
            {1003, "Cliente Tres (TEMP)", 3000.0, 0},
            {1009, "Cliente Nueve (TEMP)", 9000.0, 0}
        };
        
        // Escribir cuentas de prueba
        if (fwrite(cuentas_prueba, sizeof(Cuenta), 
                  sizeof(cuentas_prueba)/sizeof(cuentas_prueba[0]), archivo) == 0) {
            printf("[ERROR FATAL] Error al escribir cuentas de prueba: %s\n", strerror(errno));
            fclose(archivo);
            return -1;
        }
        
        // Rebobinar para leer desde el inicio
        rewind(archivo);
        printf("[INFO] Archivo temporal de prueba creado con %ld cuentas\n", 
               sizeof(cuentas_prueba)/sizeof(cuentas_prueba[0]));
        ruta_usada = "archivo temporal";
    }
    
    printf("[DEBUG] Archivo abierto: %s\n", ruta_usada);
    
    // Ahora leer las cuentas
    Cuenta cuenta_actual;
    int encontrada = 0;
    
    printf("[DEBUG] Buscando cuenta %d en %s...\n", cuenta, ruta_usada);
    
    // Leer cada registro y buscar la cuenta
    while (fread(&cuenta_actual, sizeof(Cuenta), 1, archivo) == 1) {
        printf("[DEBUG] Cuenta leída: %d (%s) con saldo %.2f\n", 
               cuenta_actual.numero_cuenta, cuenta_actual.titular, cuenta_actual.saldo);
        
        if (cuenta_actual.numero_cuenta == cuenta) {
            encontrada = 1;
            break;
        }
    }
    
    double resultado = -1;  // Valor por defecto
    
    if (encontrada) {
        printf("[DEBUG] ¡Cuenta %d encontrada! Saldo: %.2f\n", cuenta, cuenta_actual.saldo);
        resultado = cuenta_actual.saldo;
    } else {
        printf("[DEBUG] Cuenta %d no encontrada\n", cuenta);
    }
    
    fclose(archivo);
    return resultado;
}

// Función para procesar la consulta de saldo
void procesar_consulta_saldo(int cuenta, int fifo_escritura_fd) {
    debug_log("Iniciando proceso de consulta de saldo para cuenta %d (fd=%d)", 
           cuenta, fifo_escritura_fd);
    
    // Verify that the descriptor is valid
    if (fifo_escritura_fd < 0) {
        debug_log("ERROR: Descriptor de FIFO inválido");
        return;
    }
    
    struct stat fifo_stat;
    if (fstat(fifo_escritura_fd, &fifo_stat) < 0) {
        debug_log("ERROR: El descriptor %d no es válido", fifo_escritura_fd);
        perror("fstat");
        return;
    }
    
    debug_log("Descriptor de FIFO %d verificado y válido", fifo_escritura_fd);
    
    // Obtener el saldo real
    double saldo = obtener_saldo_cuenta(cuenta);
    debug_log("Saldo obtenido para cuenta %d: %.2f", cuenta, saldo);
    
    // Create a message packet with fixed format and length
    char respuesta[BUFFER_SIZE];
    
    if (saldo < 0) {
        sprintf(respuesta, "[SALDO:ERROR:No se pudo obtener el saldo de la cuenta %d]", cuenta);
    } else {
        sprintf(respuesta, "[SALDO:%.2f:OK]", saldo);
    }
    
    debug_log("Mensaje estructurado: '%s'", respuesta);
    
    // Add a test signature and newline for clear message boundaries
    strcat(respuesta, "\nFIN-MSG\n");
    
    // Send the response
    debug_log("Enviando respuesta (%ld bytes) por fd=%d...", strlen(respuesta), fifo_escritura_fd);
    
    ssize_t bytes_escritos = write(fifo_escritura_fd, respuesta, strlen(respuesta));
    if (bytes_escritos < 0) {
        debug_log("ERROR: No se pudo enviar la respuesta al cliente");
        perror("write");
        return;
    }
    
    debug_log("Respuesta enviada correctamente: %ld bytes escritos", bytes_escritos);
    
    // Ensure the data is sent immediately
    fsync(fifo_escritura_fd);
    debug_log("Datos sincronizados a disco");
    
    // Shorter delay that's still effective
    debug_log("Esperando para asegurar que el cliente procese la respuesta (100ms)...");
    usleep(100000);  // 100ms instead of 1 second
    debug_log("Respuesta completada");
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

    // Verificar la existencia del archivo de cuentas al iniciar
    const char* ruta_cuentas = strlen(config.archivo_cuentas) > 0 ? 
                          config.archivo_cuentas : "../data/cuentas.dat";
    
    if (access(ruta_cuentas, F_OK) != 0) {
        printf("\n");
        printf("┌──────────────────────────────────────────────────┐\n");
        printf("│               ¡¡ ADVERTENCIA !!                  │\n");
        printf("│     NO SE ENCONTRÓ EL ARCHIVO DE CUENTAS:        │\n");
        printf("│     %-42s │\n", ruta_cuentas);
        printf("└──────────────────────────────────────────────────┘\n");
        printf("  Se iniciará con valores temporales al consultar saldos.\n");
        printf("  Para crear un archivo permanente ejecute: ./init_cuentas\n\n");
    } else if (access(ruta_cuentas, R_OK) != 0) {
        printf("\n");
        printf("┌──────────────────────────────────────────────────┐\n");
        printf("│               ¡¡ ADVERTENCIA !!                  │\n");
        printf("│   NO TIENE PERMISOS PARA LEER ARCHIVO CUENTAS:   │\n");
        printf("│     %-42s │\n", ruta_cuentas);
        printf("└──────────────────────────────────────────────────┘\n");
        printf("  Corrija los permisos con: chmod 644 %s\n\n", ruta_cuentas);
    }

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

    // Initialize FIFO connections
    init_fifo_connections();

    printf("Banco iniciado. Esperando conexiones de usuario...\n");
    printf("Presione Ctrl+C para terminar.\n\n");

    // Add a variable to control the check interval
    int check_count = 0;
    
    // Bucle principal para aceptar conexiones de usuarios
    while (continuar_ejecucion) {
        // Increment the counter for periodic messages
        check_count++;
        
        // Every 30 iterations, print a waiting message to show the bank is still listening
        if (check_count % 30 == 0) {
            printf("Banco activo - Esperando mensajes de usuarios o nuevas conexiones...\n");
            check_count = 0;  // Reset counter
        }
        
        // First process messages from existing users
        for (int i = 0; i < MAX_USUARIOS_SIMULTANEOS; i++) {
            if (usuarios[i].fifo_lectura_fd > 0) {
                char buffer[256];
                fd_set set;
                struct timeval timeout;
                
                FD_ZERO(&set);
                FD_SET(usuarios[i].fifo_lectura_fd, &set);
                
                // Revert to a shorter timeout for better responsiveness
                timeout.tv_sec = 0;
                timeout.tv_usec = 20000; // 20ms (reduced from 1 second but still better than 1ms)
                
                int ready = select(usuarios[i].fifo_lectura_fd + 1, &set, NULL, NULL, &timeout);
                
                if (ready > 0 && FD_ISSET(usuarios[i].fifo_lectura_fd, &set)) {
                    ssize_t nbytes = read(usuarios[i].fifo_lectura_fd, buffer, sizeof(buffer) - 1);
                    
                    if (nbytes > 0) {
                        buffer[nbytes] = '\0';
                        
                        // Categorize the message for better logging
                        int is_login = strstr(buffer, "ha iniciado sesión") != NULL;
                        int is_logout = strstr(buffer, "ha cerrado sesión") != NULL;
                        int is_balance_query = strstr(buffer, "Consulta de saldo") != NULL || 
                                              strstr(buffer, "consulta de saldo") != NULL ||
                                              strstr(buffer, "saldo") != NULL;
                        
                        if (is_login) {
                            debug_log("✅ Mensaje de conexión detectado: Usuario %d ha iniciado sesión", usuarios[i].cuenta);
                        } else if (is_logout) {
                            debug_log("👋 Mensaje de desconexión detectado: Usuario %d ha cerrado sesión", usuarios[i].cuenta);
                        } else {
                            debug_log("📩 Mensaje regular recibido de usuario %d (cuenta %d) - %d bytes: '%s'", 
                                    i, usuarios[i].cuenta, (int)nbytes, buffer);
                        }
                        
                        // Log to transaction log
                        fprintf(log_file, "Usuario (Cuenta %d): %s", usuarios[i].cuenta, buffer);
                        fflush(log_file);
                        
                        // Print to console in a more formatted way
                        printf("Mensaje de usuario %d (Cuenta %d): %s", i, usuarios[i].cuenta, buffer);
                        
                        // Only check for operations in non-connection messages
                        if (!is_login && !is_logout) {
                            debug_log("Analizando mensaje para detectar operaciones...");
                            
                            // Check for balance query with consistent patterns
                            if (is_balance_query) {
                                debug_log("🔍 Detectada consulta de saldo de cuenta %d", usuarios[i].cuenta);
                                
                                // Verify that the FIFO exists
                                if (access(usuarios[i].fifo_escritura, F_OK) == -1) {
                                    debug_log("❌ ERROR: El FIFO %s no existe", usuarios[i].fifo_escritura);
                                    continue;
                                }
                                
                                // Get the persistent connection instead of opening a new one
                                int fifo_escritura_fd = get_fifo_connection(i, usuarios[i].fifo_escritura);
                                if (fifo_escritura_fd < 0) {
                                    debug_log("❌ ERROR: No se pudo obtener la conexión FIFO persistente");
                                    continue;
                                }
                                
                                debug_log("✅ FIFO abierto correctamente (fd=%d)", fifo_escritura_fd);
                                procesar_consulta_saldo(usuarios[i].cuenta, fifo_escritura_fd);
                            } else {
                                debug_log("ℹ️ No se detectó ninguna operación en el mensaje");
                            }
                        } else {
                            debug_log("ℹ️ Mensaje administrativo procesado, no requiere respuesta");
                        }
                    }
                    else if (nbytes == 0) { // EOF - el FIFO se cerró
                        close(usuarios[i].fifo_lectura_fd);
                        usuarios[i].fifo_lectura_fd = 0;
                    }
                }
            }
        }

        // Fix the waitpid issue - only mark as disconnected if truly gone
        for (int i = 0; i < MAX_USUARIOS_SIMULTANEOS; i++) {
            if (usuarios[i].pid > 0) {
                int status;
                pid_t result = waitpid(usuarios[i].pid, &status, WNOHANG);
                
                // Only consider truly terminated processes (not just the terminal launcher)
                if (result > 0) {
                    // Check if this is the actual user process terminating or just the launcher
                    // We'll do this by checking if we can still read from the FIFO
                    if (usuarios[i].fifo_lectura_fd > 0) {
                        // Try a non-blocking read to see if the connection is truly dead
                        char test_buf[10];
                        int flags = fcntl(usuarios[i].fifo_lectura_fd, F_GETFL);
                        fcntl(usuarios[i].fifo_lectura_fd, F_SETFL, flags | O_NONBLOCK);
                        
                        // We don't care about the contents, just whether the read fails
                        ssize_t test_read = read(usuarios[i].fifo_lectura_fd, test_buf, 1);
                        
                        // Only disconnect if read returns 0 (EOF) or error
                        if (test_read <= 0) {
                            printf("Usuario (Cuenta: %d, PID: %d) desconectado.\n", 
                                   usuarios[i].cuenta, usuarios[i].pid);
                            fprintf(log_file, "Usuario desconectado: Cuenta %d (PID %d)\n", 
                                    usuarios[i].cuenta, usuarios[i].pid);
                            fflush(log_file);
                            
                            limpiar_recursos_usuario(i);
                        } else {
                            // False alarm - just the launcher exited
                            debug_log("Launcher exited but user connection still active (Cuenta: %d)", 
                                     usuarios[i].cuenta);
                        }
                    } else {
                        // FIFO is already closed, definitely disconnected
                        limpiar_recursos_usuario(i);
                    }
                }
                else if (result < 0 && errno != ECHILD) {
                    perror("Error en waitpid");
                }
            }
        }

        int cuenta_usuario;
        int slot_disponible = -1;

        // Buscar un slot disponible para un nuevo usuario
        for (int i = 0; i < MAX_USUARIOS_SIMULTANEOS; i++) {
            if (usuarios[i].pid == 0) {
                slot_disponible = i;
                break;
            }
        }

        if (slot_disponible != -1) {
            // Create a non-blocking input method to check for user account input
            fd_set stdin_set;
            struct timeval input_timeout;
            FD_ZERO(&stdin_set);
            FD_SET(STDIN_FILENO, &stdin_set);
            
            // Set a very short timeout so we don't block the message processing
            input_timeout.tv_sec = 0;
            input_timeout.tv_usec = 10000;  // 10ms
            
            // Check if there's input ready to be read
            if (select(STDIN_FILENO + 1, &stdin_set, NULL, NULL, &input_timeout) > 0) {
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
                    
                    // Primero cerramos los FIFOs que podría tener abiertos el proceso padre
                    // para evitar bloqueos
                    for (int j = 0; j < MAX_USUARIOS_SIMULTANEOS; j++) {
                        if (usuarios[j].fifo_lectura_fd > 0) {
                            close(usuarios[j].fifo_lectura_fd);
                        }
                    }
                    
                    // Convertir cuenta_usuario a string y preparar argumentos
                    char cuenta_str[20];
                    sprintf(cuenta_str, "%d", cuenta_usuario);
                    
                    // Crear el comando para ejecutar en la nueva terminal
                    char command[512];
                    if (system("command -v xterm > /dev/null") == 0) {
                        sprintf(command, "xterm -T \"Usuario Banco - Cuenta %d\" -e \"./usuario %s %s %s\"",
                                cuenta_usuario, cuenta_str, fifo_from_usuario, fifo_to_usuario);
                    } else if (system("command -v gnome-terminal > /dev/null") == 0) {
                        sprintf(command, "gnome-terminal -- ./usuario %s %s %s",
                                cuenta_str, fifo_from_usuario, fifo_to_usuario);
                    } else {
                        fprintf(stderr, "Error: No se encontró xterm ni gnome-terminal\n");
                        exit(EXIT_FAILURE);
                    }
                    
                    // Ejecutar el comando
                    system(command);
                    exit(EXIT_SUCCESS);
                } else {
                    // Proceso padre
                    usuarios[slot_disponible].pid = pid;
                    usuarios[slot_disponible].cuenta = cuenta_usuario;
                    
                    // Primero abrimos el FIFO para lectura (bloqueante)
                    printf("Esperando a que el usuario abra el FIFO para lectura...\n");
                    usuarios[slot_disponible].fifo_lectura_fd = open(fifo_from_usuario, O_RDONLY);
                    if (usuarios[slot_disponible].fifo_lectura_fd < 0) {
                        perror("Error al abrir FIFO para lectura");
                        limpiar_recursos_usuario(slot_disponible);
                        continue;
                    }
                    
                    // Ahora configuramos como no bloqueante
                    int flags = fcntl(usuarios[slot_disponible].fifo_lectura_fd, F_GETFL);
                    fcntl(usuarios[slot_disponible].fifo_lectura_fd, F_SETFL, flags | O_NONBLOCK);
                    
                    // Establecer una conexión FIFO persistente para escritura
                    printf("Esperando a que el usuario abra el FIFO para escritura...\n");
                    int fifo_escritura_fd = get_fifo_connection(slot_disponible, fifo_to_usuario);
                    if (fifo_escritura_fd < 0) {
                        perror("Error al obtener conexión FIFO persistente");
                        limpiar_recursos_usuario(slot_disponible);
                        continue;
                    }
                    
                    printf("Usuario con cuenta %d conectado (PID: %d)\n", cuenta_usuario, pid);
                    fprintf(log_file, "Usuario conectado: Cuenta %d (PID: %d)\n", cuenta_usuario, pid);
                    fflush(log_file);
                }
            }
        }
        
        // Revert to original sleep value for responsiveness
        usleep(100000); // 100ms (original value)
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

    // When cleaning up resources, close all persistent FIFO connections
    printf("Cerrando todas las conexiones FIFO persistentes...\n");
    for (int i = 0; i < MAX_USUARIOS_SIMULTANEOS; i++) {
        if (fifo_connections[i].is_open) {
            close(fifo_connections[i].fd);
            fifo_connections[i].is_open = 0;
        }
    }

    // Cierre de recursos.
    fclose(log_file);
    sem_close(sem);
    sem_unlink("/cuentas_semaphore");

    printf("Proceso del banco finalizado correctamente.\n");
    return EXIT_SUCCESS;
}
