#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define MAX_LINE_LENGTH 256
#define MAX_USERS 100
#define MAX_DAYS 31 // Suponiendo que el mes tiene máximo 31 días
// Variable global para almacenar el número total de operaciones registradas
int totalOperacionesRegistradas = 0;

typedef struct {/*Hemos creado un struct para los usuarios, ya que a la hora de analizar los datos las posibles irregularidades se dan por usuarios*/
    char idUsuario[20];/*Primero ponemos un id de usuario para identificar cada usuario*/
    int numTransaccionesPorHora[MAX_DAYS][24]; // Contador de transacciones por hora para cada día
    int numRetirosPorDia[MAX_DAYS];/*contador de retiros por dia*/
    int numErroresPorDia[MAX_DAYS];/*contador de errores por dia*/
    int operacionesPorTipoPorDia[MAX_DAYS][5]; // Contador de operaciones por tipo para cada día
    float dineroRetiradoPorDia[MAX_DAYS];/*almacena el dinero retirado en un mismo dia*/
    float dineroIngresadoPorDia[MAX_DAYS];/*almacena el dinero ingresado en un mismo dia*/
} Usuario;

char *obtenerFechaHora() { /*Funcion que se encarga de obtener la fecha para asi poder guardar el momento en el que se realizan las diferentes cosas dentro de nuestros
programas y poder almacenar la informacion dentro de nuestro archivo de log*/
    static char buffer[26];
    time_t tiempo;
    struct tm *tmInfo;

    time(&tiempo);
    tmInfo = localtime(&tiempo);
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tmInfo);/*Guardamos toda la fecha dentro de un buffer el cual será lo que devuelve la funcion*/

    return buffer;
}



void inicializarUsuarios(Usuario usuarios[MAX_USERS]) {
    // Inicializar datos de los usuarios dentro del array usuarios
    for (int i = 0; i < MAX_USERS; i++) { /*Hacemos uso del array de usuarios creada y con este bucle vamos recorriendo todos los usuarios dentro del array, y inicializando
    los datos de los mismos */
        strcpy(usuarios[i].idUsuario, "");
        for (int j = 0; j < MAX_DAYS; j++) {/*Este bucle sirve para recorrer todos los dias de cada usuario, porque tienen datos dieferentes en sus variables en funcion del dia
        de cada usuario*/
            usuarios[i].numRetirosPorDia[j] = 0;
            usuarios[i].numErroresPorDia[j] = 0;
            usuarios[i].dineroRetiradoPorDia[j] = 0;
            usuarios[i].dineroIngresadoPorDia[j] = 0;
            for (int k = 0; k < 24; k++) {/*Otro bucle igual que para el de los dias pero para las horas, porque a su vez dentro de un mismo dia tenemos 24 horas diferentes*/
                usuarios[i].numTransaccionesPorHora[j][k] = 0;
            }
            for (int k = 0; k < 5; k++) {/*Otro bucle por los tipo de operaciones*/
                usuarios[i].operacionesPorTipoPorDia[j][k] = 0;
            }
        }
    }
}

void procesarTransaccion(Usuario usuarios[MAX_USERS], char *line) {/*Funcion que procesa todas las transacciones del archivo consolidado.csv y actualiza la informacion de los
distintos usuarios*/

    /*DECLARO LAS VARIABLES QUE VOY A UTILIZAR PARA COPIAR LOS DATOS DE LAS LINEAS*/
    char idUsuario[20], idTipoOperacion[20], estado[20];
    char fechaInicio[20], fechaFin[20];
    float importe;
    int noOperacion, dia, hora;
    
    sscanf(line, "%*[^;];%[^;];%[^;];%[^;];%[^;];%d;%f€;%s", fechaInicio, fechaFin, idUsuario, idTipoOperacion, &noOperacion, &importe, estado);/*Cogo los valores de las lineas de consolidado.csv y guardo los
    valores en las variables*/
    
    // Obtener el día y la hora de la transacción, lo tengo que dividir porque al cogerlo viene junto tanto la hora como el dia de la operacion
    sscanf(fechaInicio, "%d/", &dia);
    sscanf(fechaInicio + 3, "%*[^/]/%*[^ ] %d:", &hora);

    /*IFs en caso de que el importe recogido de la linea sea negito se trataa de una operacion de retiro y en caso de que sea positivo se trata de ingreso*/
    if(importe < 0){

        strcpy(idTipoOperacion, "RETIRO");

    }else if(importe >= 0){

        strcpy(idTipoOperacion, "INGRESO");

    };

    // Encontrar el usuario en la lista de usuarios o agregarlo si es nuevo
    int indiceUsuario = -1;
    for (int i = 0; i < MAX_USERS; i++) {
        if (strcmp(usuarios[i].idUsuario, idUsuario) == 0) {
            indiceUsuario = i;
            break;
        }
        if (strcmp(usuarios[i].idUsuario, "") == 0) {
            strcpy(usuarios[i].idUsuario, idUsuario);
            indiceUsuario = i;
            break;
        }
    }
    
    // Actualizar los registros del usuario

    /*Voy cambiando mediante if y comparando con la informacion almacenada en las variables que he creado la informacion de cada usuario, dentro del array dek usuario*/
    usuarios[indiceUsuario].numTransaccionesPorHora[dia - 1][hora]++;

    if (strcmp(estado, "Error") == 0) {
        usuarios[indiceUsuario].numErroresPorDia[dia - 1]++;
    }
    if (strcmp(idTipoOperacion, "RETIRO") == 0) {
        usuarios[indiceUsuario].numRetirosPorDia[dia - 1]++;
        usuarios[indiceUsuario].dineroRetiradoPorDia[dia - 1] -= importe;
    } else if (strcmp(idTipoOperacion, "INGRESO") == 0) {
        usuarios[indiceUsuario].dineroIngresadoPorDia[dia - 1] += importe;
    }
    if (usuarios[indiceUsuario].operacionesPorTipoPorDia[dia - 1][noOperacion - 1] == 0) {
        usuarios[indiceUsuario].operacionesPorTipoPorDia[dia - 1][noOperacion - 1] = 1;
    }
    
    // Incrementar el contador de operaciones registradas
    totalOperacionesRegistradas++;
    
    // Imprimir la información de la transacción
    printf("Transacción registrada para el usuario %s:\n", idUsuario);
    printf("Fecha de inicio: %s\n", fechaInicio);
    printf("Fecha de fin: %s\n", fechaFin);
    printf("Tipo de operación: %s\n", idTipoOperacion);
    printf("Número de operación: %d\n", noOperacion);
    printf("Importe: %.2f€\n", importe);
    printf("Estado: %s\n", estado);
    printf("\n");
}

void detectarPatrones(Usuario usuarios[MAX_USERS]) {

    FILE *logfile;

    logfile = fopen("file_log.log", "a+");

    // Patrón 1: Más de 5 transacciones en una misma hora para un mismo usuario.
    for (int i = 0; i < MAX_USERS; i++) {
        for (int j = 0; j < MAX_DAYS; j++) {
            for (int k = 0; k < 24; k++) {
                if (usuarios[i].numTransaccionesPorHora[j][k] > 5) {
                    printf("Patrón 1 detectado: Más de 5 transacciones en una misma hora para el usuario %s en el día %d a la hora %d.\n", usuarios[i].idUsuario, j + 1, k);
                    fprintf(logfile, "[%s]Patrón 1 detectado: Más de 5 transacciones en una misma hora para el usuario %s en el día %d a la hora %d.\n", obtenerFechaHora(), usuarios[i].idUsuario, j + 1, k);
                }
            }
        }
    }

    // Patrón 2: Más de 3 retiros a la vez durante un mismo día para un usuario.
    for (int i = 0; i < MAX_USERS; i++) {
        for (int j = 0; j < MAX_DAYS; j++) {
            if (usuarios[i].numRetirosPorDia[j] > 3) {
                printf("Patrón 2 detectado: Más de 3 retiros a la vez para el usuario %s en el día %d.\n", usuarios[i].idUsuario, j + 1);
                fprintf(logfile,"[%s]Patrón 2 detectado: Más de 3 retiros a la vez para el usuario %s en el día %d.\n", obtenerFechaHora(), usuarios[i].idUsuario, j + 1);
            }
        }
    }

    // Patrón 3: Más de 3 errores en un mismo día para un usuario.
    for (int i = 0; i < MAX_USERS; i++) {
        for (int j = 0; j < MAX_DAYS; j++) {
            if (usuarios[i].numErroresPorDia[j] > 3) {
                printf("Patrón 3 detectado: Más de 3 errores para el usuario %s en el día %d.\n", usuarios[i].idUsuario, j + 1);
                fprintf(logfile,"[%s]Patrón 3 detectado: Más de 3 errores para el usuario %s en el día %d.\n", obtenerFechaHora(), usuarios[i].idUsuario, j + 1);
            }
        }
    }

    // Patrón 4: Una operación de cada tipo en un mismo día para un usuario.
    for (int i = 0; i < MAX_USERS; i++) {
        for (int j = 0; j < MAX_DAYS; j++) {
            int contadorTiposOperacion = 0;
            for (int k = 0; k < 5; k++) {
                if (usuarios[i].operacionesPorTipoPorDia[j][k] == 1) {
                    contadorTiposOperacion++;
                }
            }
            if (contadorTiposOperacion == 5) {
                printf("Patrón 4 detectado: Una operación de cada tipo para el usuario %s en el día %d.\n", usuarios[i].idUsuario, j + 1);
                fprintf(logfile,"[%s]Patrón 4 detectado: Una operación de cada tipo para el usuario %s en el día %d.\n", obtenerFechaHora(), usuarios[i].idUsuario, j + 1);
            }
        }
    }

    // Patrón 5: La cantidad de dinero retirado en un día es mayor que el dinero ingresado ese mismo día para un usuario.
    for (int i = 0; i < MAX_USERS; i++) {
        for (int j = 0; j < MAX_DAYS; j++) {
            if (usuarios[i].dineroRetiradoPorDia[j] > usuarios[i].dineroIngresadoPorDia[j]) {
                printf("Patrón 5 detectado: La cantidad de dinero retirado es mayor que el dinero ingresado para el usuario %s en el día %d.\n", usuarios[i].idUsuario, j + 1);
                fprintf(logfile,"[%s]Patrón 5 detectado: La cantidad de dinero retirado es mayor que el dinero ingresado para el usuario %s en el día %d.\n",obtenerFechaHora() , usuarios[i].idUsuario, j + 1);
            }
        }
    }

}

int main() {
    FILE *file;
    char line[MAX_LINE_LENGTH];
    Usuario usuarios[MAX_USERS];
    
    inicializarUsuarios(usuarios);
    
    // Abrir el archivo
    file = fopen("consolidado.csv", "r");
    if (file == NULL) {
        fprintf(stderr, "No se pudo abrir el archivo.\n");
        return 1;
    }

    // Leer y descartar la primera línea del archivo
    fgets(line, MAX_LINE_LENGTH, file); // Esta línea lee la primera línea y la descarta

    // Leer el archivo línea por línea
    while (fgets(line, MAX_LINE_LENGTH, file)) {
        // Procesar cada transacción
        procesarTransaccion(usuarios, line);
    }

    // Cerrar el archivo
    fclose(file);

    // Detectar patrones y mostrar resultados
    detectarPatrones(usuarios);
    
    // Mostrar el número total de operaciones registradas
    printf("Número total de operaciones registradas: %d\n", totalOperacionesRegistradas);

    return 0;
}
