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
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || strcmp(dp->d_name, "listaArchivos.bin.bin") == 0) {
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
/*
void sendFile(int socket, const char *filePath) {
    FILE *file = fopen(filePath, "rb");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        return;
    }

    char buffer[MAX_BUFFER_SIZE];
    size_t bytesRead;

    // Extrae el nombre del archivo del path
    const char *fileName = strrchr(filePath, '/');
    if (fileName == NULL) {
        fileName = filePath;  // No se encontró una barra diagonal en el path
    } else {
        fileName++;  // Salta la barra diagonal
    }

    // Combina el nombre del archivo y su contenido en un solo buffer
    /*snprintf(buffer, sizeof(buffer), "%s\n", fileName);
    if (send(socket, buffer, strlen(buffer), 0) == -1) {
        perror("Error al enviar el nombre del archivo");
        fclose(file);
        return;
    }*/

    // Envía el contenido del archivo
    /*while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(socket, buffer, bytesRead, 0) == -1) {
            perror("Error al enviar el archivo");
            break;
        }
    }

    printf("Solicitud enviada\n\n");
    fclose(file);
    
}*/

/*char filePath[MAX_BUFFER_SIZE];
    snprintf(filePath, sizeof(filePath), "%s/%s", localDir, fileName);
    printf("Nombre del archivo: %s\n", fileName);
    printf("Contenido del archivo: %s\n", contentBuffer);
    printf("Ruta del registro del archivo entrante: %s\n\n", filePath);"*/
void sendFile(int socket, const char *filePath) {
    FILE *file = fopen(filePath, "rb");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        return;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (fileSize <= 0) {
        perror("Tamaño de archivo no válido");
        fclose(file);
        return;
    }

    char *fileBuffer = (char *)malloc(fileSize);
    if (fileBuffer == NULL) {
        perror("Error al asignar memoria para el archivo");
        fclose(file);
        return;
    }

    if (fread(fileBuffer, 1, fileSize, file) != fileSize) {
        perror("Error al leer el archivo");
        fclose(file);
        free(fileBuffer);
        return;
    }

    if (send(socket, fileBuffer, fileSize, 0) == -1) {
        perror("Error al enviar el archivo");
    } else {
        printf("Archivo enviado: %s\n", filePath);
    }

    fclose(file);
    free(fileBuffer);
}


void createRequestFileList(const char *localDir, const char *listaArchivosServer, const char *listaArchivosClient,const char *solicitudArchivosPath) {
    printf("Arhivo Server %s y archivo cliente %s\n\n", listaArchivosServer, listaArchivosClient);
    FILE *listaArchivosServerFile = fopen(listaArchivosServer, "rb");
    if (listaArchivosServerFile == NULL) {
        perror("Error al abrir el archivo listaArchivos.bin");
        return;
    }
    FILE *listaArchivosTempClientFile = fopen(listaArchivosClient, "rb");
    if (listaArchivosTempClientFile == NULL) {
        perror("Error al abrir el archivo listaArchivosTemp.bin");
        return;
    }
    FILE *solicitudArchivosFile = fopen(solicitudArchivosPath, "wb");
    if (solicitudArchivosFile == NULL) {
        perror("Error al crear el archivo solicitudArchivos.bin");
        fclose(listaArchivosServerFile);
        fclose(listaArchivosTempClientFile);
        return;
    }

    struct FileInfo file_info_entry;
    struct FileInfo file_info_temp;

    while (fread(&file_info_temp, sizeof(struct FileInfo), 1, listaArchivosTempClientFile) == 1) {
        int found = 0;
        // Buscar en listaArchivos.bin si el archivo existe
        rewind(listaArchivosServerFile);
        while (fread(&file_info_entry, sizeof(struct FileInfo), 1, listaArchivosServerFile) == 1) {
            printf("Archivo server %s, archivo cliente %s\n",file_info_temp.filename,file_info_entry.filename );
            if (strcmp(file_info_temp.filename, file_info_entry.filename) == 0) {
                printf("Archivo encontrado server %s, archivo encontrado cliente %s\n\n",file_info_temp.filename,file_info_entry.filename );
                found = 1;
                break;
            }
        }
        if (!found) {
            // El archivo de listaArchivosTemp.bin no se encontró en listaArchivos.bin, por lo que es un archivo faltante
            printf("No se encontró el archivo archivo cliente %s\n\n",file_info_temp.filename );
            fwrite(&file_info_temp, sizeof(struct FileInfo), 1, solicitudArchivosFile);
        }
    }

    fclose(listaArchivosServerFile);
    fclose(listaArchivosTempClientFile);
    fclose(solicitudArchivosFile);
}

int receiveFile(int clientSocket, const char *localDir, const char *listaArchivosServer) {
    
    char nameBuffer[2000];
    int bytesRead;

    // Recibe el nombre del archivo
    bytesRead = recv(clientSocket, nameBuffer, sizeof(nameBuffer), 0);
    if (bytesRead < 0) {
        perror("Error al recibir el nombre del archivo");
        return -1;
    }
    nameBuffer[bytesRead] = '\0';

    printf("Recibí algo\n");
    printf("Info: %d, %s, %s\n",clientSocket,localDir,listaArchivosServer);

    // Extrae el nombre del archivo (token) utilizando el delimitador "\n"
    char *fileName = strtok(nameBuffer, "\n");
    printf("Nombre del archivo recibido: %s\n",fileName);
    char filePath[1024];

    if(strcmp(fileName,"listaArchivos.bin")==0){
        printf("Lo que recibí fue el historial del cliente, significa primera corrida\n");
        strcpy(fileName, "listaArchivosTemp.bin");
        snprintf(filePath, sizeof(filePath), "%s/%s", localDir, fileName);
        printf("Prueba\n");
        // Abre un archivo temporal para escritura y guarda el contenido
        FILE *file = fopen(filePath, "wb");
        if (file == NULL) {
            perror("Error al crear el archivo temporal para guardar el contenido");
            return -1;
        }

        // Recibe y guarda el contenido del archivo
        char buffer[1024];
        while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
            if (fwrite(buffer, 1, bytesRead, file) < bytesRead) {
                perror("Error al escribir el archivo temporal");
                fclose(file);
                return -1;
            }
        }
        fclose(file);
        printf("Nombre del archivo: %s\n", fileName);
        printf("Archivo recibido y guardado en: %s\n\n", filePath);
        // Crea el archivo de solicitudArchivos.bin
        const char *solicitudArchivosFilename = "solicitudArchivos.bin";
        char solicitudArchivosPath[500];
        snprintf(solicitudArchivosPath, sizeof(solicitudArchivosPath), "%s/%s", localDir, solicitudArchivosFilename);
        printf("Llamada a crear solicitud de archivos\n");
        createRequestFileList(localDir, listaArchivosServer,filePath , solicitudArchivosPath);
        printFileList(solicitudArchivosPath);
        sendFile(clientSocket, solicitudArchivosPath);
        printf("Terminé de enviar la solicitud\n");
        //remove(filePath);
    }
    else if(strcmp(fileName,"solicitudArchivos.bin")==0){
        printf("Parte cliente de la primera corrida\n\n");
    }

    //snprintf(filePath, sizeof(filePath), "%s/%s", localDir, fileName);
    return 0;
}

void syncDirectoriesClient(const char* localDir, const char* remoteIP, const char* listaArchivosPath) {
    int sock;
    struct sockaddr_in server;

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("Could not create socket");
        exit(1);
    }
    puts("Socket created");

    server.sin_addr.s_addr = inet_addr(remoteIP);
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    // Connect to the remote server
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Connect failed. Error"); //Si no se conecta lo que hará es que solo actualiza el historial e archivos locales
                                            //Por si hubo alguna modificación, eliminación o creación de nuevos archivos.
        exit(1);
    }
    puts("Connected");

    //Verifica si existe el archivo listaArchivos.txt 
    if (fileExists(listaArchivosPath)) {
        //Si existe solo lo actualiza
        printf("El archivo existe en la ruta %s.\n", listaArchivosPath);
        printf("Actualizando el archivo de historial...\n");
        //updateFileList(localDir, listaArchivosPath);
        printf("Lista de archivos actualizada en: %s\n\n", listaArchivosPath);
    } else {
        //Si no existe se crea uno nuevo
        printf("\nEl archivo %s no existe en el directorio %s. \nSe creará el archivo historial.\n", listaArchivosPath, localDir);
        createFileList(localDir, listaArchivosPath);
        printf("Lista de archivos creada en: %s\n\n", listaArchivosPath);
        //Envia la lista de archivos al servidor
        //sendFile(sock, listaArchivosPath);
        //syncDirectories
    }

    //Envia la lista de archivos al servidor
    sendFile(sock, listaArchivosPath);
    char server_reply[2000];
    // Receive the response
    if (recv(sock, server_reply, sizeof(server_reply), 0) < 0) {
        puts("Receive failed");
        exit(1);
    }
    printf("server_reply: %s", server_reply);

    // Ahora puedes recibir los archivos del servidor
    /*if (receiveFile(sock, localDir, listaArchivosPath) < 0) {
        perror("Recepción de archivos fallida");
    }*/

    close(sock);
};

void syncDirectoriesServer(const char* localDir, const char* remoteIP, const char* listaArchivosPath) {
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
    puts("\nSocket created");

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
        printf("Conexion del cliente aceptada\n\n");

        // Manejar las peticiones del cliente
        //receiveFile(clientSocket, localDir,listaArchivosPath);
        /*if (receiveFile(clientSocket, localDir,listaArchivosPath) < 0) {
            perror("Recepción fallida");
            continue;
        }*/
        int bytesRead;
        char client_message[2000];
        if ((bytesRead = recv(clientSocket, client_message, sizeof(client_message), 0)) < 0) {
            perror("Recepción fallida");
            continue;
        }

        
        // Extrae el nombre del archivo (token) utilizando el delimitador "\n"
        char *fileName = strtok(client_message, "\n");
        printf("Nombre del archivo recibido: %s\n",client_message);
        char filePath[1024];
        /*char result_message[2000];
        snprintf(result_message, sizeof(result_message), "%s", "adasdas");
        // Enviar el resultado de vuelta al cliente
        if (send(clientSocket, result_message, strlen(result_message), 0) < 0) {
            perror("Envío fallido");
        }*/
        if(strcmp(fileName,"listaArchivos.bin")==0){
        printf("Lo que recibí fue el historial del cliente, significa primera corrida\n");
        strcpy(fileName, "listaArchivosTemp.bin");
        snprintf(filePath, sizeof(filePath), "%s/%s", localDir, fileName);
        printf("Prueba\n");
        // Abre un archivo temporal para escritura y guarda el contenido
        FILE *file = fopen(filePath, "wb");
        if (file == NULL) {
            perror("Error al crear el archivo temporal para guardar el contenido");
            exit(1);
        }
        printf("Prueba2\n");
        // Recibe y guarda el contenido del archivo
        char buffer[1024];
        while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
            if (fwrite(buffer, 1, bytesRead, file) < bytesRead) {
                perror("Error al escribir el archivo temporal");
                fclose(file);
                exit(1);
            }
        }
        fclose(file);
        printf("Nombre del archivo: %s\n", fileName);
        printf("Archivo recibido y guardado en: %s\n\n", filePath);
        // Crea el archivo de solicitudArchivos.bin
        const char *solicitudArchivosFilename = "solicitudArchivos.bin";
        char solicitudArchivosPath[500];
        snprintf(solicitudArchivosPath, sizeof(solicitudArchivosPath), "%s/%s", localDir, solicitudArchivosFilename);
        printf("Llamada a crear solicitud de archivos\n");
        createRequestFileList(localDir, listaArchivosPath,filePath , solicitudArchivosPath);
        printFileList(solicitudArchivosPath);
        sendFile(clientSocket, solicitudArchivosPath);
        printf("Terminé de enviar la solicitud\n");
        //remove(filePath);
    }
        //sendFile(clientSocket, listaArchivosPath);
        //handleClient(clientSocket);
    }

    close(serverSocket);
};

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

    // Comprueba si este programa debe actuar como servidor
    if (argc == 3) {
        // Modo cliente: El tercer argumento contiene la dirección IP del servidor
        printf("Conexion del cliente\n");
        // syncDirectories(localDir, "", remoteIP);
        syncDirectoriesClient(argv[1], argv[2], listaArchivosPath);
        
    } else {
        // Modo servidor: Espera conexiones entrantes
        printf("Conexion del servidor\n");

        //Verifica si existe el archivo listaArchivos.txt 
        if (fileExists(listaArchivosPath)) {
            //Si existe solo lo actualiza
            printf("El archivo %s existe en el directorio %s.\n", listaArchivosFilename, localDir);
            printf("Actualizando el archivo de historial...\n");
            //updateFileList(localDir, listaArchivosPath);
            printf("Lista de archivos actualizada en: %s\n", listaArchivosPath);
        } else {
            //Si no existe se crea uno nuevo
            printf("\nEl archivo %s no existe en el directorio %s. \nSe creará el archivo historial.\n", listaArchivosPath, localDir);
            createFileList(localDir, listaArchivosPath);
            printf("Lista de archivos creada en: %s\n", listaArchivosPath);
        }

        syncDirectoriesServer(argv[1], "127.0.0.1", listaArchivosPath);

        /*printFileList(listaArchivosPath);
        printf("------------------------------------------------------\n");
        printf("Limpiando el archivo de historial...\n");
        cleanFileList(listaArchivosPath);
        printf("Historial limpiado.\n");

        printFileList(listaArchivosPath);*/

        
    }

    return 0;
}