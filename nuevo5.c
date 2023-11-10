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
    char modificationTime[100];

    //Recibe la fecha de modificación
    recv(socket, modificationTime, sizeof(modificationTime), 0);
    printf("Fecha: %s\n", modificationTime);

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

// Función para enviar la información de archivos al cliente
void sendFileData(int clientSocket, const char *localDir) {
    printf("HOLA?\n");
    struct dirent *entry;
    DIR *dir = opendir(localDir);

    if (dir == NULL) {
        perror("Error al abrir el directorio");
        return;
    }

    char archivo_lista[500][1024];
    int num_archivos = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 &&
            strcmp(entry->d_name, "listaArchivosTemp.bin") != 0 &&
            strcmp(entry->d_name, "listaArchivos.bin") != 0) {
            snprintf(archivo_lista[num_archivos], sizeof(archivo_lista[num_archivos]), "%s/%s", localDir, entry->d_name);
            num_archivos++;
        }
    }
    closedir(dir);
    printf("HOLA?\n");
    
    // Envía el número de archivos al cliente
    printf("cantidad de archivos %d\n", num_archivos);
    send(clientSocket, &num_archivos, sizeof(int), 0);
    
    for (int i = 0; i < 1; i++) {
        printf("HOLA? for \n");
        char fileName[1024];
        snprintf(fileName, sizeof(fileName), "%s", archivo_lista[i]);

        FILE *file = fopen(fileName, "rb");
        printf("HOLA? for \n");
        if (file == NULL) {
            perror("Error al abrir el archivo");
            continue; // Si no se pudo abrir el archivo, continuamos con el siguiente
        }

        // Obtiene la fecha de modificación del archivo
        struct stat file_Info;
        stat(fileName, &file_Info);
        char modificationTime[1024];
        strftime(modificationTime, sizeof(modificationTime), "%c", localtime(&file_Info.st_mtime));
        
        // Obtiene el tamaño del archivo
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);
        printf("HOLA? for \n");
        // Envía el nombre del archivo al cliente
        send(clientSocket, fileName, sizeof(fileName), 0);
        printf("HOLA? for \n");
        // Envía la fecha de modificación al cliente
        send(clientSocket, modificationTime, sizeof(modificationTime), 0);
        // Envía el tamaño del archivo al cliente
        send(clientSocket, &fileSize, sizeof(long), 0);
        
        /*char buffer[1024];
        int bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            send(clientSocket, buffer, bytesRead, 0);
        }*/

        fclose(file);

        printf("Información de archivo enviada: %s\n", archivo_lista[i]);
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
                    printf("Archivo recibido: %s\n", token);
                } else {
                    printf("Archivo recibido: %s (ignorado)\n", token);
                    long fileSize;
                    recv(clientSocket, &fileSize, sizeof(long), 0);
                    while (fileSize > 0) {
                        char buffer[1024];
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

        // Envía información de archivos al cliente
        /*char result_message[2000];
        snprintf(result_message, sizeof(result_message), "%s", "LISTA DE ARCHIVOS FINALIZADA");
        // Enviar el resultado de vuelta al cliente
        if (send(clientSocket, result_message, strlen(result_message), 0) < 0) {
            perror("Envío fallido");
        }
        sendFileData(clientSocket, localDir);*/

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

        // Obtén la lista de archivos del directorio local
        char fileList[1024];
        memset(fileList, 0, sizeof(fileList));
        struct dirent *entry;
        DIR *dir = opendir(localDir);
        if (dir == NULL) {
            perror("Error al abrir el directorio");
            return 1;
        }
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && 
                strcmp(entry->d_name, "listaArchivosTemp.bin") != 0 && strcmp(entry->d_name, "listaArchivos.bin") != 0 ) {
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

            // Envía los archivos al servidor
            FILE *file = fopen(filePath, "rb");
            if (file == NULL) {
                perror("Error al abrir el archivo");
                return 1;
            }

            // Obtiene la fecha de modificación del archivo
            struct stat file_Info;
            stat(filePath, &file_Info);
            char modificationTime[1024];
            strftime(modificationTime, sizeof(modificationTime), "%c", localtime(&file_Info.st_mtime));
            // Envía la fecha de modificación al servidor
            send(clientSocket, modificationTime, strlen(modificationTime), 0);

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


        // Obtén la lista de archivos del servidor y muestra la información
        /*char server_reply[2000];
        int num_server_files;
        // Receive the response
        if (recv(clientSocket, server_reply, sizeof(server_reply), 0) < 0) {
            puts("Receive failed");
            return -1.0;
        }
        printf("Mensaje del servidor %s\n",server_reply);
        //int num_server_files;
        recv(clientSocket, &num_server_files, sizeof(int), 0);
        printf("recibi la cant archivos %d\n", num_server_files);
        for (int i = 0; i < 1; i++) {
            char server_fileName[1024];
            char server_modificationTime[1024];
            long server_fileSize;

            recv(clientSocket, server_fileName, sizeof(server_fileName), 0);
            recv(clientSocket, server_modificationTime, sizeof(server_modificationTime), 0);
            recv(clientSocket, &server_fileSize, sizeof(long), 0);

            printf("Nombre: %s\n", server_fileName);
            printf("Fecha: %s\n", server_modificationTime);
            printf("Tamaño: %ld bytes\n", server_fileSize);

            /*char server_filePath[1024];
            snprintf(server_filePath, sizeof(server_filePath), "%s/%s", localDir, server_fileName);
            receiveFile(clientSocket, server_filePath);
            printf("Archivo recibido: %s\n", server_fileName);*/
        //}

        close(clientSocket);
    }

    return 0;
}
