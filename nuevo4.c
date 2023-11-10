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

struct Files {
    char filename[256];   
    char action[10];      //Creado, Modificado, Eliminado
    long size;            
    time_t lastModified;  
};


// Función para recibir un archivo y guardarlo en el servidor
void receiveFile(int socket, const char *filePath) {
    FILE *file = fopen(filePath, "wb");
    if (file == NULL) {
        perror("Error al crear el archivo");
        return;
    }

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

void getFileList(const char *localDir, char *fileList, size_t listSize, char *option) {
    memset(fileList, 0, listSize);
    struct dirent *entry;
    DIR *dir = opendir(localDir);
    if (dir == NULL) {
        perror("Error al abrir el directorio");
        return;
    }
    if(strcmp(option,"all")==0){
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 &&
                strcmp(entry->d_name, "listaArchivosTemp.bin") != 0 && strcmp(entry->d_name, "listaArchivos.bin") != 0) {
                strncat(fileList, entry->d_name, listSize - 1);
                strncat(fileList, ",", listSize - 1);
            }
        }
    }
    closedir(dir);
}

void sendFilesListData(int clientSocket, const char *localDir, char *option){
    // Obtén la lista de archivos del directorio local
    char fileList[1024];

    getFileList(localDir,fileList,sizeof(fileList), option);
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

void receiveFilesListData(int clientSocket, const char *localDir){
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
                receiveFile(clientSocket, filePath);
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

        // Recibe el nombre de los archivos del cliente
        receiveFilesListData(clientSocket, localDir);
        printf("\n");
        sendFilesListData(clientSocket, localDir, "all");

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


        sendFilesListData(clientSocket, localDir,"all");
        printf("\n");
        receiveFilesListData(clientSocket, localDir);

        close(clientSocket);
    }

    return 0;
}
