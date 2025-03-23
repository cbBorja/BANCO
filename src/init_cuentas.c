#include <stdio.h>
#include <string.h>


struct Cuenta{
    int numero_cuenta;
    char titular[50];
    float saldo;
    int num_transacciones;
};

int main(){
    const char *ruta_archivo = "../data/cuentas.dat";
    //Abrimos el archivo en modo escritura binaria
    FILE *archivo=fopen(ruta_archivo, "wb");
    if(archivo == NULL){
        printf("Error al abrir el archivo\n");
        return 1;
    }
    
    //Creamos cuentas ejemplo
    struct Cuenta cuentas[]={
        {1001, "Juan Vázquez", 1000.00f, 0},
        {1002, "Pedro Federico", 2000.67f, 0},
        {1003, "Maria Fernández", 3000.43f, 0},
        {1004, "Ana Ramírez", 4000.23f, 0},
        {1005, "Luis García", 5000.98f, 0}
    };

    //Contamos el número de cuentas
    int num_cuentas = sizeof(cuentas) / sizeof(cuentas[0]);

    //Escribimos las cuentas en el archivo
    size_t elemtos_escritos = fwrite(cuentas, sizeof(struct Cuenta), num_cuentas, archivo);
    if(elemtos_escritos != num_cuentas){
        printf("Error al escribir las cuentas en el archivo\n");
        fclose(archivo);
        return 1;
    }

    fclose(archivo);
    printf("Cuentas escritas en el archivo\n");
    return 0;

    
}
