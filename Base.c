#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 8889
#define MAX_BUFFER_SIZE 1024

struct FileChange {
    char filename[256];   
    char action[10];      //Crear, Borrar, Modificar
    long size;            
    time_t lastModified;  
};

struct DeletedFile {
    char filename[256];
    time_t deletionTime;
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Formato: %s <directorio> o <directorio> <IP>\n", argv[0]);
        return 1;
    }

    char *localDir = argv[1];
    char *remoteIP = argv[2];

    // Comprueba si este programa debe actuar como servidor
    if (argc == 3) {
        // Modo cliente: El tercer argumento contiene la dirección IP del servidor
       // syncDirectories(localDir, "", remoteIP);
        printf("Conexion del cliente\n");
    } else {
        // Modo servidor: Espera conexiones entrantes
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

        printf("Esperando conexiones en el puerto %d...\n", PORT);

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