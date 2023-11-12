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
    char action[100];      //Creado, Modificado, Eliminado
    long size;            
    time_t lastModified;  
};

struct RenameFile {
    char oldPath[256];   
    char newPath[256];
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
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || strcmp(dp->d_name, "listaArchivos.bin") == 0
        || strcmp(dp->d_name, "listaArchivosLocal.bin") == 0 ) {
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

void updateFileInHistory(const char *filePath, time_t *lastModified, const char *fileName, long fileSize, char *action) {
    FILE *history_file = fopen(filePath, "r+b");
    if (history_file == NULL) {
        perror("Error al abrir el archivo de historial");
        exit(1);
    }

    struct FileInfo file_info_entry;

    while (fread(&file_info_entry, sizeof(struct FileInfo), 1, history_file) == 1) {
        if (strcmp(file_info_entry.filename, fileName) == 0) {
            // Encontramos la entrada que deseamos actualizar
            file_info_entry.size = fileSize;
            file_info_entry.lastModified = *lastModified;
            strcpy(file_info_entry.action, action);
            
            fseek(history_file, -sizeof(struct FileInfo), SEEK_CUR);
            fwrite(&file_info_entry, sizeof(struct FileInfo), 1, history_file);
            break; // Sal del bucle una vez que se haya encontrado y actualizado el archivo.
        }
    }

    fclose(history_file);
}

void insertFileInfo(const char *historyFileName, const struct FileInfo *newEntry) {
    FILE *historyFile = fopen(historyFileName, "ab");  // Abre el archivo en modo binario para añadir al final

    if (historyFile == NULL) {
        perror("Error al abrir el archivo de historial");
        exit(1);
    }

    // Escribe el nuevo struct FileInfo al final del archivo
    fwrite(newEntry, sizeof(struct FileInfo), 1, historyFile);

    fclose(historyFile);
}

time_t getFileInfoModification(const char *historyFileName, const char *fileName) {
    struct FileInfo file_info_entry;
    
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


/*
* Función para actualizar la lista de archivos (listaArchivos.bin)
* Esta es la encargada de comparar los archivos actuales con los que ya se encuentran
* registrados en la lista para saber si hay archivos:
* 
* Modificados (esto se obtiene comparando las fechas de modificación de ambos archivos)
* Creados (si no están registrados en dicha lista)
* Eliminados (si existen en la lista, pero actualmente no existe en el directorio)
*/
void updateFileList(const char* dirName, const char* historyFileName, const char *listaArchivosSinc, char *deletedFiles) {
    size_t listSize = sizeof(deletedFiles);
    memset(deletedFiles, 0, listSize);
    time_t time;

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
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || strcmp(dp->d_name, "listaArchivos.bin") == 0
        || strcmp(dp->d_name, "listaArchivosLocal.bin") == 0 ) {
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
                    if (existing_entry.lastModified != file_info_entry.lastModified && 
                        existing_entry.size != file_info_entry.size) {
                        // El archivo se ha modificado
                        strcpy(existing_entry.action, "Modificado");
                        existing_entry.lastModified = file_info_entry.lastModified;
                        updateFileInHistory(listaArchivosSinc, &existing_entry.lastModified, file_info_entry.filename, 
                        existing_entry.size, "Modificado");
                    } else {
                        // El archivo no ha cambiado
                        strcpy(existing_entry.action, "No modificado");
                        updateFileInHistory(listaArchivosSinc, &file_info_entry.lastModified, file_info_entry.filename, 
                        file_info_entry.size, "No modificado");
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

                time = getFileInfoModification(listaArchivosSinc, existing_entry.filename);
                if(time == -1){
                    insertFileInfo(listaArchivosSinc, &existing_entry);
                }
                
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

            updateFileInHistory(listaArchivosSinc, &file_info_entry.lastModified, file_info_entry.filename, 
            file_info_entry.size, "Eliminado");

            strcat(deletedFiles, file_info_entry.filename);
            strcat(deletedFiles, ",");
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
void cleanFileList(const char* historyFileName, const char *localDir) {
    const char *tempFile = "temp_file_list.bin";
    char tempFilePath[500]; 
    snprintf(tempFilePath, sizeof(tempFilePath), "%s/%s", localDir, tempFile);

    FILE *history_file = fopen(historyFileName, "r+b");
    if (history_file == NULL) {
        perror("Error al abrir el archivo de historial");
        exit(1);
    }

    FILE *temp_file = fopen(tempFilePath, "wb");
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
    rename(tempFilePath, historyFileName);
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
            if(strcmp(file_info_entry.action, "Eliminado") != 0){
                // Agrega el nombre del archivo a la cadena con una coma
                strncat(fileList, file_info_entry.filename, listSize - strlen(fileList) - 1);
                strncat(fileList, ",", listSize - strlen(fileList) - 1);
                
                strcpy(fileListData[cont].filename, file_info_entry.filename);
                strcpy(fileListData[cont].action, file_info_entry.action);
                cont++;
            }
        }
    } else {
        while (fread(&file_info_entry, sizeof(struct FileInfo), 1, historyFile) == 1) {
            if(strcmp(file_info_entry.action, "Modificado") == 0 || strcmp(file_info_entry.action, "Creado") == 0){
                // Agrega el nombre del archivo a la cadena con una coma
                strncat(fileList, file_info_entry.filename, listSize - strlen(fileList) - 1);
                strncat(fileList, ",", listSize - strlen(fileList) - 1);
                
                strcpy(fileListData[cont].filename, file_info_entry.filename);
                strcpy(fileListData[cont].action, file_info_entry.action);
                cont++;
            }
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
        /*time_t modificationTime = file_Info.st_mtime;
        // Envía la fecha de modificación al servidor
        send(clientSocket, &fileListData[cont].lastModified, sizeof(time_t), 0);*/
        //Envia la accion
        char action[100];
        strcpy(action, fileListData[cont].action);
        send(clientSocket, &action, sizeof(action), 0);

        // Obtiene el tamaño del archivo
        long fileSize = (long)file_Info.st_size;
        // Envía el tamaño del archivo al servidor
        send(clientSocket, &fileSize, sizeof(long), 0);

        
        char buffer[8000];
        int bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            send(clientSocket, buffer, bytesRead, 0);
        }

        fclose(file);

        printf("Archivo enviado: %s\nTamaño: %ld bytes\nAccion: %s\n", token, fileSize, action);

        
        char ackMessage[4];
        recv(clientSocket, ackMessage, sizeof(ackMessage), 0);

        if (strcmp(ackMessage, "ACK") == 0) {
            printf("Operación confirmada por el servidor.\n\n");
        } else {
            //fprintf(stderr, "Error: Falta de confirmación del servidor o mensaje de error.\n");
        }
        cont++;
        token = strtok(NULL, ",");
    }

}


void renameFileInHistory(const char *listaArchivos, const char *filePath, const char *oldFileName, 
                        const char *newFileName) {
    FILE *history_file = fopen(listaArchivos, "r+b");
    if (history_file == NULL) {
        perror("Error al abrir el archivo de historial");
        exit(1);
    }
    struct FileInfo file_info_entry;

    while (fread(&file_info_entry, sizeof(struct FileInfo), 1, history_file) == 1) {
        printf("nombre lista %s, nombre a buscar %s\n", file_info_entry.filename, oldFileName);
        if (strcmp(file_info_entry.filename, oldFileName) == 0) {
            // Encontramos la entrada que deseamos renombrar
            strcpy(file_info_entry.filename, newFileName);
            fseek(history_file, -sizeof(struct FileInfo), SEEK_CUR);
            fwrite(&file_info_entry, sizeof(struct FileInfo), 1, history_file);
            
            break; // Sal del bucle una vez que se haya encontrado y actualizado el archivo.
        }
    }
    fclose(history_file);
}

long getFileInfoSize(const char *historyFileName, const char *fileName) {
    struct FileInfo file_info_entry;
    
    FILE *history_file = fopen(historyFileName, "rb");
    if (history_file == NULL) {
        perror("Error al abrir el archivo de historial");
        exit(1);
    }
    
    while (fread(&file_info_entry, sizeof(struct FileInfo), 1, history_file) == 1) {
        if (strcmp(file_info_entry.filename, fileName) == 0) {
            fclose(history_file);
            return file_info_entry.size; //Si lo encuentra retorna su fecha de modificación
        }
    }

    fclose(history_file);
    return -1; // Indica que el archivo no se encontró
}

void addNewRenameFile(struct RenameFile *listRenameFiles, int *currentIndex, const char *filePath, const char *newFilePath){
    if (*currentIndex < 100) {
        // Copia los nuevos datos en la posición actual del array
        snprintf(listRenameFiles[*currentIndex].oldPath, sizeof(listRenameFiles[*currentIndex].oldPath), "%s", filePath);
        snprintf(listRenameFiles[*currentIndex].newPath, sizeof(listRenameFiles[*currentIndex].newPath), "%s", newFilePath);
        
        // Incrementa el índice para apuntar al siguiente espacio disponible
        (*currentIndex)++;
    } else {
        printf("El array de renombrar está lleno.\n");
    }
}


struct FileInfo getFileStruct(const char *listaArchivosPath, const char *fileName){
    struct FileInfo file_info_entry;
    
    FILE *history_file = fopen(listaArchivosPath, "rb");
    if (history_file == NULL) {
        perror("Error al abrir el archivo de historial");
        exit(1);
    }
    
    while (fread(&file_info_entry, sizeof(struct FileInfo), 1, history_file) == 1) {
        if (strcmp(file_info_entry.filename, fileName) == 0) {
            fclose(history_file);
            return file_info_entry; //Si lo encuentra retorna su fecha de modificación
        }
    }

    fclose(history_file);
}

// Función para recibir un archivo y guardarlo en el servidor
void receiveFile(int socket, const char *filePath, const char *listaArchivosPath, const char *localDir, const char *listaLocalPath,
                 struct RenameFile *listRenameFiles, int *currentIndex, const char *option) {

    char filePathCopy[strlen(filePath) + 1];
    char filePathCopy2[strlen(filePath) + 1];
    strcpy(filePathCopy, filePath);

    //Recibe la información
    int bytesRead;
    long fileSize;
    char action[100];
    time_t modificationTime;
    recv(socket, &action, sizeof(action), 0);

    printf("Accion: %s\n", action);

    // Recibe el tamaño del archivo
    recv(socket, &fileSize, sizeof(long), 0);
    printf("Tamaño: %ld bytes\n", fileSize);

    //Verifica si existe o no para el cambio de nombre si hay conflictos de archivos.
    // Obtener nombre y extensión del archivo
    const char *fileName = strrchr(filePath, '/')+1;
    const char *fileExtension = strrchr(fileName, '.');

    long fileInfoSize = getFileInfoSize(listaArchivosPath, fileName);
    if (fileInfoSize == -1) {
        printf("El archivo %s no se encontró en el historial.\n", fileName);
        struct FileInfo newFileInfo;

        strcpy(newFileInfo.filename, fileName);
        newFileInfo.size = fileSize;
        newFileInfo.lastModified = modificationTime;
        strcpy(newFileInfo.action, "Creado");

        insertFileInfo(listaArchivosPath, &newFileInfo);
    } else {
        struct FileInfo localFileStruct = getFileStruct(listaArchivosPath,fileName);
        printf("Accion entrante: %s Accion lista: %s\n", action, localFileStruct.action);
        printf("Tamaño Archivo entrante: %ld Tamaño archivo lista: %ld\n", fileSize, fileInfoSize);
        if (strcmp(action,"Modificado") == 0 && strcmp(localFileStruct.action,"Modificado") == 0){
            printf("El archivo ENTRANTE es el más nuevo\n");
            // Calcular la longitud del nombre del archivo sin la extensión
            size_t name_length = fileExtension - fileName;

            // Crear una nueva cadena para el nuevo nombre de archivo
            char new_name1[200];  // +4 para Old/New y el terminador nulo Entrante
            strncpy(new_name1, fileName, name_length);
            strcpy(new_name1 + name_length, "Cliente");
            strcat(new_name1, fileExtension);

            char new_name2[200];  // +4 para Old/New y el terminador nulo Local
            strncpy(new_name2, fileName, name_length);
            strcpy(new_name2 + name_length, "Servidor");
            strcat(new_name2, fileExtension);

            if(strcmp(option, "Server") == 0){
                char newFilePath1[500]; 
                snprintf(newFilePath1, sizeof(newFilePath1), "%s/%s", localDir, new_name2);
                addNewRenameFile(listRenameFiles, currentIndex, filePath, newFilePath1);

                char newFilePath2[500]; 
                snprintf(newFilePath2, sizeof(newFilePath2), "%s/%s", localDir, new_name1);
                strcpy(filePathCopy, newFilePath2);

            } else if (strcmp(option, "Client") == 0) {
                char newFilePath1[500]; 
                snprintf(newFilePath1, sizeof(newFilePath1), "%s/%s", localDir, new_name1);
                addNewRenameFile(listRenameFiles, currentIndex, filePath, newFilePath1);

                char newFilePath2[500]; 
                snprintf(newFilePath2, sizeof(newFilePath2), "%s/%s", localDir, new_name2);
                strcpy(filePathCopy, newFilePath2);
            }
            
        } else if (strcmp(action,"No Modificado") == 0 && strcmp(localFileStruct.action,"Modificado") == 0 ||
                   strcmp(action,"No Modificado") == 0 && strcmp(localFileStruct.action,"No Modificado") == 0 ) {
            char buffer[8000];
            while (fileSize > 0) {
                bytesRead = recv(socket, buffer, sizeof(buffer), 0);
                if (bytesRead <= 0) {
                    break;
                }
                fileSize -= bytesRead;
            }
            send(socket, "ACK", strlen("ACK"), 0);
            return;
        } else if (strcmp(action,"Creado") == 0 || strcmp(action,"Modificado") == 0 && strcmp(localFileStruct.action,"No Modificado")) {
            printf("El archivo local no se modificó entonces le cae encima.\n");
        }
    }

    FILE *file = fopen(filePathCopy, "wb");
    if (file == NULL) {
        perror("Error al crear el archivo");
        return;
    }

    char buffer[8000];
    while (fileSize > 0) {
        bytesRead = recv(socket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            break;
        }
        fwrite(buffer, 1, bytesRead, file);
        fileSize -= bytesRead;
    }

    fclose(file);

    send(socket, "ACK", strlen("ACK"), 0);
}

void receiveFilesListData(int clientSocket, const char *localDir, const char *listaArchivosPath, const char *listaLocalPath,
                          struct RenameFile *listRenameFiles, int *currentIndex, const char *option){
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
                receiveFile(clientSocket, filePath, listaArchivosPath, localDir, listaLocalPath, listRenameFiles, currentIndex, option);
                printf("Archivo recibido: %s\n\n", token);
            }
            

            token = strtok(NULL, ",");
        }
    }
}

void copyFile(const char *sourceFileName, const char *destinationFileName) {
    printf("probando %s\n tempotal %s\n", sourceFileName, destinationFileName);
    FILE *sourceFile = fopen(sourceFileName, "rb");
    if (sourceFile == NULL) {
        perror("Error al abrir el archivo de origen");
        exit(EXIT_FAILURE);
    }

    FILE *destinationFile = fopen(destinationFileName, "wb");
    if (destinationFile == NULL) {
        fclose(sourceFile);
        perror("Error al abrir o crear el archivo de destino");
        exit(EXIT_FAILURE);
    }

    struct FileInfo fileInfo;

    // Leer cada entrada del archivo original y escribir en el nuevo archivo
    while (fread(&fileInfo, sizeof(struct FileInfo), 1, sourceFile) == 1) {
        fwrite(&fileInfo, sizeof(struct FileInfo), 1, destinationFile);
    }

    // Cerrar archivos
    fclose(sourceFile);
    fclose(destinationFile);
}

void updateFileAction(const char *filePath, const char *fileName, char *action ){
    FILE *history_file = fopen(filePath, "r+b");
    if (history_file == NULL) {
        perror("Error al abrir el archivo de historial");
        exit(1);
    }

    struct FileInfo file_info_entry;

    while (fread(&file_info_entry, sizeof(struct FileInfo), 1, history_file) == 1) {
        if (strcmp(file_info_entry.filename, fileName) == 0) {
            // Encontramos la entrada que deseamos actualizar
            strcpy(file_info_entry.action, action);
            
            fseek(history_file, -sizeof(struct FileInfo), SEEK_CUR);
            fwrite(&file_info_entry, sizeof(struct FileInfo), 1, history_file);
            break; // Sal del bucle una vez que se haya encontrado y actualizado el archivo.
        }
    }

    fclose(history_file);
}

void receiveDeleteFiles(int clientSocket, const char *localDir, const char *listaArchivosPath, const char *listaLocalPath){
    // Recibe el nombre de los archivos del cliente
    char filenames[1024];
    int bytesRead = recv(clientSocket, filenames, sizeof(filenames), 0);

    if (bytesRead < 0) {
        printf("Error al recibir los nombres de los archivos eliminados");
    } else {
        filenames[bytesRead] = '\0';
        // Tokeniza los nombres de los archivos
        char *token = strtok(filenames, ",");
        while (token != NULL) {
            char filePath[500];
            snprintf(filePath, sizeof(filePath), "%s/%s", localDir, token);
            printf("ARCHIVO A ELIMINAR %s\n", filePath);
            remove(filePath);
            updateFileAction(listaArchivosPath, token, "Eliminado");
            updateFileAction(listaLocalPath, token, "Eliminado");
            token = strtok(NULL, ",");
        }
        printf("Lista Archivos---------------------\n");
        printFileList(listaArchivosPath);
        printf("Lista local---------------------\n");
        printFileList(listaLocalPath);

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

    /*const char *listaArchivosTemp = "listaArchivosTemp.bin";
    char listaArchivosTempPath[500]; 
    snprintf(listaArchivosTempPath, sizeof(listaArchivosTempPath), "%s/%s", localDir, listaArchivosTemp);*/
    

    const char *listaArchivosLocalFilename = "listaArchivosLocal.bin";
    char listaArchivosLocalPath[500]; 
    snprintf(listaArchivosLocalPath, sizeof(listaArchivosLocalPath), "%s/%s", localDir, listaArchivosLocalFilename);

    /*const char *listaArchivosLocalTemp = "listaArchivosLocalTemp.bin";
    char listaArchivosLocalTempPath[500]; 
    snprintf(listaArchivosLocalTempPath, sizeof(listaArchivosLocalTempPath), "%s/%s", localDir, listaArchivosLocalTemp);*/
    
    struct RenameFile listRenameFiles[100];
    int currentIndex = 0;

    char listaArchivosEliminados[1024] = {0};
    memset(listaArchivosEliminados, 0, sizeof(listaArchivosEliminados));

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

        //Verifica si existe el archivo listaArchivosLocal.bin 
        if (fileExists(listaArchivosLocalPath)) {
            //Si existe solo lo actualiza
            printf("El archivo %s existe en el directorio %s.\n", listaArchivosLocalFilename, localDir);
            printf("Actualizando el archivo de historial...\n");
            updateFileList(localDir, listaArchivosLocalPath, listaArchivosPath, listaArchivosEliminados);
            printFileList(listaArchivosPath);
            printFileList(listaArchivosLocalPath);
            printf("Lista de archivos actualizada en: %s\n\n", listaArchivosLocalPath);
        } else {
            //Si no existe se crea uno nuevo
            printf("\nEl archivo %s no existe en el directorio %s. \nSe creará el archivo historial.\n", listaArchivosLocalPath, localDir);
            createFileList(localDir, listaArchivosLocalPath);
            createFileList(localDir, listaArchivosPath);
            printf("Lista de archivos creada en: %s\n\n", listaArchivosLocalPath);
        }

        /*copyFile(listaArchivosPath, listaArchivosTempPath);
        copyFile(listaArchivosLocalPath, listaArchivosLocalTempPath);*/

        // Recibe el nombre de los archivos del cliente
        //receiveFilesListData(clientSocket, localDir, listaArchivosTempPath, listaArchivosLocalTempPath);
        printf("----------------------------Lista Eliminados Cliente ------------------------------\n\n");
        receiveDeleteFiles(clientSocket, localDir, listaArchivosPath, listaArchivosLocalPath);
        send(clientSocket, "LCK", strlen("LCK"), 0);

        printf("----------------------------Lista Eliminados Servidor------------------------------\n\n");
        printf("Lista eliminados: %s\n", listaArchivosEliminados);
        send (clientSocket, listaArchivosEliminados, sizeof(listaArchivosEliminados), 0);
        send(clientSocket, "OOK", strlen("OOK"), 0);
    


        receiveFilesListData(clientSocket, localDir, listaArchivosPath, listaArchivosLocalPath, listRenameFiles, &currentIndex, "Server");
        char ackMessage[4];
        recv(clientSocket, ackMessage, sizeof(ackMessage), 0);
        if (strcmp(ackMessage, "SCK") == 0) {
            printf("Operación confirmada por el servidor Me toca enviar archivos.\n\n");
        } else {
            //fprintf(stderr, "Error: Falta de confirmación del servidor o mensaje de error.\n");
        }

        printf("\n");

        printf("-------------------------------------------------------------------------\n\n");
        
        sendFilesListData(clientSocket, localDir,"all", listaArchivosPath);
        updateFileList(localDir, listaArchivosLocalPath, listaArchivosPath, listaArchivosEliminados);
        send(clientSocket, "MCK", strlen("MCK"), 0);

        printf("-------------------------------------------------------------------------\n\n");
        //printFileList(listaArchivosLocalTempPath);
        printf("-------------------------------------------------------------------------\n\n");
        //printFileList(listaArchivosTempPath);
        printf("index %d\n", currentIndex);
        for (int i = 0; i < currentIndex; ++i) {
            printf("Elemento %d:\n", i);
            printf("Old Path: %s\n", listRenameFiles[i].oldPath);
            printf("New Path: %s\n", listRenameFiles[i].newPath);
            printf("\n");

            rename(listRenameFiles[i].oldPath, listRenameFiles[i].newPath);
            renameFileInHistory(listaArchivosPath, listRenameFiles[i].oldPath, strrchr(listRenameFiles[i].oldPath, '/')+1, 
            strrchr(listRenameFiles[i].newPath, '/')+1);
            renameFileInHistory(listaArchivosLocalPath, listRenameFiles[i].oldPath, strrchr(listRenameFiles[i].oldPath, '/')+1, 
            strrchr(listRenameFiles[i].newPath, '/')+1);
        }

        updateFileList(localDir, listaArchivosLocalPath, listaArchivosPath, listaArchivosEliminados);
        printf("--Lista Archivos-----------------------------------------------------------------------\n\n");
        printFileList(listaArchivosPath);
        printf("--Lista LOCAL-----------------------------------------------------------------------\n\n");
        printFileList(listaArchivosLocalPath);
        /*char prueba[1024] = {0};
        memset(prueba, 0, sizeof(prueba));
        struct FileInfo fileListData[100];
        getFileList(listaArchivosPath, prueba, sizeof(prueba), "not all",fileListData);
        if(strlen(prueba) > 0){
            printf("Lista Obtenida: %s\n", prueba);
        } else {
            printf("No hay modificaciones ni nuevos archivos, no es necesario mandar ningún archivo escribir: %s\n", prueba);
        }*/
        
        /*printf("----------------------------Lista Eliminados------------------------------\n\n");
        printf("Lista eliminados: %s\n", listaArchivosEliminados);
        cleanFileList(listaArchivosLocalPath);
        cleanFileList(listaArchivosPath);
        printf("-------------------------------------------------------------------------\n\n");
        printFileList(listaArchivosLocalPath);
        printf("-------------------------------------------------------------------------\n\n");
        printFileList(listaArchivosPath);*/
        

        cleanFileList(listaArchivosLocalPath, localDir);
        cleanFileList(listaArchivosPath, localDir);

        close(clientSocket);
        close(serverSocket);
    } else {
        // Modo cliente
        printf("Modo cliente\n\n");

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

        //Verifica si existe el archivo listaArchivosLocal.bin 
        if (fileExists(listaArchivosLocalPath)) {
            //Si existe solo lo actualiza
            printf("El archivo %s existe en el directorio %s.\n", listaArchivosLocalFilename, localDir);
            printf("Actualizando el archivo de historial...\n");
            updateFileList(localDir, listaArchivosLocalPath, listaArchivosPath, listaArchivosEliminados);
            printf("----------------------------Lista aRCHIVOS------------------------------\n\n");
            printFileList(listaArchivosPath);
            printf("----------------------------Lista Local------------------------------\n\n");
            printFileList(listaArchivosLocalPath);
            printf("Lista de archivos actualizada en: %s\n\n", listaArchivosLocalPath);
        } else {
            //Si no existe se crea uno nuevo
            printf("\nEl archivo %s no existe en el directorio %s. \nSe creará el archivo historial.\n", listaArchivosLocalPath, localDir);
            createFileList(localDir, listaArchivosLocalPath);
            createFileList(localDir, listaArchivosPath);
            printf("Lista de archivos creada en: %s\n\n", listaArchivosLocalPath);
        }
        printf("----------------------------Lista Eliminados Cliente------------------------------\n\n");
        printf("Lista eliminados: %s\n", listaArchivosEliminados);
        send (clientSocket, listaArchivosEliminados, sizeof(listaArchivosEliminados), 0);
        
        char ackMessage2[4];
        recv(clientSocket, ackMessage2, sizeof(ackMessage2), 0);
        if (strcmp(ackMessage2, "LCK") == 0) {
            printf("Recibido que el servidor ya verificó la lista de eliminados. \n\n");
        } else {
            //fprintf(stderr, "Error: Falta de confirmación del servidor o mensaje de error.\n");
        }

        printf("----------------------------Lista Eliminados Servidor ------------------------------\n\n");
        receiveDeleteFiles(clientSocket, localDir, listaArchivosPath, listaArchivosLocalPath);
        recv(clientSocket, ackMessage2, sizeof(ackMessage2), 0);
        if (strcmp(ackMessage2, "OOK") == 0) {
            printf("Recibido que el servidor ya verificó la lista de eliminados. \n\n");
        } else {
            //fprintf(stderr, "Error: Falta de confirmación del servidor o mensaje de error.\n");
        }

        /*copyFile(listaArchivosPath, listaArchivosTempPath);
        copyFile(listaArchivosLocalPath, listaArchivosLocalTempPath);*/

        sendFilesListData(clientSocket, localDir,"all", listaArchivosPath);
        send(clientSocket, "SCK", strlen("SCK"), 0);

        printf("\n");

        //receiveFilesListData(clientSocket, localDir, listaArchivosTempPath, listaArchivosLocalTempPath);
        receiveFilesListData(clientSocket, localDir, listaArchivosPath, listaArchivosLocalPath, listRenameFiles, &currentIndex, "Client");
        updateFileList(localDir, listaArchivosLocalPath, listaArchivosPath, listaArchivosEliminados);
        char ackMessage[4];
        recv(clientSocket, ackMessage, sizeof(ackMessage), 0);
        if (strcmp(ackMessage, "MCK") == 0) {
            printf("Operación confirmada por el servidor Me toca enviar enviar lista de eliminados. \n\n");
        } else {
            //fprintf(stderr, "Error: Falta de confirmación del servidor o mensaje de error.\n");
        }

        printf("-------------------------------------------------------------------------\n\n");
        //printFileList(listaArchivosLocalTempPath);
        printf("-------------------------------------------------------------------------\n\n");
        //printFileList(listaArchivosTempPath);
        printf("index %d\n", currentIndex);
        for (int i = 0; i < currentIndex; ++i) {
            printf("Elemento %d:\n", i);
            printf("Old Path: %s\n", listRenameFiles[i].oldPath);
            printf("New Path: %s\n", listRenameFiles[i].newPath);
            printf("\n");

            rename(listRenameFiles[i].oldPath, listRenameFiles[i].newPath);
            renameFileInHistory(listaArchivosPath, listRenameFiles[i].oldPath, strrchr(listRenameFiles[i].oldPath, '/')+1, 
            strrchr(listRenameFiles[i].newPath, '/')+1);
            renameFileInHistory(listaArchivosLocalPath, listRenameFiles[i].oldPath, strrchr(listRenameFiles[i].oldPath, '/')+1, 
            strrchr(listRenameFiles[i].newPath, '/')+1);
        }

        updateFileList(localDir, listaArchivosLocalPath, listaArchivosPath, listaArchivosEliminados);
        printf("--Lista Archivos-----------------------------------------------------------------------\n\n");
        printFileList(listaArchivosPath);
        printf("--Lista LOCAL-----------------------------------------------------------------------\n\n");
        printFileList(listaArchivosLocalPath);
        /*printf("-------------------------------------------------------------------------\n\n");
        printf("----------------------------Lista Eliminados------------------------------\n\n");
        printf("Lista eliminados: %s\n", listaArchivosEliminados);*/

        cleanFileList(listaArchivosLocalPath, localDir);
        cleanFileList(listaArchivosPath, localDir);
        
        close(clientSocket);
    }

    return 0;
}
