#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

struct FileInfo {
    char filename[256];   
    char action[10];      //Creado, Modificado, Eliminado
    long size;            
    time_t lastModified;  
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
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || strcmp(dp->d_name, "listaArchivos.bin") == 0) {
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

void getFileList(const char *listaArchivosPath, char *fileList, size_t listSize, const char *option, struct FileInfo fileListData[]) {
    int cont = 0;
    memset(fileList, 0, listSize);

    FILE *historyFile = fopen(listaArchivosPath, "rb");
    if (historyFile == NULL) {
        perror("Error al abrir el archivo de historial");
        return;
    }

    struct FileInfo file_info_entry;
    if(strcmp(option, "all") == 0 ){
        while (fread(&file_info_entry, sizeof(struct FileInfo), 1, historyFile) == 1) {
            // Agrega el nombre del archivo a la cadena con una coma
            strncat(fileList, file_info_entry.filename, listSize - strlen(fileList) - 1);
            strncat(fileList, ",", listSize - strlen(fileList) - 1);
            
            strcpy(fileListData[cont].filename, file_info_entry.filename);
            fileListData[cont].lastModified = file_info_entry.lastModified;
            cont++;
        }
    }
    
    fclose(historyFile);
}

void sendFilesListData(int clientSocket, const char *localDir, char *option, const char *listaArchivosPath){
    // Obtén la lista de archivos del directorio local
    int cont = 0;
    char fileList[1024];
    struct FileInfo fileListData[100];
    getFileList(listaArchivosPath,fileList,sizeof(fileList), option, fileListData);

    // Envía la lista de archivos al servidor
    send(clientSocket, fileList, strlen(fileList), 0);

    // Tokeniza la lista de archivos
    char *token = strtok(fileList, ",");
    while (token != NULL) {
        char filePath[500];
        snprintf(filePath, sizeof(filePath), "%s/%s", localDir, token);

        // Envía los archivos al servidor
        FILE *file = fopen(filePath, "rb");
        if (file == NULL) {
            perror("Error al abrir el archivo");
            exit(1);
        }

        // Obtiene la fecha de modificación del archivo
        struct stat file_Info;
        stat(filePath, &file_Info);
        time_t modificationTime = file_Info.st_mtime;
        // Envía la fecha de modificación al servidor
        send(clientSocket, &modificationTime, sizeof(time_t), 0);


        // Obtiene el tamaño del archivo
        long fileSize = (long)file_Info.st_size;
        // Envía el tamaño del archivo al servidor
        send(clientSocket, &fileSize, sizeof(long), 0);

        
        char buffer[fileSize];
        int bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            send(clientSocket, buffer, bytesRead, 0);
        }

        fclose(file);

        printf("Archivo enviado: %s\n", token);

        token = strtok(NULL, ",");
    }

}

time_t getFileInfoModification(const char *historyFileName, const char *fileName) {
    struct FileInfo file_info_entry;

    printf("archivo %s, nombre %s\n", historyFileName, fileName);

    
    FILE *history_file = fopen(historyFileName, "rb");
    if (history_file == NULL) {
        perror("Error al abrir el archivo de historial");
        exit(1);
    }

    
    while (fread(&file_info_entry, sizeof(struct FileInfo), 1, history_file) == 1) {
        if (strcmp(file_info_entry.filename, fileName) == 0) {
            fclose(history_file);
            return file_info_entry.lastModified; //Si lo encuentra retorna su fecha de modificación
        }
    }

    fclose(history_file);
    return -1; // Indica que el archivo no se encontró
}


// Función para recibir un archivo y guardarlo en el servidor
void receiveFile(int socket, const char *filePath, const char *listaArchivosPath, const char *localDir) {
    //Recibe la información
    int bytesRead;
    long fileSize;
    time_t modificationTime;
    recv(socket, &modificationTime, sizeof(time_t), 0);

    char modificationTimeStr[1024];
    strftime(modificationTimeStr, sizeof(modificationTimeStr), "%c", localtime(&modificationTime));
    printf("Fecha: %s\n", modificationTimeStr);

    // Recibe el tamaño del archivo
    recv(socket, &fileSize, sizeof(long), 0);
    printf("Tamaño: %ld bytes\n", fileSize);

    //Verifica si existe o no para el cambio de nombre si hay conflictos de archivos.
    // Obtener nombre y extensión del archivo
    const char *fileName = strrchr(filePath, '/')+1;
    const char *fileExtension = strrchr(fileName, '.');
    time_t fileInfoModifiedDate = getFileInfoModification(listaArchivosPath, fileName);
    /*if (fileInfoModifiedDate == -1) {
        printf("El archivo %s no se encontró en el historial.\n", fileName);
    } else {
        printf("Fecha Archivo entrante: %sFecha archivo lista: %s", ctime(&modificationTime), ctime(&fileInfoModifiedDate));
        if (difftime(modificationTime, fileInfoModifiedDate) > 0){
            printf("El archivo ENTRANTE es el más nuevo\n");
            //strcat(fileName, "New");
            //renameFileInHistory(filePath, fileName, strcat(fileName, "Old"), localDir);
        } else if (difftime(modificationTime, fileInfoModifiedDate) < 0) {
            printf("el archivo de LOCAL es mas nuevo\n");
            //strcat(fileName, "Old");
            //renameFileInHistory(filePath, fileName, strcat(fileName, "New"), localDir);
        } else {
            printf("Los dos son iguales \n");
        }
    }*/

    FILE *file = fopen(filePath, "wb");
    if (file == NULL) {
        perror("Error al crear el archivo");
        return;
    }

    char buffer[fileSize];
    while (fileSize > 0) {
        bytesRead = recv(socket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            break;
        }
        fwrite(buffer, 1, bytesRead, file);
        fileSize -= bytesRead;
    }

    fclose(file);
}

void receiveFilesListData(int clientSocket, const char *localDir, const char *listaArchivosPath){
    // Recibe el nombre de los archivos del cliente
    char filenames[1024];
    int bytesRead = recv(clientSocket, filenames, sizeof(filenames), 0);

    if (bytesRead < 0) {
        perror("Error al recibir los nombres de los archivos");
    } else {
        filenames[bytesRead] = '\0';
        // Tokeniza los nombres de los archivos
        char *token = strtok(filenames, ",");
        while (token != NULL) {
            char filePath[500];
            snprintf(filePath, sizeof(filePath), "%s/%s", localDir, token);

            if (strcmp(token, "listaArchivosTemp.bin") != 0) {
                if (strcmp(token, "listaArchivos.bin") == 0) {
                    snprintf(filePath, sizeof(filePath), "%s/%s", localDir, "listaArchivosTemp.bin");
                }
                receiveFile(clientSocket, filePath, listaArchivosPath, localDir);
                printf("Archivo recibido: %s\n\n", token);
            } else {
                printf("Archivo recibido: %s (ignorado)\n", token);
                long fileSize;
                recv(clientSocket, &fileSize, sizeof(long), 0);
                while (fileSize > 0) {
                    char buffer[8000];
                    int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
                    if (bytesRead <= 0) {
                        break;
                    }
                    fileSize -= bytesRead;
                }
            }

            token = strtok(NULL, ",");
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Formato: %s <directorio> o %s <directorio> <IP>\n", argv[0], argv[0]);
        return 1;
    }

    char *localDir = argv[1];
    char *remoteIP = (argc == 3) ? argv[2] : NULL;

    const char *listaArchivosFilename = "listaArchivos.bin";
    char listaArchivosPath[500]; 
    snprintf(listaArchivosPath, sizeof(listaArchivosPath), "%s/%s", localDir, listaArchivosFilename);

    if (remoteIP == NULL) {
        // Modo servidor
        printf("Modo servidor\n");

        int serverSocket, clientSocket;
        struct sockaddr_in serverAddr, clientAddr;
        socklen_t addrSize = sizeof(clientAddr);

        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == -1) {
            perror("Error al crear el socket");
            return 1;
        }

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(8889);
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
            perror("Error al hacer el enlace");
            return 1;
        }

        if (listen(serverSocket, 1) < 0) {
            perror("Error al escuchar conexiones entrantes");
            return 1;
        }

        printf("Esperando conexiones entrantes...\n\n");

        clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addrSize);
        if (clientSocket < 0) {
            perror("Error al aceptar la conexión entrante");
            return 1;
        }

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

        // Recibe el nombre de los archivos del cliente
        receiveFilesListData(clientSocket, localDir, listaArchivosPath);
        printf("\n");
        sendFilesListData(clientSocket, localDir,"all", listaArchivosPath);

        close(clientSocket);
        close(serverSocket);
    } else {
        // Modo cliente
        printf("Modo cliente\n");

        int clientSocket;
        struct sockaddr_in serverAddr;

        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == -1) {
            perror("Error al crear el socket");
            return 1;
        }

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(8889);
        serverAddr.sin_addr.s_addr = inet_addr(remoteIP);

        if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
            perror("Error al conectarse al servidor");
            return 1;
        }

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

        sendFilesListData(clientSocket, localDir,"all", listaArchivosPath);
        printf("\n");
        receiveFilesListData(clientSocket, localDir, listaArchivosPath);

        close(clientSocket);
    }

    return 0;
}
