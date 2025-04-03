#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>

#define CONFIG_FILE "../config/config.txt"
#define LOG_FILE "../data/transacciones.log"
void Menu_InicioSesion();
int main()
{
    Menu_InicioSesion();
    // Leer el fichero de configuración.
   // leer_configuracion(CONFIG_FILE, &config);

    printf("h");
   
    // Abrir el archivo de log. Si en la configuración se especifica otro nombre, se usará.
    /*
    const char *log_filename = strlen(config.archivo_log) > 0 ? config.archivo_log : LOG_FILE;

    FILE *log_file = fopen(log_filename, "a");
    if (log_file == NULL)
    {
        perror("Error al abrir el archivo de log");
        exit(EXIT_FAILURE);
    }
        */
    printf("h");
    // Simulación de usuarios: se crea una tubería y se lanza un proceso hijo para cada usuario.
   

    return 0;
}
//typedef struct
//{
  //  int limite_retiro;
 //   int limite_transferencia;
  //  int umbral_retiros;
  //  int umbral_transferencias;
 //   int num_hilos;
  //  char archivo_cuentas[256];
  //  char archivo_log[256];
//} Config;

//Config config;

/* Función para leer la configuración desde el archivo.
   Se espera que cada línea siga el formato clave=valor sin espacios extra. */
   /*
void leer_configuracion(const char *filename, Config *config)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("Error al abrir el archivo de configuración");
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        // Eliminar salto de línea
        line[strcspn(line, "\n")] = 0;
        if (strncmp(line, "LIMITE_RETIRO=", 14) == 0)
        {
            config->limite_retiro = atoi(line + 14);
        }
        else if (strncmp(line, "LIMITE_TRANSFERENCIA=", 21) == 0)
        {
            config->limite_transferencia = atoi(line + 21);
        }
        else if (strncmp(line, "UMBRAL_RETIROS=", 15) == 0)
        {
            config->umbral_retiros = atoi(line + 15);
        }
        else if (strncmp(line, "UMBRAL_TRANSFERENCIAS=", 22) == 0)
        {
            config->umbral_transferencias = atoi(line + 22);
        }
        else if (strncmp(line, "NUM_HILOS=", 10) == 0)
        {
            config->num_hilos = atoi(line + 10);
        }
        else if (strncmp(line, "ARCHIVO_CUENTAS=", 16) == 0)
        {
            sscanf(line + 16, "%s", config->archivo_cuentas);
        }
        else if (strncmp(line, "ARCHIVO_LOG=", 12) == 0)
        {
            sscanf(line + 12, "%s", config->archivo_log);
        }
    }
    fclose(file);
}
    */


void Menu_InicioSesion() {
    int continuar = 1;
    bool Encontrado = false;
    int NumeroCuentaArch;

    while (continuar) {
        int NumeroCuenta;
        printf("Introduce el numero de cuenta\n");
        scanf("%d", &NumeroCuenta);

        FILE *archivoCuentas = fopen("cuentas.txt", "r");
        if (archivoCuentas == NULL) {
            perror("Error al abrir el archivo");
            return;
        }

        char linea[50];
        Encontrado = false;

        while (fgets(linea, sizeof(linea), archivoCuentas)) {
            sscanf(linea, "%d", &NumeroCuentaArch);
            if (NumeroCuenta == NumeroCuentaArch) {
                Encontrado = true;
                break;
            }
        }

        fclose(archivoCuentas);

        if (!Encontrado) {
            printf("No se ha encontrado ese número de cuenta\n");
            sleep(2);
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) { // Proceso hijo
            char numeroCuentaStr[10];
            sprintf(numeroCuentaStr, "%d", NumeroCuenta);

            execlp("gnome-terminal", "gnome-terminal", "--", "./usuario", numeroCuentaStr, NULL);
            perror("Error en execlp");
            exit(1);
        } else if (pid > 0) { // Proceso padre
            wait(NULL); // Espera a que el hijo termine
        } else {
            perror("Error en fork");
        }
    }
}