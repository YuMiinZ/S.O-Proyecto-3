#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>


// Estructura para almacenar información de los archivos
struct FileInfo {
    char filename[256];
    long size;
    time_t lastModified;
    char action[10];
};

int fileExists(const char *filename) {
    return access(filename, F_OK) != -1;
}

void printHistory(const char *historyFileName) {
    FILE *history_file = fopen(historyFileName, "r");
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

// Función para recorrer el directorio y crear el historial
void createHistoryFile(const char* dirName, const char* historyFileName) {
    DIR *dir;
    struct dirent *dp;
    struct stat file_info;

    dir = opendir(dirName);

    FILE *history_file = fopen(historyFileName, "wb");
    if (history_file == NULL) {
        perror("Error al abrir o crear el archivo de historial");
        exit(1);
    }

    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || strcmp(dp->d_name, "historial.txt") == 0) {
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

void updateHistoryFile(const char* dirName, const char* historyFileName) {
    DIR *dir;
    struct dirent *dp;
    struct stat file_info;

    dir = opendir(dirName);

    FILE *history_file = fopen(historyFileName, "r+");
    if (history_file == NULL) {
        perror("Error al abrir el archivo de historial");
        exit(1);
    }

    struct FileInfo file_info_entry;

    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || strcmp(dp->d_name, "historial.txt") == 0) {
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

// Función para limpiar el historial eliminando las entradas "Eliminado"
void cleanHistoryFile(const char* historyFileName) {
    FILE *history_file = fopen(historyFileName, "r+");
    if (history_file == NULL) {
        perror("Error al abrir el archivo de historial");
        exit(1);
    }

    FILE *temp_file = fopen("temp_history.txt", "w");
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
    rename("temp_history.txt", historyFileName);
}

struct FileInfo getFileInfo(const char *historyFileName, const char *fileName) {
    FILE *history_file = fopen(historyFileName, "rb");
    if (history_file == NULL) {
        perror("Error al abrir el archivo de historial");
        exit(1);
    }

    struct FileInfo file_info_entry;
    struct FileInfo notFound; // Estructura para representar un archivo no encontrado
    strcpy(notFound.action, "No encontrado"); // Establece el campo 'action'

    while (fread(&file_info_entry, sizeof(struct FileInfo), 1, history_file) == 1) {
        if (strcmp(file_info_entry.filename, fileName) == 0) {
            fclose(history_file);
            return file_info_entry;
        }
    }

    fclose(history_file);
    return notFound; // Retorna la estructura para indicar que el archivo no se encontró
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Formato: %s <directorio>\n", argv[0]);
        return 1;
    }
    const char *dirName = argv[1];
    const char *historialFilename = "listaArchivos.bin";

    char historialPath[500]; 

    //snprintf(historialPath, sizeof(historialPath), "%s/%s", dirName, historialFilename);

    /*if (fileExists(historialPath)) {
        //Si existe solo lo actualiza
        printf("El archivo %s existe en el directorio %s.\n\n", historialFilename, dirName);
        printf("Actualizando el archivo de historial...\n");
        updateHistoryFile(dirName, historialPath);
        printf("Historial actualizado en: %s\n", historialPath);
    } else {
        //Si no existe el historial.txt se crea uno nuevo
        createHistoryFile(dirName, historialPath);
        printf("\nEl archivo %s no existe en el directorio %s. \n\nSe creará el archivo historial.\n", historialFilename, dirName);
        printf("Historial creado en: %s\n\n", historialPath);
    }

    
    
    printHistory(historialPath);
    printf("------------------------------------------------------\n");
    printf("Limpiando el archivo de historial...\n");
    cleanHistoryFile(historialPath);
    printf("Historial limpiado.\n");*/

    //printHistory(historialPath);

    /*struct FileInfo file_info = getFileInfo(historialPath, "server2.txt");

    if(strcmp(file_info.action, "No encontrado") == 0){
        printf("No se encontró\n");
    } else {
        printf("Nombre: %s\n", file_info.filename);
        printf("Tamaño: %ld bytes\n", file_info.size);
        printf("Fecha de Modificación: %s", ctime(&file_info.lastModified));
        printf("Acción: %s\n", file_info.action);
        printf("\n");
    }*/

    struct stat file_Info1;
    struct stat file_Info2;

    stat("DirectorioServer/server2.txt", &file_Info1);
    stat("DirectorioCliente/server2.txt", &file_Info2);

    printf("Fecha Server: %sFecha Server: %s", ctime(&file_Info1.st_ctime), ctime(&file_Info1.st_mtime));
    printf("Fecha Cliente: %sFecha Cliente: %s", ctime(&file_Info2.st_ctime), ctime(&file_Info2.st_mtime));
    printf("tamaño %ld", file_Info1.st_size);
    return 0;
}
