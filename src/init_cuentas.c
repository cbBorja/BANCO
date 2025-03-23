#include <stdio.h>
#include <string.h>

struct Cuenta{
    int numero_cuenta;
    char titular[50];
    float saldo;
    int num_transacciones;
};

void CrearCuentas(char nombre_usuario[50]){
    struct Cuenta nueva_cuenta;
    nueva_cuenta.numero_cuenta = 123456; // Puedes generar un número de cuenta único
    strcpy(nueva_cuenta.titular, nombre_usuario);
    nueva_cuenta.saldo = 0.0;
    nueva_cuenta.num_transacciones = 0;

    printf("Cuenta creada:\n");
    printf("Numero de cuenta: %d\n", nueva_cuenta.numero_cuenta);
    printf("Titular: %s\n", nueva_cuenta.titular);
    printf("Saldo: %.2f\n", nueva_cuenta.saldo);
    printf("Numero de transacciones: %d\n", nueva_cuenta.num_transacciones);
}

