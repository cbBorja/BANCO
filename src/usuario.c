#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BUFFER_SIZE 256

int main() {
    char buffer[BUFFER_SIZE];

    while (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
        printf("Usuario recibió: %s", buffer);
        // Aquí se manejarían las solicitudes del usuario y se comunicaría con el proceso padre
    }

    return 0;
}
