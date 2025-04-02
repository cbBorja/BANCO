#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Definición de la estructura Cuenta
typedef struct {
    int numero_cuenta;
    char titular[50];
    float saldo;
    int num_transacciones;
} Cuenta;

// Función para probar el acceso a una cuenta
double leer_saldo(int numero_cuenta, const char* ruta_archivo) {
    FILE *archivo = fopen(ruta_archivo, "rb");
    if (archivo == NULL) {
        perror("Error al abrir el archivo");
        return -1.0;
    }
    
    printf("Archivo abierto correctamente: %s\n", ruta_archivo);
    
    Cuenta cuenta;
    int encontrado = 0;
    
    while (fread(&cuenta, sizeof(Cuenta), 1, archivo) == 1) {
        printf("Cuenta leída: %d, Titular: %s, Saldo: %.2f\n",
              cuenta.numero_cuenta, cuenta.titular, cuenta.saldo);
              
        if (cuenta.numero_cuenta == numero_cuenta) {
            encontrado = 1;
            break;
        }
    }
    
    fclose(archivo);
    
    if (encontrado) {
        printf("¡Cuenta %d encontrada! Saldo: %.2f\n", numero_cuenta, cuenta.saldo);
        return cuenta.saldo;
    } else {
        printf("Cuenta %d no encontrada\n", numero_cuenta);
        return -1.0;
    }
}

int main(int argc, char* argv[]) {
    int numero_cuenta = 1001;  // Por defecto
    const char* ruta_archivo = "../data/cuentas.dat";  // Por defecto
    
    // Procesar argumentos
    if (argc > 1) {
        numero_cuenta = atoi(argv[1]);
    }
    
    if (argc > 2) {
        ruta_archivo = argv[2];
    }
    
    printf("=== Test de lectura de cuenta ===\n");
    printf("Cuenta a buscar: %d\n", numero_cuenta);
    printf("Archivo: %s\n", ruta_archivo);
    
    double saldo = leer_saldo(numero_cuenta, ruta_archivo);
    
    if (saldo >= 0) {
        printf("\nResultado: ÉXITO - Saldo: %.2f\n", saldo);
        return 0;
    } else {
        printf("\nResultado: ERROR - No se pudo leer el saldo\n");
        return 1;
    }
}
