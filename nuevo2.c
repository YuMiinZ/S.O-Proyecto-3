#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

#include <dirent.h> // Agrega esta línea para incluir la biblioteca de directorios

// Estructura para la información de los archivos
struct FileInfo {
    char filename[256];
    char action[10]; // Creado, Modificado, Eliminado
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

    char buffer[1024];
    int bytesRead;
    long fileSize;

    // Recibe el tamaño del archivo
    recv(socket, &fileSize, sizeof(long), 0);

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

        printf("Esperando conexiones entrantes...\n");

        clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addrSize);
        if (clientSocket < 0) {
            perror("Error al aceptar la conexión entrante");
            return 1;
        }

        // Recibe el nombre de los archivos del cliente
        char filenames[1024];
        int bytesRead = recv(clientSocket, filenames, sizeof(filenames), 0);

        if (bytesRead < 0) {
            perror("Error al recibir los nombres de los archivos");
        } else {
            // Tokeniza los nombres de los archivos
            char *token = strtok(filenames, ",");
            while (token != NULL) {
                char filePath[500];
                snprintf(filePath, sizeof(filePath), "%s/%s", localDir, token);

                // Verifica si el archivo es listaArchivos.bin y, en ese caso, guárdalo como listaArchivosTemp.bin
                if (strcmp(token, "listaArchivos.bin") == 0) {
                    char tempFilePath[500];
                    snprintf(tempFilePath, sizeof(tempFilePath), "%s/listaArchivosTemp.bin", localDir);
                    receiveFile(clientSocket, tempFilePath);
                    printf("Archivo recibido: %s (guardado como listaArchivosTemp.bin)\n", token);
                } else {
                    receiveFile(clientSocket, filePath);
                    printf("Archivo recibido: %s\n", token);
                }

                token = strtok(NULL, ",");
            }
        }

        // Aquí puedes agregar el código para enviar archivos del servidor al cliente
        // Lista de archivos en el directorio local
        char fileList[1024];
        memset(fileList, 0, sizeof(fileList));
        struct dirent *entry;
        DIR *dir = opendir(localDir);
        if (dir == NULL) {
            perror("Error al abrir el directorio");
            return 1;
        }
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                strncat(fileList, entry->d_name, sizeof(fileList) - 1);
                strncat(fileList, ",", sizeof(fileList) - 1);
            }
        }
        closedir(dir);

        // Envía la lista de archivos al servidor
        send(clientSocket, fileList, strlen(fileList), 0);

        // Tokeniza la lista de archivos
        char *token = strtok(fileList, ",");
        while (token != NULL) {
            char filePath[500];
            snprintf(filePath, sizeof(filePath), "%s/%s", localDir, token);

            FILE *file = fopen(filePath, "rb");
            if (file == NULL) {
                perror("Error al abrir el archivo");
                return 1;
            }

            // Obtiene el tamaño del archivo
            fseek(file, 0, SEEK_END);
            long fileSize = ftell(file);
            fseek(file, 0, SEEK_SET);

            // Envía el tamaño del archivo al servidor
            send(clientSocket, &fileSize, sizeof(long), 0);

            char buffer[1024];
            int bytesRead;
            while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
                send(clientSocket, buffer, bytesRead, 0);
            }

            fclose(file);

            printf("Archivo enviado: %s\n", token);

            token = strtok(NULL, ",");
        }
        //sendServerFiles(clientSocket, localDir);

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

        // Lista de archivos en el directorio local
        char fileList[1024];
        memset(fileList, 0, sizeof(fileList));
        struct dirent *entry;
        DIR *dir = opendir(localDir);
        if (dir == NULL) {
            perror("Error al abrir el directorio");
            return 1;
        }
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                strncat(fileList, entry->d_name, sizeof(fileList) - 1);
                strncat(fileList, ",", sizeof(fileList) - 1);
            }
        }
        closedir(dir);

        // Envía la lista de archivos al servidor
        send(clientSocket, fileList, strlen(fileList), 0);

        // Tokeniza la lista de archivos
        char *token = strtok(fileList, ",");
        while (token != NULL) {
            char filePath[500];
            snprintf(filePath, sizeof(filePath), "%s/%s", localDir, token);

            FILE *file = fopen(filePath, "rb");
            if (file == NULL) {
                perror("Error al abrir el archivo");
                return 1;
            }

            // Obtiene el tamaño del archivo
            fseek(file, 0, SEEK_END);
            long fileSize = ftell(file);
            fseek(file, 0, SEEK_SET);

            // Envía el tamaño del archivo al servidor
            send(clientSocket, &fileSize, sizeof(long), 0);

            char buffer[1024];
            int bytesRead;
            while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
                send(clientSocket, buffer, bytesRead, 0);
            }

            fclose(file);

            printf("Archivo enviado: %s\n", token);

            token = strtok(NULL, ",");
        }

        // Recibe el nombre de los archivos del cliente
        char filenames[1024];
        int bytesRead = recv(clientSocket, filenames, sizeof(filenames), 0);

        if (bytesRead < 0) {
            perror("Error al recibir los nombres de los archivos");
        } else {
            // Tokeniza los nombres de los archivos
            char *token = strtok(filenames, ",");
            while (token != NULL) {
                char filePath[500];
                snprintf(filePath, sizeof(filePath), "%s/%s", localDir, token);

                // Verifica si el archivo es listaArchivos.bin y, en ese caso, guárdalo como listaArchivosTemp.bin
                if (strcmp(token, "listaArchivos.bin") == 0) {
                    char tempFilePath[500];
                    snprintf(tempFilePath, sizeof(tempFilePath), "%s/listaArchivosTemp.bin", localDir);
                    receiveFile(clientSocket, tempFilePath);
                    printf("Archivo recibido: %s (guardado como listaArchivosTemp.bin)\n", token);
                } else {
                    receiveFile(clientSocket, filePath);
                    printf("Archivo recibido: %s\n", token);
                }

                token = strtok(NULL, ",");
            }
        }

        close(clientSocket);
    }

    return 0;
}
