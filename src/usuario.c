#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <errno.h>

#define BUFFER_SIZE 256
#define LOG_FILE "../data/transacciones.log"

// Debug function to log with timestamp
void debug_log(const char *format, ...) {
    char timestamp[30];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    va_list args;
    va_start(args, format);
    
    printf("[USUARIO %s] ", timestamp);
    vprintf(format, args);
    printf("\n");
    fflush(stdout);
    
    va_end(args);
}

// Definición de la estructura Operacion.
typedef struct {
    int tipo_operacion; // 1: Depósito, 2: Retiro, 3: Transferencia, 4: Consultar saldo
    double monto;
    int cuenta;
    // Descriptor de archivo para escribir al banco
    int fifo_fd;
} Operacion;

// Estructura para pasar parámetros al hilo.
typedef struct {
    Operacion op;
} OperacionArgs;

// Variables globales para los FIFOs
char fifo_escritura[256]; // Usuario escribe aquí, banco lee
char fifo_lectura[256];   // Banco escribe aquí, usuario lee
int fifo_escritura_fd = -1;
int fifo_lectura_fd = -1;

// Mutex para sincronizar la salida
pthread_mutex_t stdout_mutex = PTHREAD_MUTEX_INITIALIZER;

// Manejador para cerrar apropiadamente
void manejador_terminar(int sig) {
    printf("\nTerminando sesión...\n");
    
    if (fifo_escritura_fd >= 0) {
        close(fifo_escritura_fd);
    }
    
    if (fifo_lectura_fd >= 0) {
        close(fifo_lectura_fd);
    }
    
    exit(0);
}

// Función para obtener timestamp actual
void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// Función que ejecuta la operación y comunica con el banco
void *ejecutar_operacion(void *arg) {
    OperacionArgs *args = (OperacionArgs *)arg;
    char mensaje[BUFFER_SIZE];
    char timestamp[30];
    get_timestamp(timestamp, sizeof(timestamp));
    
    // Bloquear el mutex para asegurar que el mensaje completo se escriba de una vez
    pthread_mutex_lock(&stdout_mutex);
    
    // Crear mensaje basado en el tipo de operación
    switch (args->op.tipo_operacion) {
        case 1:
            sprintf(mensaje, "[%s] Depósito de %.2f en la cuenta %d completado.\n", 
                   timestamp, args->op.monto, args->op.cuenta);
            break;
        case 2:
            sprintf(mensaje, "[%s] Retiro de %.2f de la cuenta %d completado.\n", 
                   timestamp, args->op.monto, args->op.cuenta);
            break;
        case 3:
            sprintf(mensaje, "[%s] Transferencia de %.2f desde la cuenta %d completada.\n", 
                   timestamp, args->op.monto, args->op.cuenta);
            break;
        case 4:
            sprintf(mensaje, "[%s] Consulta de saldo en la cuenta %d solicitada.\n", 
                   timestamp, args->op.cuenta);
            break;
        default:
            sprintf(mensaje, "[%s] Operación desconocida en la cuenta %d.\n", 
                   timestamp, args->op.cuenta);
            break;
    }
    
    // Enviar mensaje al banco a través del FIFO
    if (fifo_escritura_fd >= 0) {
        printf("[DEBUG] Enviando mensaje al banco: %s", mensaje);
        ssize_t bytes_escritos = write(fifo_escritura_fd, mensaje, strlen(mensaje));
        if (bytes_escritos < 0) {
            perror("[ERROR] Error al escribir en FIFO");
            pthread_mutex_unlock(&stdout_mutex);
            free(args);
            pthread_exit(NULL);
        } else {
            printf("[DEBUG] Mensaje enviado correctamente: %ld bytes escritos\n", bytes_escritos);
        }
    }
    
    // Si es una consulta de saldo, esperar respuesta del banco
    if (args->op.tipo_operacion == 4 && fifo_lectura_fd >= 0) {
        debug_log("Preparando para consulta de saldo con cuenta %d", args->op.cuenta);
        
        // Use direct mode setting instead of toggling between modes
        int original_flags = fcntl(fifo_lectura_fd, F_GETFL);
        fcntl(fifo_lectura_fd, F_SETFL, original_flags & ~O_NONBLOCK); // Set blocking
        
        debug_log("Esperando respuesta del banco (puede tardar unos segundos)...");
        printf("Esperando respuesta del banco...\n");
        
        // Use select() with a timeout and limited retries
        int max_retries = 3;
        int retry_count = 0;
        int got_response = 0;
        
        while (retry_count < max_retries && !got_response) {
            fd_set readfds;
            struct timeval tv;
            FD_ZERO(&readfds);
            FD_SET(fifo_lectura_fd, &readfds);
            tv.tv_sec = 5;  // 5 second timeout per retry
            tv.tv_usec = 0;
            
            debug_log("Intento %d de %d para leer respuesta", retry_count + 1, max_retries);
            int ret = select(fifo_lectura_fd + 1, &readfds, NULL, NULL, &tv);
            
            if (ret == -1) {
                debug_log("ERROR en select(): %s", strerror(errno));
                retry_count++;
                continue;
            } else if (ret == 0) {
                debug_log("Timeout de 5 segundos esperando respuesta");
                retry_count++;
                continue;
            } else {
                // Data available
                debug_log("¡Hay datos disponibles para leer!");
                
                // Clear buffer and read data
                char buffer[BUFFER_SIZE * 2] = {0};
                ssize_t bytes_leidos = read(fifo_lectura_fd, buffer, sizeof(buffer) - 1);
                
                if (bytes_leidos > 0) {
                    buffer[bytes_leidos] = '\0'; // Ensure null termination
                    debug_log("Datos recibidos: '%s'", buffer);
                    printf("Respuesta del banco: %s\n", buffer);
                    got_response = 1;  // Got a valid response
                } else if (bytes_leidos == 0) {
                    debug_log("EOF detectado - El banco cerró la conexión sin enviar datos");
                    retry_count++;
                    continue;
                } else {
                    debug_log("Error al leer del FIFO: %s", strerror(errno));
                    retry_count++;
                    continue;
                }
            }
        }
        
        if (!got_response) {
            printf("No se pudo obtener respuesta del banco después de %d intentos\n", max_retries);
        }
        
        // Restore original flags
        fcntl(fifo_lectura_fd, F_SETFL, original_flags);
        debug_log("Restaurada configuración original del FIFO");
    }
    
    // Desbloquear el mutex
    pthread_mutex_unlock(&stdout_mutex);
    
    free(args);
    pthread_exit(NULL);
}

// Función que muestra el menú interactivo y lanza un hilo por cada operación.
void menu_usuario(int cuenta) {
    int opcion;
    double monto;
    pthread_t tid;

    printf("\n¡Bienvenido al Sistema Bancario!\n");
    printf("Sesión iniciada para la cuenta: %d\n", cuenta);
    printf("Comunicación establecida con el servidor del banco.\n");

    while (1) {
        printf("\nMenú de Usuario (Cuenta %d):\n", cuenta);
        printf("1. Depósito\n2. Retiro\n3. Transferencia\n4. Consultar saldo\n5. Salir\n");
        printf("Seleccione una opción: ");
        if (scanf("%d", &opcion) != 1) {
            fprintf(stderr, "Entrada inválida. Intente de nuevo.\n");
            while (getchar() != '\n'); // Limpiar el buffer de entrada.
            continue;
        }

        // Si se selecciona 'Salir', se termina el proceso.
        if (opcion == 5) {
            printf("Sesión finalizada para la cuenta %d.\n", cuenta);
            break;
        }
        
        Operacion op;
        op.tipo_operacion = opcion;
        op.monto = 0.0;
        op.cuenta = cuenta;
        op.fifo_fd = fifo_escritura_fd;

        // Para operaciones que requieren monto.
        if (opcion >= 1 && opcion <= 3) {
            printf("Ingrese el monto: ");
            if (scanf("%lf", &monto) != 1) {
                fprintf(stderr, "Monto inválido. Intente de nuevo.\n");
                while (getchar() != '\n');
                continue;
            }
            op.monto = monto;
        }
        
        // Preparar los argumentos para el hilo.
        OperacionArgs *args = malloc(sizeof(OperacionArgs));
        if (args == NULL) {
            perror("Error al asignar memoria para OperacionArgs");
            continue;
        }
        args->op = op;
        
        // Crear un hilo para ejecutar la operación.
        if (pthread_create(&tid, NULL, ejecutar_operacion, (void *)args) != 0) {
            perror("Error al crear el hilo");
            free(args);
            continue;
        }
        
        // Desvincular el hilo para que se limpie automáticamente al finalizar.
        pthread_detach(tid);
    }

    // Enviar mensaje de cierre al banco
    char mensaje[BUFFER_SIZE];
    sprintf(mensaje, "Usuario con cuenta %d ha cerrado sesión.\n", cuenta);
    
    if (fifo_escritura_fd >= 0) {
        if (write(fifo_escritura_fd, mensaje, strlen(mensaje)) < 0) {
            perror("Error al enviar mensaje de cierre");
        }
    }
    
    // Cerrar FIFOs
    if (fifo_escritura_fd >= 0) close(fifo_escritura_fd);
    if (fifo_lectura_fd >= 0) close(fifo_lectura_fd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <numero_cuenta> [fifo_escritura] [fifo_lectura]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int numero_cuenta = atoi(argv[1]);
    
    // Configurar manejadores de señales
    signal(SIGTERM, manejador_terminar);
    signal(SIGINT, manejador_terminar);
    
    // Si se proporcionaron nombres de FIFOs, usarlos para la comunicación
    if (argc >= 4) {
        strcpy(fifo_escritura, argv[2]); // Usuario -> Banco
        strcpy(fifo_lectura, argv[3]);   // Banco -> Usuario
        
        printf("Conectando con el banco...\n");
        printf("FIFO escritura: %s\n", fifo_escritura);
        printf("FIFO lectura: %s\n", fifo_lectura);
        
        // Abrir FIFO para escritura (bloqueante)
        printf("Abriendo FIFO para escritura...\n");
        fifo_escritura_fd = open(fifo_escritura, O_WRONLY);
        if (fifo_escritura_fd < 0) {
            perror("Error al abrir FIFO para escritura");
            exit(EXIT_FAILURE);
        }
        
        // Abrir FIFO para lectura (no bloqueante para evitar que el programa
        // se quede colgado en la apertura si no hay datos)
        printf("Abriendo FIFO para lectura...\n");
        fifo_lectura_fd = open(fifo_lectura, O_RDONLY | O_NONBLOCK);
        if (fifo_lectura_fd < 0) {
            perror("Error al abrir FIFO para lectura");
            close(fifo_escritura_fd);
            exit(EXIT_FAILURE);
        }
        
        // Enviar mensaje de inicio
        if (fifo_escritura_fd >= 0) {
            char mensaje[BUFFER_SIZE];
            sprintf(mensaje, "Usuario con cuenta %d ha iniciado sesión.\n", numero_cuenta);
            if (write(fifo_escritura_fd, mensaje, strlen(mensaje)) < 0) {
                perror("Error al enviar mensaje de inicio");
            }
        }
    }
    
    menu_usuario(numero_cuenta);

    return EXIT_SUCCESS;
}
