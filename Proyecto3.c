#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define PORT 8889
#define MAX_BUFFER_SIZE 1024

struct FileInfo {
    char filename[256];   
    char action[10];      //Creado, Modificado, Eliminado
    long size;            
    time_t lastModified;  
};

struct DeletedFile { //Aun no estoy segura si lo usare o no
    char filename[256];
    time_t deletionTime;
};

/*
* Función para averiguar si un archivo existe o no, en este caso es utilizado
* con el fin de saber si el cliente o el servidor tiene su respectivo listaArchivos.bin
* cabe de recalcar que listaArchivos es el archivo que contendrá la información de los archivos
* existentes de dicho directorio (cliente o del servidor)
*/
int fileExists(const char *filename) {
    return access(filename, F_OK) != -1;
}

/*
* Función para imprimir el contenido de listaArchivos con su respectiva información
* que en este caso cada archivo registrado contiene el nombre, tamaño del archivo, 
* la última fecha de modificación y la acción realizada (creado, modificado o eliminado)
*/
void printFileList(const char *historyFileName) {
    FILE *history_file = fopen(historyFileName, "rb");
    if (history_file == NULL) {
        perror("Error al abrir el archivo de historial");
        exit(1);
    }

    struct FileInfo file_info_entry;

    while (fread(&file_info_entry, sizeof(struct FileInfo), 1, history_file) == 1) {
        printf("Nombre: %s\n", file_info_entry.filename);
        printf("Tamaño: %ld bytes\n", file_info_entry.size);
        printf("Fecha de Modificación: %s", ctime(&file_info_entry.lastModified));
        printf("Acción: %s\n", file_info_entry.action);
        printf("\n");
    }

    fclose(history_file);
}

/*
* Función para crear el archivo listaArchivos.bin, lo que hace es recorrer cada uno
* de los archivos que se encuentran en el directorio indicado, registrando así su 
* respectiva información. Todos los archivos se marcarán con la acción de "creado".
*/
void createFileList(const char* dirName, const char* historyFileName) {
    DIR *dir;
    struct dirent *dp;
    struct stat file_info;

    dir = opendir(dirName);

    FILE *history_file = fopen(historyFileName, "wb");
    if (history_file == NULL) {
        perror("Error al abrir o crear la lista de los archivos");
        exit(1);
    }

    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || strcmp(dp->d_name, "listaArchivos.bin") == 0 && 
            strcmp(dp->d_name, "listaArchivosLocal.bin") == 0) {
            continue;
        }

        char path[2000];
        snprintf(path, sizeof(path), "%s/%s", dirName, dp->d_name);

        if (stat(path, &file_info) == 0) {
            struct FileInfo file_info_entry;
            strcpy(file_info_entry.filename, dp->d_name);
            file_info_entry.size = file_info.st_size;
            file_info_entry.lastModified = file_info.st_mtime;
            strcpy(file_info_entry.action, "Creado");

            fwrite(&file_info_entry, sizeof(struct FileInfo), 1, history_file);
        }
    }

    fclose(history_file);
    closedir(dir);
}

/*
* Función para actualizar la lista de archivos (listaArchivos.bin)
* Esta es la encargada de comparar los archivos actuales con los que ya se encuentran
* registrados en la lista para saber si hay archivos:
* 
* Modificados (esto se obtiene comparando las fechas de modificación de ambos archivos)
* Creados (si no están registrados en dicha lista)
* Eliminados (si existen en la lista, pero actualmente no existe en el directorio)
*/
void updateFileList(const char* dirName, const char* historyFileName) {
    DIR *dir;
    struct dirent *dp;
    struct stat file_info;

    dir = opendir(dirName);

    FILE *history_file = fopen(historyFileName, "r+b");
    if (history_file == NULL) {
        perror("Error al abrir la lista de archivos");
        exit(1);
    }

    struct FileInfo file_info_entry;

    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || strcmp(dp->d_name, "listaArchivos.bin") == 0) {
            continue;
        }

        char path[2000];
        snprintf(path, sizeof(path), "%s/%s", dirName, dp->d_name);

        if (stat(path, &file_info) == 0) {
            struct FileInfo existing_entry;
            strcpy(existing_entry.filename, dp->d_name);
            existing_entry.size = file_info.st_size;
            existing_entry.lastModified = file_info.st_mtime;
            strcpy(existing_entry.action, "Creado");

            while (fread(&file_info_entry, sizeof(struct FileInfo), 1, history_file) == 1) {
                if (strcmp(existing_entry.filename, file_info_entry.filename) == 0) {
                    if (existing_entry.lastModified != file_info_entry.lastModified) {
                        // El archivo se ha modificado
                        strcpy(existing_entry.action, "Modificado");
                    } else {
                        // El archivo no ha cambiado
                        strcpy(existing_entry.action, "No modificado");
                    }
                    // Actualiza el archivo de historial con la entrada modificada
                    fseek(history_file, -sizeof(struct FileInfo), SEEK_CUR);
                    fwrite(&existing_entry, sizeof(struct FileInfo), 1, history_file);
                    break;
                }
            }
            // Si no se encontró en el historial, es un archivo nuevo
            if (strcmp(existing_entry.action, "Creado") == 0) {
                fseek(history_file, 0, SEEK_END);
                fwrite(&existing_entry, sizeof(struct FileInfo), 1, history_file);
            }
            fseek(history_file, 0, SEEK_SET);
        }
    }

    // Busca archivos en el historial que no estén en el directorio
    while (fread(&file_info_entry, sizeof(struct FileInfo), 1, history_file) == 1) {
        char path[2000];
        snprintf(path, sizeof(path), "%s/%s", dirName, file_info_entry.filename);

        if (access(path, F_OK) == -1) {
            // El archivo no se encontró en el directorio, marca como eliminado
            //Debe de mandarle al servidor que se eliminó x archivo para que
            strcpy(file_info_entry.action, "Eliminado");
            fseek(history_file, -sizeof(struct FileInfo), SEEK_CUR);
            fwrite(&file_info_entry, sizeof(struct FileInfo), 1, history_file);
        }
    }

    fclose(history_file);
    closedir(dir);
}

/*
* Función encargada de limpiar la lista de los archivos del directorio (listaArchivos.bin).
 * Su objetivo es eliminar toda la información relacionada con archivos marcados como "Eliminados",
 * lo que garantiza que el archivo de historial solo contenga datos sobre archivos existentes.
 * Para lograrlo, crea un archivo temporal (temp_file_list.bin) y copia todas las entradas del archivo
 * de historial original que no tengan la acción "Eliminado". Luego, reemplaza el archivo de historial
 * original con el archivo temporal, lo que da como resultado un historial limpio sin información
 * sobre archivos eliminados.
 */
void cleanFileList(const char* historyFileName) {
    FILE *history_file = fopen(historyFileName, "r+b");
    if (history_file == NULL) {
        perror("Error al abrir el archivo de historial");
        exit(1);
    }

    FILE *temp_file = fopen("temp_file_list.bin", "wb");
    if (temp_file == NULL) {
        perror("Error al crear el archivo temporal");
        exit(1);
    }

    struct FileInfo file_info_entry;

    while (fread(&file_info_entry, sizeof(struct FileInfo), 1, history_file) == 1) {
        if (strcmp(file_info_entry.action, "Eliminado") != 0) {
            // Conserva la entrada si la acción no es "Eliminado"
            fwrite(&file_info_entry, sizeof(struct FileInfo), 1, temp_file);
        }
    }

    fclose(history_file);
    fclose(temp_file);

    // Reemplaza el archivo de historial original con el archivo temporal
    remove(historyFileName);
    rename("temp_file_list.bin", historyFileName);
}

void syncDirectories();

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Formato: %s <directorio> o <directorio> <IP>\n", argv[0]);
        return 1;
    }

    char *localDir = argv[1];
    char *remoteIP = argv[2];
    printf("Agumentos entrantes\nArgumento 1: %s\nArgumento 2: %s\n\n", localDir, remoteIP);

    const char *listaArchivosFilename = "listaArchivos.bin";
    char listaArchivosPath[500]; 
    snprintf(listaArchivosPath, sizeof(listaArchivosPath), "%s/%s", localDir, listaArchivosFilename);

    const char *listaArchivosLocalFilename = "listaArchivosLocal.bin";
    char listaArchivosPath[500]; 
    snprintf(listaArchivosPath, sizeof(listaArchivosPath), "%s/%s", localDir, listaArchivosFilename);

    // Comprueba si este programa debe actuar como servidor
    if (argc == 3) {
        // Modo cliente: El tercer argumento contiene la dirección IP del servidor
        printf("Conexion del cliente\n");

        //Verifica si existe el archivo listaArchivos.txt 
        if (fileExists(listaArchivosLocalFilename)) {
            //Si existe solo lo actualiza
            printf("El archivo %s existe en el directorio %s.\n", listaArchivosLocalFilename, localDir);
            printf("Actualizando el archivo de historial...\n");
            updateFileList(localDir, listaArchivosLocalFilename);
            printf("Lista de archivos actualizada en: %s\n\n", listaArchivosLocalFilename);
        } else {
            //Si no existe se crea uno nuevo
            printf("\nEl archivo %s no existe en el directorio %s. \nSe creará el archivo historial.\n", listaArchivosLocalFilename, localDir);
            createFileList(localDir, listaArchivosLocalFilename);
            printf("Lista de archivos creada en: %s\n\n", listaArchivosLocalFilename);
        }

        // syncDirectories(localDir, "", remoteIP);

        int sock;
        struct sockaddr_in server;

        // Create socket
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            printf("Could not create socket");
            return -1.0;
        }
        puts("Socket created");

        server.sin_addr.s_addr = inet_addr(remoteIP);
        server.sin_family = AF_INET;
        server.sin_port = htons(PORT);

        // Connect to the remote server
        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
            perror("Connect failed. Error"); //Si no se conecta lo que hará es que solo actualiza el historial e archivos locales
                                             //Por si hubo alguna modificación, eliminación o creación de nuevos archivos.
            return -1.0;
        }
        puts("Connected");

        while(1){
            
        }

        close(sock);
    } else {
        // Modo servidor: Espera conexiones entrantes
        printf("Conexion del servidor\n");

        //Verifica si existe el archivo listaArchivos.txt 
        if (fileExists(listaArchivosLocalFilename)) {
            //Si existe solo lo actualiza
            printf("El archivo %s existe en el directorio %s.\n", listaArchivosLocalFilename, localDir);
            printf("Actualizando el archivo de historial...\n");
            updateFileList(localDir, listaArchivosLocalFilename);
            printf("Lista de archivos actualizada en: %s\n", listaArchivosLocalFilename);
            //modificados y nuevos 1 funcion para obtener la lista - usan el mismo sendfiles mismo (mandar contenido)
            // crear la lista (se crean dos), enviar, recibir
            //luego manda nombre - lista nueva para los eliminados - diferente solo manda lista

            //send
        } else {
            //Si no existe se crea uno nuevo
            printf("\nEl archivo %s no existe en el directorio %s. \nSe creará el archivo historial.\n", listaArchivosLocalFilename, localDir);
            createFileList(localDir, listaArchivosLocalFilename);
            printf("Lista de archivos creada en: %s\n\n", listaArchivosLocalFilename);
            //allfiles 1 funcion para obtener la lista - usan el mismo send - mismo
        }

        /*printFileList(listaArchivosPath);
        printf("------------------------------------------------------\n");
        printf("Limpiando el archivo de historial...\n");
        cleanFileList(listaArchivosPath);
        printf("Historial limpiado.\n");

        printFileList(listaArchivosPath);*/

        //Configurando la conexión del socket en espera por una petición del cliente
        int serverSocket, clientSocket;
        struct sockaddr_in serverAddr, clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        // Crear el socket del servidor
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == -1) {
            perror("Error al crear el socket del servidor");
            exit(1);
        }
        puts("Socket created");

        // Configurar la dirección del servidor
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(PORT);

        // Vincular el socket a la dirección y puerto
        if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
            perror("bind failed. Error");
            exit(1);
        }
        puts("bind done");

        // Escuchar por conexiones entrantes
        if (listen(serverSocket, 3) == -1) {
            perror("Error al escuchar por conexiones entrantes");
            exit(1);
        }

        printf("Esperando conexiones en el puerto %d...\n\n", PORT);

        while (1) {
            // Aceptar una conexión entrante
            clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
            if (clientSocket == -1) {
                perror("Error al aceptar la conexión del cliente");
                continue;
            }
            printf("Conexion del cliente aceptada\n");
            // Manejar la conexión del cliente en un hilo separado
            //handleClient(clientSocket);
        }

        close(serverSocket);
    }

    return 0;
}