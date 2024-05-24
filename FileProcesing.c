#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>

#define MAX_LINE_LENGTH 400 
#define MAX_FILE_COUNT 100 // Máximo número de ficheros por sucursal
#define MAX_SUCURSALES 10 // Máximo número de sucursales
#define MAX_PATH_LENGTH 100000
#define SHM_SIZE 1024 * 1024 // 1MB de memoria compartida

pthread_mutex_t mutex;

int shm_fd;
char *memoria_compartida = NULL;

typedef struct { // Hemos decidido crear un struct para defenir nuestras sucursales, ya que todas van a tener unas misamas caracteristicas.
    int indice; //Asignamos un indice a cada sucursal para así poder diferenciarlas
    int num_ficheros; /* Esta variable la utilizamos para poder saber el numero de ficheros que le corresponden a cada sucursal y asi hacer los bucles en funcion 
                        del numero de ficheros que tenga cada sucursal*/
    char ficheros[MAX_FILE_COUNT][MAX_LINE_LENGTH];/*Para poder procesar los ficheros de cada sucursal necesitamos una forma de almacenar el nombre de los ficheros
    para que asi el programa sepa que ficheros tiene que abrir cuando esta procesando esa sucursal, por eso tenemos esta matriz que almacena los nombres de los ficheros
    de cada sucursal.*/

} Sucursal;

// Estructura para almacenar el consolidado de operaciones bancarias
typedef struct {
    char consolidado[MAX_LINE_LENGTH]; // Suponiendo que el consolidado sea una cadena de caracteres
} Consolidado;

void cleanup(int signum) {
    printf("\nCerrando el programa...\n");

    // Crear el archivo cosolidado.csv y escribir la información
    FILE *todosucurs = fopen("consolidado.csv", "w");
    if (todosucurs != NULL) {
        fprintf(todosucurs, "%s\n", memoria_compartida);
        fclose(todosucurs);
    } else {
        perror("Error al crear el archivo todosucurs");
    }

    // Desvincular y liberar la memoria compartida
    if (munmap(memoria_compartida, SHM_SIZE) == -1) {
        perror("Error al desvincular la memoria compartida");
    }
    close(shm_fd);
    shm_unlink("/my_shared_memory");

    printf("Programa cerrado correctamente.\n");
    exit(EXIT_SUCCESS);
}



/*Las siguientes variables declaradas son las que extraemos de nuestro archivo de configuracion (fp.conf)*/
char line[MAX_LINE_LENGTH];
char PATH_FILES[MAX_LINE_LENGTH];
char INVENTORY_FILE[MAX_LINE_LENGTH];
char LOG_FILE[MAX_LINE_LENGTH];
int NUM_SUCURSALES = 0;
int NUM_PROCESOS = 0;
int SIMULATE_SLEEP;
int SIZE_FP = 0;



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


void LeerConfig(const char *nombreArchivo) {
    char *key, *value;
    FILE *file;

    // Abre el fichero de properties
    file = fopen(nombreArchivo, "r");

    if (!file) { //if para en caso de que no se haya podido abrir el archivo de configuracion
        perror("No se ha podido abrir el archivo");
        exit(EXIT_FAILURE);
    }

    // Lee el fichero línea por línea
    while (fgets(line, MAX_LINE_LENGTH, file)) {
        // Elimina el salto de línea al final, si existe
        line[strcspn(line, "\n")] = 0;

        // Divide la línea en clave y valor
        key = strtok(line, "=");
        value = strtok(NULL, "=");

        // Comprueba si la información es correcta
        if (key && value) {
            if (strcmp(key, "PATH_FILES") == 0) {
                strncpy(PATH_FILES, value, MAX_LINE_LENGTH);
            } else if (strcmp(key, "INVENTORY_FILE") == 0) {
                strncpy(INVENTORY_FILE, value, MAX_LINE_LENGTH);
            } else if (strcmp(key, "LOG_FILE") == 0) {
                strncpy(LOG_FILE, value, MAX_LINE_LENGTH);
            } else if (strcmp(key, "NUM_SUCURSALES") == 0) {
                NUM_SUCURSALES = atoi(value);    
            } else if (strcmp(key, "NUM_PROCESOS") == 0) {
                NUM_PROCESOS = atoi(value);
            } else if (strcmp(key, "SIMULATE_SLEEP") == 0) {
                SIMULATE_SLEEP = atoi(value);
            }else if(strcmp(key, "SIZE_FP") == 0){
                SIZE_FP = atoi(value);
            }
        }
    }

    // Cierra el fichero
    fclose(file);

    printf("---------------------------------------------------------------------\n");

    printf("INFORMACION EXTRAIDA DEl FICHERO DE CONFIGURACION\n\n");

    // Imprime las variables para verificar
    printf("PATH FILE: %s\n", PATH_FILES);
    printf("INVENTORY FILES: %s\n", INVENTORY_FILE);
    printf("LOG FILE: %s\n", LOG_FILE);
    printf("NUM SUCURSALES: %d\n", NUM_SUCURSALES);
    printf("NUM PROCESOS: %d\n", NUM_PROCESOS);
    printf("SIMULATE SLEEP: %d\n", SIMULATE_SLEEP);
    printf("SIZE_FP: %d\n", SIZE_FP);



}

void cargarEstructura() {
    if (mkdir("files_data", 0700) != 0) {
        perror("Error creating directory files_data");
        return;
    }

    for (int i = 1; i <= NUM_SUCURSALES; i++) {
        char path[50];
        snprintf(path, sizeof(path), "files_data/SU00%d", i);
        if (mkdir(path, 0700) != 0) {
            perror("Error creating subdirectory");
            return;
        }
    }
}






void contarFicheros(Sucursal *sucursales) { /*Funcion que se encarga de contar los ficheros que le corresponden a cada sucursal asi como de almacenar los nombres de esos
ficheros dentro de las structuras de las distintas sucursles*/

    DIR *dir; /*Puntero hacia nuestro directorio*/

    struct dirent *entrada;

    dir = opendir(PATH_FILES);/*Funcion que nos devuelve un puntero al directorio que queremos abrir, en nuestro caso la ruta de dicho directorio la tenemos almacenada
    en la variable que hemos sacado de nuestro archivo de configuracion*/

    if (dir == NULL) {/*Comprobamos en caso de que no se halla abierto correctamente el directorio*/
        printf("No se pudo abrir el directorio\n");
        return;
    }

    while ((entrada = readdir(dir)) != NULL) {/*En caso de haber abierto correctamente el directorio vamos a recorrelo entero*/

        if (strcmp(entrada->d_name, ".") != 0 && strcmp(entrada->d_name, "..") != 0) {/*Este If, esta puesto para tener en cuenta el filtrado de lo que se conocen como archivos
        especiales, los cuales serian el directorio actual y el directorio padre, pero como lo que queremos son los archivos de nuestro directorio en que hemos abierto
        tenemos que ignorara todos los archivos que tengan esto en el nombre*/

            int indice_sucursal = atoi(entrada->d_name + 2);/*Vamos a extraer el indice de la sucursal a la cual le pertenece el archivo, dado la estructura de nombreado 
            establecida, para saber la sucursal del archivo sabemos que son los ultimos dos digitos del nombre del archivo de su primera parte, cogemos esos digitos y con la
            funcion atoi convertimos esos dos caracteres en un numero entero y lo guardamos en la variable indice sucursal.*/

            if (indice_sucursal >= 1 && indice_sucursal <= NUM_SUCURSALES) { /*Verificamos que el indice de sucursal extraido del archivo es valido, es decir es vayor o
            igual a 1, y es menor o igual al numero de sucursales establecidos en nuestro fichero de configuracion, ya que asi nos aseguramos de no procesar sucursales
            no establecidas en la configuracion */

                Sucursal *sucursal_actual = &sucursales[indice_sucursal - 1];/*Hacmeos un puntero a la sucursal corrspondiente, de ese fichero mediante el indice del fichero
                que hemos extraido, y el indice de la sucursal*/
                strcpy(sucursal_actual->ficheros[sucursal_actual->num_ficheros], entrada->d_name);/*Copiamos el nombre del fichero extraido completo dentro del array de 
                ficheros de la structura de la sucursal, para ponerlo en la posicion que le corresponde dentro del array hacemos uso de nuestra variable que almacena el
                numero de ficheros de la sucursal, asegurandonos asi guardar los nombres de los archivos dentro del array en orden*/
                sucursal_actual->num_ficheros++;/*incrementamos nuestro numero de ficheros de nuestra sucursal*/

            }
        }
    }

    closedir(dir);/*Finalmente cerramos nuestro directorio*/
}

void inicializarFicheros(){ /*Funcion que se encraga de incializar los ficheros requeridos por el programa*/

    FILE *logFile; /*hacemos dos punteros de tipo FILE, para nuestro ficheros*/



    // Crear el archivo LOG_FILE
    logFile = fopen(LOG_FILE, "a");/*como ruta ponemos la guardada en la variable extraida de nuestro fichero de configuracion*/
    if (logFile == NULL) {/*en caso de que no se haya creado bien nuestro archivo log*/
        printf("Error al abrir el archivo de registro (%s)\n", LOG_FILE);
        return;
    }

    /* Escribir la informacion extraida del fp.conf en el log file, a destacar de esto es que hacemos uso de la funcion obtenerFechaHora(), 
    para que añada en el log la fecha enla que ocurren los eventos*/
    
    fprintf(logFile, "[%s] PATH FILE: %s\n", obtenerFechaHora(), PATH_FILES);
    fprintf(logFile, "[%s] INVENTORY FILES: %s\n", obtenerFechaHora(), INVENTORY_FILE);
    fprintf(logFile, "[%s] LOG FILE: %s\n", obtenerFechaHora(), LOG_FILE);
    fprintf(logFile, "[%s] NUM SUCURSALES: %d\n", obtenerFechaHora(), NUM_SUCURSALES);
    fprintf(logFile, "[%s] NUM PROCESOS: %d\n", obtenerFechaHora(), NUM_PROCESOS);
    fprintf(logFile, "[%s] SIMULATE SLEEP: %d\n", obtenerFechaHora(), SIMULATE_SLEEP);

    fprintf(logFile, "[%s] Archivos creados correctamente:  %s\n", obtenerFechaHora() , LOG_FILE);

    fclose(logFile);/*Cerramos nuestro fichero de log*/

    printf("---------------------------------------------------------------------\n");

    printf("Archivos creados correctamente: %s\n", LOG_FILE);

    printf("---------------------------------------------------------------------\n");

}



void *procesarSucursal(void *arg) {
    Sucursal *sucursal = (Sucursal *)arg;

    printf("Procesando sucursal %d...\n", sucursal->indice);
    
    // Escribir la información en la memoria compartida
    pthread_mutex_lock(&mutex);
    sprintf(memoria_compartida, "[%s] Procesando sucursal : SU00%d\n", obtenerFechaHora(), sucursal->indice);
    pthread_mutex_unlock(&mutex);

    for (int i = 0; i < sucursal->num_ficheros; i++) {
        char rutaFichero[MAX_PATH_LENGTH];
        snprintf(rutaFichero, sizeof(rutaFichero), "%s/%s", PATH_FILES, sucursal->ficheros[i]);
        FILE *ficheroSucursal = fopen(rutaFichero, "r");
        
        if (ficheroSucursal == NULL) {
            printf("No se pudo abrir el fichero %s\n", rutaFichero);
            continue;
        }

        char linea[MAX_LINE_LENGTH];
        if (fgets(linea, MAX_LINE_LENGTH, ficheroSucursal) == NULL) {
            printf("No se pudo leer la primera línea del fichero %s\n", rutaFichero);
            fclose(ficheroSucursal);
            continue;
        }

        pthread_mutex_lock(&mutex);
        while (fgets(linea, MAX_LINE_LENGTH, ficheroSucursal) != NULL) {
            strcat(memoria_compartida, linea); // Concatenar el contenido de linea con la memoria compartida
        }
        pthread_mutex_unlock(&mutex);

        fclose(ficheroSucursal);

        // Construir la ruta de destino
        char rutaDestino[MAX_PATH_LENGTH];
        snprintf(rutaDestino, sizeof(rutaDestino), "files_data/SU00%d/%s", sucursal->indice, sucursal->ficheros[i]);
        // Mover el fichero
        if (rename(rutaFichero, rutaDestino) != 0) {
            fprintf(stderr, "Error moviendo el fichero %s a %s\n", rutaFichero, rutaDestino);
        } else {
            printf("Fichero %s movido a %s\n", rutaFichero, rutaDestino);
        }
    }

    printf("Procesamiento de sucursal %d completado.\n", sucursal->indice);
    pthread_exit(NULL);
}


int main() {

    /*Llamamos primero a la funcion que se encarga de extraer la informacion de nuestro archivo de configuracion*/
    LeerConfig("./conf/fp.conf");

    inicializarFicheros();/*Inicializamos nuestro ficheros*/

    cargarEstructura();/*Cargamos nuestra estructura de ficheros*/

    // Registrar el manejador de señales para SIGINT (Ctrl+C)
    signal(SIGINT, cleanup);

    // Crear la memoria compartida
    shm_fd = shm_open("/my_shared_memory", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error al crear la memoria compartida");
        return EXIT_FAILURE;
    }
    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("Error al truncar la memoria compartida");
        return EXIT_FAILURE;
    }
    memoria_compartida = mmap(0, SHM_SIZE, PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (memoria_compartida == MAP_FAILED) {
        perror("Error al mapear la memoria compartida");
        return EXIT_FAILURE;
    }




    while(1){



    Sucursal sucursales[NUM_SUCURSALES];/*Creamos un array de estrucutras de sucursales para almacenarlas con tamaño igual al NUM_SUCURSALES extraido de la configuracion*/

    for (int i = 0; i < NUM_SUCURSALES; i++) {/*Inicializamos las variable indice y numero de sucursales de todas las sucursales de nuestro array, para que su  indice empieze en 1 y 
    su numero de ficheros sea 0 inicialmente*/
        sucursales[i].indice = i + 1;
        sucursales[i].num_ficheros = 0;
    }


        
        contarFicheros(sucursales);/*Contamos los ficheros de las distintas sucursales pasando el array de sucursales a la funcion*/

        pthread_t hilos[NUM_PROCESOS];/*creamos un array de hilos llamado hilos es funcion del numero de procesos establecidos en el archivo de configuracion*/

        pthread_mutex_init(&mutex,NULL);/*inicializamos nuestro mutex que utilizaremos mas tarde*/

        for (int i = 0; i < NUM_PROCESOS; i++) {/*Bucle que se repite en funcion de numero de procesos, dentro del cual rceamos hilos que ejecuten la funcion procesarSucursal()
        cada hilo recibe como argumento un puntero a una estrucuta Scursal la cual sera la correspondiente dentro del array de sucursales*/
            pthread_create(&hilos[i], NULL, procesarSucursal, (void *)&sucursales[i]);
        }

        for (int i = 0; i < NUM_PROCESOS; i++) {/*Segundo bucle que sirve para esperar a que todos los hilos terminen su ejecucion, para asegurarnos de no avanzar hasta que todos
        los hilos han completado su funcion */
            pthread_join(hilos[i], NULL);
        }

        pthread_mutex_destroy(&mutex);/*Destruimos nuestro mutex para liberar recursos*/

        sleep(SIMULATE_SLEEP);

    }

    return EXIT_SUCCESS;
}
