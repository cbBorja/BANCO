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

#define BUFFER_SIZE 256
#define LOG_FILE "../data/transacciones.log"

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
            sprintf(mensaje, "[%s] Consulta de saldo en la cuenta %d completada.\n", 
                   timestamp, args->op.cuenta);
            break;
        default:
            sprintf(mensaje, "[%s] Operación desconocida en la cuenta %d.\n", 
                   timestamp, args->op.cuenta);
            break;
    }
    
    // Mostrar en pantalla para el usuario
    printf("%s", mensaje);
    
    // Enviar mensaje al banco a través del FIFO
    if (fifo_escritura_fd >= 0) {
        // Si el FIFO se cerró, intentamos reabrirlo
        if (fifo_escritura_fd == -1) {
            fifo_escritura_fd = open(fifo_escritura, O_WRONLY);
        }
        
        if (fifo_escritura_fd >= 0) {
            if (write(fifo_escritura_fd, mensaje, strlen(mensaje)) < 0) {
                perror("Error al escribir en FIFO");
            }
        } else {
            perror("No se pudo abrir el FIFO para escritura");
        }
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
        
        // Abrir FIFO para escritura (no bloqueante)
        fifo_escritura_fd = open(fifo_escritura, O_WRONLY);
        if (fifo_escritura_fd < 0) {
            perror("Error al abrir FIFO para escritura");
        }
        
        // Abrir FIFO para lectura (no bloqueante)
        fifo_lectura_fd = open(fifo_lectura, O_RDONLY | O_NONBLOCK);
        if (fifo_lectura_fd < 0) {
            perror("Error al abrir FIFO para lectura");
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
