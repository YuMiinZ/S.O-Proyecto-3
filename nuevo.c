#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

struct FileInfo {
    char filename[256];
    char action[10]; // Creado, Modificado, Eliminado
    long size;
    time_t lastModified;
};

void sendFile(int socket, const char *filename, const struct FileInfo *fileInfo, const char *localDir) {
    char filePath[500]; // Aumenta el tamaño de la cadena
    snprintf(filePath, sizeof(filePath), "%s/%s", localDir, filename); // Usa localDir

    FILE *file = fopen(filePath, "rb");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        return;
    }

    // Envía la estructura FileInfo
    send(socket, fileInfo, sizeof(struct FileInfo), 0);

    char buffer[1024];
    int bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(socket, buffer, bytesRead, 0);
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Formato: %s <directorio> o <directorio> <IP>\n", argv[0]);
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

        printf("Esperando conexiones entrantes...\n");

        clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addrSize);
        if (clientSocket < 0) {
            perror("Error al aceptar la conexión entrante");
            return 1;
        }

        // El servidor recibe archivos del cliente
        struct FileInfo fileInfo;
        ssize_t bytesRead = recv(clientSocket, &fileInfo, sizeof(struct FileInfo), 0);

        if (bytesRead < 0) {
            perror("Error al recibir FileInfo");
        } else if (bytesRead == sizeof(struct FileInfo)) {
            // Se recibió la estructura FileInfo, imprímela
            printf("Received FileInfo:\n");
            printf("Filename: %s\n", fileInfo.filename);
            printf("Action: %s\n", fileInfo.action);
            printf("Size: %ld bytes\n", fileInfo.size);
            printf("Last Modified: %ld\n", (long)fileInfo.lastModified);
        } else {
            printf("No se recibió FileInfo completa\n");
        }

        char filePath[500]; // Aumenta el tamaño de la cadena de ruta
        snprintf(filePath, sizeof(filePath), "%s/%s", localDir, fileInfo.filename); // Usa localDir

        FILE *file = fopen(filePath, "wb");
        if (file == NULL) {
            perror("Error al crear el archivo");
            return 1;
        }

        char buffer[1024];
        int bytesReceived;
        long bytesLeft = fileInfo.size;
        while (bytesLeft > 0) {
            bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesReceived <= 0) {
                break;
            }
            fwrite(buffer, 1, bytesReceived, file);
            bytesLeft -= bytesReceived;
        }

        fclose(file);

        printf("Archivo recibido: %s\n", filePath);

        // Después de recibir y guardar los archivos, envía "server1.txt" de vuelta al cliente
        struct FileInfo serverFileInfo;
        memset(&serverFileInfo, 0, sizeof(struct FileInfo));
        strncpy(serverFileInfo.filename, "server1.txt", sizeof(serverFileInfo.filename));

        sendFile(clientSocket, serverFileInfo.filename, &serverFileInfo, localDir);

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

        // El cliente envía un archivo al servidor
        struct FileInfo fileInfo;
        memset(&fileInfo, 0, sizeof(struct FileInfo));
        strncpy(fileInfo.filename, "prueba1.txt", sizeof(fileInfo.filename));

        char file1Path[500]; // Aumenta el tamaño de la cadena de ruta
        snprintf(file1Path, sizeof(file1Path), "%s/%s", localDir, fileInfo.filename); // Usa localDir

        FILE *file = fopen(file1Path, "rb");
        if (file == NULL) {
            perror("Error al abrir el archivo");
            return 1;
        }

        fseek(file, 0, SEEK_END);
        fileInfo.size = ftell(file);
        fseek(file, 0, SEEK_SET);

        fclose(file);

        sendFile(clientSocket, fileInfo.filename, &fileInfo, localDir);

        // El servidor recibe archivos del cliente
        struct FileInfo fileInfoReceive;
        ssize_t bytesRead = recv(clientSocket, &fileInfoReceive, sizeof(struct FileInfo), 0);

        if (bytesRead < 0) {
            perror("Error al recibir FileInfo");
        } else if (bytesRead == sizeof(struct FileInfo)) {
            // Se recibió la estructura FileInfo, imprímela
            printf("Received FileInfo:\n");
            printf("Filename: %s\n", fileInfoReceive.filename);
            printf("Action: %s\n", fileInfoReceive.action);
            printf("Size: %ld bytes\n", fileInfoReceive.size);
            printf("Last Modified: %ld\n", (long)fileInfoReceive.lastModified);
        } else {
            printf("No se recibió FileInfo completa\n");
        }

        char filePath[500]; // Aumenta el tamaño de la cadena de ruta
        snprintf(filePath, sizeof(filePath), "%s/%s", localDir, fileInfoReceive.filename); // Usa localDir

        FILE *fileClient = fopen(filePath, "wb");
        if (fileClient == NULL) {
            perror("Error al crear el archivo");
            return 1;
        }

        char buffer[1024];
        int bytesReceived;
        long bytesLeft = fileInfo.size;
        while (bytesLeft > 0) {
            bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesReceived <= 0) {
                break;
            }
            fwrite(buffer, 1, bytesReceived, fileClient);
            bytesLeft -= bytesReceived;
        }

        fclose(fileClient);

        printf("Archivo recibido: %s\n", filePath);

        close(clientSocket);
    }

    return 0;
}
