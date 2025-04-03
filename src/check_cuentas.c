#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Definición de la estructura Cuenta
typedef struct {
    int numero_cuenta;
    char titular[50];
    float saldo;
    int num_transacciones;
} Cuenta;

int main(int argc, char *argv[]) {
    const char *filename;
    
    if (argc > 1) {
        filename = argv[1];
    } else {
        filename = "../data/cuentas.dat";
    }
    
    printf("Verificando archivo de cuentas: %s\n", filename);
    
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error: No se pudo abrir el archivo de cuentas %s (%s)\n", 
                filename, strerror(errno));
        return 1;
    }
    
    Cuenta cuenta;
    int count = 0;
    
    printf("Cuentas encontradas:\n");
    printf("---------------------------------------------------------\n");
    printf("| %-10s | %-30s | %-10s |\n", "Número", "Titular", "Saldo");
    printf("---------------------------------------------------------\n");
    
    while (fread(&cuenta, sizeof(Cuenta), 1, file) == 1) {
        printf("| %-10d | %-30s | %-10.2f |\n", 
               cuenta.numero_cuenta, cuenta.titular, cuenta.saldo);
        count++;
    }
    
    printf("---------------------------------------------------------\n");
    printf("Total de cuentas: %d\n", count);
    
    if (ferror(file)) {
        perror("Error al leer el archivo");
    }
    
    fclose(file);
    return EXIT_SUCCESS;
}
