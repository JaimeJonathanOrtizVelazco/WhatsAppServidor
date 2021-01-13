#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ftw.h>
#include <fcntl.h>

#define PUERTO 5000
#define COLA_CLIENTES 50
#define TAM_BUFFER 80

pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

void *client(void *accept_sd);

void *RecevMSG(void *args);

void *SendMSG(void *args);

struct args {
    int accept_sd;
    char *username;
    char *contact;
};

int LoginRegisterMenu(void *accept_sd, void *username, void *File);

int Login(void *accept_sd, void *username, void *File);

void Register(void *cliente_sockfd, void *File);

void LoggedUserMenu(void *accept_sd, void *username, void *File);

void ContactMenu(void *accept_sd, void *username, void *File);

int GetContactList(void *accept_sd, void *username, void *File);

void AddContact(void *accept_sd, void *username, void *File);

void Chat(void *accept_sd, void *username, void *File);

void SendToSocket(void *accept_sd, void *bufferMessage);

void RecievFSocket(void *accept_sd, void *variable);

int main() {
    int sockfd, cliente_sockfd;
    pthread_t id_hilo;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PUERTO);
    addr.sin_addr.s_addr = INADDR_ANY;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        exit(-1);
    }
    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(sockfd);
        exit(-1);
    }
    if (listen(sockfd, COLA_CLIENTES) < 0) {
        close(sockfd);
        exit(-1);
    }
    while (1) {
        if ((cliente_sockfd = accept(sockfd, NULL, NULL)) < 0) {
            printf("Algo malo ocurrio 1");
            exit(1);
        }
        if (pthread_create(&id_hilo, NULL, client, (void *) &cliente_sockfd) != 0) {
            printf("Algo malo ocurrio 2");
            close(cliente_sockfd);
            close(sockfd);
            exit(1);
        }
    }
    close(sockfd);
}

void *client(void *accept_sd) {
    char username[TAM_BUFFER];
    FILE *File = NULL;
    int fd = *((int *) accept_sd);
    while (1) {
        if (LoginRegisterMenu(&fd, &username, File) == 0)
            break;
        LoggedUserMenu(&fd, &username, File);
    }
    close(fd);
    pthread_exit(NULL);
}

int LoginRegisterMenu(void *accept_sd, void *username, void *File) {
    char buffermessage[TAM_BUFFER];
    while (1) {
        int exit = 0;
        SendToSocket(&*(int *) accept_sd, "BIENVENIDO A TU CHAT");
        SendToSocket(&*(int *) accept_sd, "1)Iniciar Sesion");
        SendToSocket(&*(int *) accept_sd, "2)Registrarse");
        SendToSocket(&*(int *) accept_sd, "3)Terminar la aplicacion");
        while (exit == 0) {
            RecievFSocket(&*(int *) accept_sd, &buffermessage);
            int caseOption = atoi(buffermessage);
            switch (caseOption) {
                case 1:
                    if (Login(&*(int *) accept_sd, username, File) == 1)
                        return 1;
                    else
                        exit = 1;
                    break;
                case 2:
                    Register(&*(int *) accept_sd, File);
                    exit = 1;
                    break;
                case 3:
                    return 0;
                default:
                    SendToSocket(&*(int *) accept_sd, "Selecciona un opcion correcta");
                    break;
            }
        }
    }
}

int Login(void *accept_sd, void *username, void *File) {
    char registry[TAM_BUFFER];
    char password[TAM_BUFFER];
    char response[1];
    while (1) {
        SendToSocket(&*(int *) accept_sd, "Nombre de usuario: ");
        RecievFSocket(&*(int *) accept_sd, username);
        SendToSocket(&*(int *) accept_sd, "Password: ");
        RecievFSocket(&*(int *) accept_sd, &password);
        File = fopen("Users.txt", "r");
        if (File == NULL) {
            fclose(File);
            fopen("Users.txt", "a");
            fclose(File);
            File = fopen("Users.txt", "r");
        }
        while (fgets(registry, sizeof(registry), File)) {
            char *results = strtok(registry, " ");
            if (strcmp(results, username) != 0)
                continue;
            results = strtok(NULL, " ");
            if (strcmp(results, password) != 0) {
                fclose(File);
                SendToSocket(&*((int *) accept_sd), "Usuario o contrasena erroneos");
                break;
            }
            fclose(File);
            SendToSocket(&*((int *) accept_sd), "Se ha loggeado exitosamente");
            return 1;
        }
        SendToSocket(&*((int *) accept_sd), "Desea volver a intentarlo? s/n");
        RecievFSocket(&*(int *) accept_sd, &response);
        if (strcmp(response, "s") == 0)
            continue;
        return 0;
    }
}

void Register(void *cliente_sockfd, void *File) {
    char usernameFile[30];
    char username[TAM_BUFFER];
    char password[TAM_BUFFER];
    char registry[TAM_BUFFER];
    char response[1];
    int flag = 0;
    while (1) {
        SendToSocket(&*(int *) cliente_sockfd, "Nombre de usuario: ");
        RecievFSocket(&*(int *) cliente_sockfd, &username);
        SendToSocket(&*(int *) cliente_sockfd, "Password: ");
        RecievFSocket(&*(int *) cliente_sockfd, &password);
        File = fopen("Users.txt", "r");
        if (File == NULL) {
            fclose(File);
            fopen("Users.txt", "a");
            fclose(File);
            File = fopen("Users.txt", "r");
        }
        while (fgets(registry, sizeof(registry), File)) {
            char *results = strtok(registry, " ");
            if (strcmp(results, username) != 0)
                continue;
            flag = 1;
        }
        fclose(File);
        if (flag == 1) {
            SendToSocket(&*(int *) cliente_sockfd, "El nombre de usuario ya ha sido registrado");
            SendToSocket(&*(int *) cliente_sockfd, "Desea intentar con otro? s/n");
            RecievFSocket(&*(int *) cliente_sockfd, &response);
            if (strcmp(response, "s") == 0)
                continue;
            break;
        }
        strcpy(usernameFile, username);
        strcat(usernameFile, ".txt");
        pthread_rwlock_wrlock(&rwlock);
        File = fopen("Users.txt", "a");
        fprintf(File, "%s %s %s\n", username, password, usernameFile);
        fclose(File);
        pthread_rwlock_unlock(&rwlock);
        File = fopen(usernameFile, "w");
        fclose(File);
        break;
    }
}

void LoggedUserMenu(void *accept_sd, void *username, void *File) {
    char buffermessage[TAM_BUFFER];
    char welcome[TAM_BUFFER] = "Bienvenido ";
    strcat(welcome, (char *) username);
    int out = 0, exit = 0;
    while (out == 0) {
        SendToSocket(&*(int *) accept_sd, welcome);
        SendToSocket(&*(int *) accept_sd, "1)Mis contactos");
        SendToSocket(&*(int *) accept_sd, "2)Solicitudes de amistad pendientes");
        SendToSocket(&*(int *) accept_sd, "3)Enviar archivo");
        SendToSocket(&*(int *) accept_sd, "4)Recibir archivos");
        SendToSocket(&*(int *) accept_sd, "5)Cerrar sesion");
        while (exit == 0) {
            RecievFSocket(&*(int *) accept_sd, &buffermessage);
            int caseOption = atoi(buffermessage);
            switch (caseOption) {
                case 1:
                    ContactMenu(&*(int *) accept_sd, username, File);
                    exit = 1;
                    break;
                case 2:
//                    Register(&*(int *) cliente_sockfd, File);
                    /* exit = 1;
                     break;*/
                case 3:
                case 4:
                case 5:
                    out = 1;
                    break;
                default:
                    SendToSocket(&*(int *) accept_sd, "Selecciona un opcion correcta");
                    break;
            }
        }
    }
}

void ContactMenu(void *accept_sd, void *username, void *File) {
    int Total = 0;
    char buffermessage[TAM_BUFFER];
    int out = 0, exit = 0;
    while (out == 0) {
        Total = GetContactList(&*(int *) accept_sd, username, File);
        sprintf(buffermessage, "\tTotal de contactos: %d", Total);
        SendToSocket(&*(int *) accept_sd, buffermessage);
        SendToSocket(&*(int *) accept_sd, "1)Iniciar conversacion\t2)Agregar contacto\t3)Eliminar Contacto\t4)Salir");
        while (exit == 0) {
            RecievFSocket(&*(int *) accept_sd, &buffermessage);
            int caseOption = atoi(buffermessage);
            switch (caseOption) {
                case 1:
                    Chat(&*(int *) accept_sd, username, File);
                    exit = 1;
                    break;
                case 2:
                    AddContact(&*(int *) accept_sd, username, File);
                    exit = 1;
                    break;
                case 3:
                    /*eliminar();
                     * out = 1;
                    break;*/
                case 4:
                    out = 1;
                    break;
                default:
                    SendToSocket(&*(int *) accept_sd, "Selecciona un opcion correcta");
                    break;
            }
        }
    }
}

void Chat(void *accept_sd, void *username, void *File) {
    char buffermessage[TAM_BUFFER];
    char FileRoute[TAM_BUFFER];
    char registry[TAM_BUFFER];
    int found = 0, end = 0;
    SendToSocket(&*(int *) accept_sd, "Nombre del usuario a chatear:");
    RecievFSocket(&*(int *) accept_sd, &buffermessage);
    strncpy(FileRoute, username, strlen(username) + 1);
    strcat(FileRoute, ".txt");
    printf("%s\n", FileRoute);
    File = fopen(FileRoute, "r");
    while (fgets(registry, sizeof(registry), File)) {
        char *results = strtok(registry, " ");
        if (strcmp(results, buffermessage) == 0)
            found = 1;
    }
    if (found == 0) {
        char *message = "Debes ser amigo de ";
        strncat(message, buffermessage, strlen(buffermessage) + 1);
        strncat(message, " para poder chatear", 20);
        SendToSocket(&*(int *) accept_sd, message);
    } else {
        pthread_t RecevMSGT, SendMSGT;
        struct args *ChatStruct = (struct args *) malloc(sizeof(struct args));
        ChatStruct->accept_sd = *(int *) accept_sd;
        ChatStruct->username = username;
        ChatStruct->contact = buffermessage;
        if (pthread_create(&RecevMSGT, NULL, RecevMSG, (void *) ChatStruct) != 0) {
            printf("Algo malo ocurrio 2");
            close(*(int *) accept_sd);
        }
        if (pthread_create(&SendMSGT, NULL, SendMSG, (void *) ChatStruct) != 0) {
            printf("Algo malo ocurrio 2");
            close(*(int *) accept_sd);
        }
        while (!end);
    }
}

void *RecevMSG(void *args) {
    int fd;
    char FifoName[TAM_BUFFER];
    char buffer[TAM_BUFFER];
    strncpy(FifoName, ((struct args *) args)->contact, strlen(((struct args *) args)->contact) + 1);
    strncat(FifoName, ((struct args *) args)->username, strlen(((struct args *) args)->username) + 1);
    fd = open(FifoName, O_RDONLY);
    while (1) {
        read(fd, buffer, TAM_BUFFER);
        SendToSocket(&((struct args *) args)->accept_sd, buffer);
    }
    close(fd);
    pthread_exit(NULL);
}

void *SendMSG(void *args) {
    int fd;
    char FifoName[TAM_BUFFER];
    char buffer[TAM_BUFFER];
    strncpy(FifoName, ((struct args *) args)->username, strlen(((struct args *) args)->username) + 1);
    strncat(FifoName, ((struct args *) args)->contact, strlen(((struct args *) args)->contact) + 1);
    fd = open(FifoName, O_WRONLY);
    while (1) {
        RecievFSocket(&((struct args *) args)->accept_sd, &buffer);
        write(fd, buffer, strlen(buffer));
        SendToSocket(&((struct args *) args)->accept_sd, buffer);
    }
    close(fd);
    pthread_exit(NULL);
}

void AddContact(void *accept_sd, void *username, void *File) {
    char buffermessage[TAM_BUFFER];
    char FileRoute[TAM_BUFFER];
    char Fifo[TAM_BUFFER];
    SendToSocket(&*(int *) accept_sd, "Nombre del usuario a agregar:");
    RecievFSocket(&*(int *) accept_sd, &buffermessage);
    strncpy(FileRoute, username, strlen(username) + 1);
    strcat(FileRoute, ".txt");
    strncpy(Fifo, username, strlen(username) + 1);
    strcat(Fifo, buffermessage);
    File = fopen(FileRoute, "a");
    fprintf(File, "%s %s\n", buffermessage, Fifo);
    fclose(File);
    mkfifo(Fifo, 0666);
    //Esto se va a utlizar para las solicitudes pendientes
    bzero(FileRoute, TAM_BUFFER);
    bzero(Fifo, TAM_BUFFER);
    strncpy(FileRoute, buffermessage, strlen(buffermessage) + 1);
    strcat(FileRoute, ".txt");
    strcpy(Fifo, buffermessage);
    strncat(Fifo, username, strlen(username) + 1);
    File = fopen(FileRoute, "a");
    fprintf(File, "%s %s\n", (char *) username, Fifo);
    fclose(File);
    mkfifo(Fifo, 0666);
}

int GetContactList(void *accept_sd, void *username, void *File) {
    int Total_Contacts = 0;
    char registry[TAM_BUFFER];
    char FileRoute[TAM_BUFFER];
    strncpy(FileRoute, username, strlen(username) + 1);
    strcat(FileRoute, ".txt");
    File = fopen(FileRoute, "r");
    while (fgets(registry, sizeof(registry), File)) {
        char *results = strtok(registry, " ");
        SendToSocket(&*(int *) accept_sd, results);
        Total_Contacts++;
    }
    fclose(File);
    return Total_Contacts;
}

void RecievFSocket(void *accept_sd, void *variable) {
    bzero(variable, sizeof(variable));
    if ((recv(*(int *) accept_sd, variable, TAM_BUFFER, 0)) < 0) {
        close(*(int *) accept_sd);
        pthread_exit(NULL);
    }
}

void SendToSocket(void *accept_sd, void *bufferMessage) {
    char ok[TAM_BUFFER];
    int lgData;
    while (1) {
        if ((send(*(int *) accept_sd, "SEND", strlen((char *) bufferMessage), 0)) < 0) {
            close(*(int *) accept_sd);
            pthread_exit(NULL);
        }
        if ((lgData = recv(*(int *) accept_sd, &ok, TAM_BUFFER, 0)) < 0) {
            close(*(int *) accept_sd);
            pthread_exit(NULL);
        }
        if (lgData == strlen((char *) bufferMessage) && strcmp(ok, "OK") == 0) {
            if ((send(*(int *) accept_sd, "OK", TAM_BUFFER, 0)) < 0) {
                close(*(int *) accept_sd);
                pthread_exit(NULL);
            }
        } else {
            if ((send(*(int *) accept_sd, "NO", TAM_BUFFER, 0)) < 0) {
                close(*(int *) accept_sd);
                pthread_exit(NULL);
            }
            sleep(1);
            continue;
        }
        if ((recv(*(int *) accept_sd, &ok, TAM_BUFFER, 0)) < 0) {
            close(*(int *) accept_sd);
            pthread_exit(NULL);
        }
        if ((send(*(int *) accept_sd, (char *) bufferMessage, strlen((char *) bufferMessage), 0)) < 0) {
            close(*(int *) accept_sd);
            pthread_exit(NULL);
        }
        if ((recv(*(int *) accept_sd, &ok, TAM_BUFFER, 0)) < 0) {
            close(*(int *) accept_sd);
            pthread_exit(NULL);
        }
        if (strcmp(ok, "OK") != 0) {
            sleep(1);
            continue;
        }
        break;
    }
}


