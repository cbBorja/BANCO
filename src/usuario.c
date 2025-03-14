#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define BUFFER_SIZE 256

typedef struct {
    int tipo_operacion; // 1: Depósito, 2: Retiro, 3: Transferencia, 4: Consultar saldo
    double monto;
} Operacion;

void realizar_deposito() {
    printf("Realizando depósito...\n");
}
void realizar_retiro() {
    printf("Realizando retiro...\n");
}
void realizar_transferencia() {
    printf("Realizando transferencia...\n");
}
void consultar_saldo() {
    printf("Consultando saldo...\n");
}

void menu_usuario(int pipe_fd) {
    int opcion;
    Operacion operacion;

    while (1) {
        printf("1. Depósito\n2. Retiro\n3. Transferencia\n4. Consultar saldo\n5. Salir\n");
        printf("Seleccione una opción: ");
        scanf("%d", &opcion);

        if (opcion == 5) {
            exit(0);
        }
       
        operacion.tipo_operacion = opcion;
        if (opcion >= 1 && opcion <= 3) {
            printf("Ingrese el monto: ");
            scanf("%lf", &operacion.monto);
        }
       
        write(pipe_fd, &operacion, sizeof(operacion));
    }
}

void proceso_principal(int pipe_fd) {
    Operacion operacion;
    while (read(pipe_fd, &operacion, sizeof(operacion)) > 0) {
        switch (operacion.tipo_operacion) {
            case 1:
                printf("Depósito de %.2f recibido.\n", operacion.monto);
                break;
            case 2:
                printf("Retiro de %.2f recibido.\n", operacion.monto);
                break;
            case 3:
                printf("Transferencia de %.2f recibida.\n", operacion.monto);
                break;
            case 4:
                printf("Solicitud de consulta de saldo recibida.\n");
                break;
            default:
                printf("Operación desconocida.\n");
                break;
        }
    }
}

int main() {
    int pipe_fd[2];
    pid_t pid;

    if (pipe(pipe_fd) == -1) {
        perror("Error al crear la tubería");
        return 1;
    }

    pid = fork();
    if (pid < 0) {
        perror("Error al crear el proceso hijo");
        return 1;
    }
   
    if (pid == 0) { // Proceso hijo
        close(pipe_fd[0]); // Cierra el extremo de lectura
        menu_usuario(pipe_fd[1]);
    } else { // Proceso padre
        close(pipe_fd[1]); // Cierra el extremo de escritura
        proceso_principal(pipe_fd[0]);
        wait(NULL); // Espera a que termine el hijo
    }

    return 0;
}