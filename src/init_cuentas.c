#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Definición de la estructura Cuenta
typedef struct 
{
    int numero_cuenta;
    char titular[50];
    float saldo;
    int num_transacciones;
} Cuenta;

int main(void) 
{
    // Ruta del archivo de cuentas
    const char *ruta_archivo = "../data/cuentas.dat";
    
    // Abrir el archivo en modo escritura binaria
    FILE *archivo = fopen(ruta_archivo, "w+");
    if (archivo == NULL) {
        perror("Error al abrir el archivo");
        exit(1);
    }
    
    //Creamos cuentas ejemplo
    Cuenta cuentas[]={
        {1001, "Juan Vázquez", 1000.00f, 0},
        {1002, "Pedro Federico", 2000.67f, 0},
        {1003, "Maria Fernández", 3000.43f, 0},
        {1004, "Ana Ramírez", 4000.23f, 0},
        {1005, "Carmen Denia", 5000.98f, 0},
        {1006, "José Luis Dominguez", 5000.98f, 0},
        {1007, "Gonzalo D'Lorenzo", 5000.98f, 0},
        {1008, "Fran García", 5000.98f, 0},
        {1009, "Carlos Sévez ", 5000.98f, 0}
    };
    
    // Calcular el número de cuentas
    size_t num_cuentas = sizeof(cuentas) / sizeof(cuentas[0]);
    
    // Escribir las cuentas en el archivo
   for(size_t i = 0; i < num_cuentas; i++) {
        fprintf(archivo, "%d %s %.2f %d\n", cuentas[i].numero_cuenta, cuentas[i].titular, cuentas[i].saldo, cuentas[i].num_transacciones);
        if(ferror(archivo)) {
            perror("Error al escribir en el archivo");
            fclose(archivo);
            exit(1);
        }
    }
    
    // Asegurarse de que los datos se escriban correctamente en disco
    if (fflush(archivo) != 0) {
        perror("Error al vaciar el buffer del archivo");
        fclose(archivo);
        exit(1);
    }
    
    // Cerrar el archivo
    if (fclose(archivo) != 0) {
        perror("Error al cerrar el archivo");
        exit(1);
    }
    

    printf("Cuentas inicializadas y guardadas exitosamente en '%s'.\n", ruta_archivo);
    exit(0);
}
