#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <pthread.h>

#define BUFFER_SIZE 256

// Definición de la estructura Operacion.
typedef struct {
    int tipo_operacion; // 1: Depósito, 2: Retiro, 3: Transferencia, 4: Consultar saldo
    double monto;
} Operacion;

// Estructura para pasar parámetros al hilo.
typedef struct {
    Operacion op;
    int pipe_fd;
} OperacionArgs;

// Función que simula la ejecución de la operación y la envía al proceso principal.
void *ejecutar_operacion(void *arg) {
    OperacionArgs *args = (OperacionArgs *)arg;

    // Simula la operación con un mensaje por pantalla.
    switch (args->op.tipo_operacion) {
        case 1:
            printf("Ejecutando: Depósito de %.2f\n", args->op.monto);
            //saldo = saldo + args->op.monto;
            break;
        case 2:
            printf("Ejecutando: Retiro de %.2f\n", args->op.monto);
            break;
        case 3:
            printf("Ejecutando: Transferencia de %.2f\n", args->op.monto);
            break;
        case 4:
            printf("Ejecutando: Consulta de saldo\n");
            break;
        default:
            printf("Operación desconocida.\n");
            break;
    }

    // Enviar la operación al proceso principal mediante la tubería.
    if (write(args->pipe_fd, &(args->op), sizeof(Operacion)) == -1) {
        perror("Error al escribir en la tubería");
    }

    free(args); // Liberar memoria asignada para los argumentos del hilo.
    pthread_exit(NULL);
}

// Función que muestra el menú interactivo y lanza un hilo por cada operación.
void menu_usuario(int pipe_fd) {
    int opcion;
    double monto;
    pthread_t tid;

    while (1) {
        printf("\nMenú de Usuario:\n");
        printf("1. Depósito\n2. Retiro\n3. Transferencia\n4. Consultar saldo\n5. Salir\n");
        printf("Seleccione una opción: ");
        if (scanf("%d", &opcion) != 1) {
            fprintf(stderr, "Entrada inválida. Intente de nuevo.\n");
            while (getchar() != '\n'); // Limpiar el buffer de entrada.
            continue;
        }

        // Si se selecciona 'Salir', se termina el proceso.
        if (opcion == 5) {
            printf("Saliendo del menú.\n");
            exit(0);
        }
        
        Operacion op;
        op.tipo_operacion = opcion;
        op.monto = 0.0;

        // Para operaciones que requieren monto.
        if (opcion >= 1 && opcion <= 3) {
            printf("Ingrese el monto: ");
            if (scanf("%lf", &monto) != 1) {
                fprintf(stderr, "Monto inválido. Intente de nuevo.\n");
                while (getchar() != '\n');
                continue;
            }
            op.monto = monto;
            //Esperar para no volver a sacar el menu
        }
        
        // Preparar los argumentos para el hilo.
        OperacionArgs *args = malloc(sizeof(OperacionArgs));
        if (args == NULL) {
            perror("Error al asignar memoria para OperacionArgs");
            continue;
        }
        args->op = op;
        args->pipe_fd = pipe_fd;
        
        // Crear un hilo para ejecutar la operación.
        if (pthread_create(&tid, NULL, ejecutar_operacion, (void *)args) != 0) {
            perror("Error al crear el hilo");
            free(args);
            continue;
        }
        // Desvincular el hilo para que se limpie automáticamente al finalizar.
        pthread_detach(tid);
    }
}

// Función del proceso principal que recibe y procesa las operaciones enviadas por el proceso hijo.
void proceso_principal(int pipe_fd) {
    Operacion operacion;
    ssize_t bytes_leidos;
    
    while ((bytes_leidos = read(pipe_fd, &operacion, sizeof(operacion))) > 0) {
        switch (operacion.tipo_operacion) {
            case 1:
                printf("Proceso principal: Depósito de %.2f recibido.\n", operacion.monto);
                break;
            case 2:
                printf("Proceso principal: Retiro de %.2f recibido.\n", operacion.monto);
                break;
            case 3:
                printf("Proceso principal: Transferencia de %.2f recibida.\n", operacion.monto);
                break;
            case 4:
                printf("Proceso principal: Solicitud de consulta de saldo recibida.\n");
                break;
            default:
                printf("Proceso principal: Operación desconocida.\n");
                break;
        }
    }
    if (bytes_leidos == -1) {
        perror("Error al leer de la tubería");
    }
}

int main() {
    int pipe_fd[2];
    pid_t pid;
    //Cuenta cuenta;

    // Crear la tubería para la comunicación entre procesos.
    if (pipe(pipe_fd) == -1) {
        perror("Error al crear la tubería");
        return EXIT_FAILURE;
    }

    // Crear el proceso hijo.
    pid = fork();
    if (pid < 0) {
        perror("Error al crear el proceso hijo");
        return EXIT_FAILURE;
    }
   
    if (pid == 0) { // Proceso hijo (usuario)
        close(pipe_fd[0]); // Cerrar el extremo de lectura en el hijo.
        menu_usuario(pipe_fd[1]);
    } else { // Proceso padre (coordinador)
        close(pipe_fd[1]); // Cerrar el extremo de escritura en el padre.
        proceso_principal(pipe_fd[0]);
        wait(NULL); // Espera a que termine el proceso hijo.
    }

    return EXIT_SUCCESS;
}
