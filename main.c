#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define PUERTO 5000
#define COLA_CLIENTES 50
#define TAM_BUFFER 80

pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
char username[TAM_BUFFER];
char password[TAM_BUFFER];
pthread_mutex_t sendMutex = PTHREAD_MUTEX_INITIALIZER;

void *client_menu(void *accept_sd);

void SendToSocket(void *accept_sd, void *bufferMessage);

void RecievFSocket(void *accept_sd, void *variable);

int VerifyOrRegisterUser(void *cliente_sockfd, void *File, int option);

int main() {
    int listen_sd, accept_sd;
    pthread_t id_hilo;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PUERTO);
    addr.sin_addr.s_addr = INADDR_ANY;
    if ((listen_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        exit(-1);
    }
    if (bind(listen_sd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(listen_sd);
        exit(-1);
    }
    if (listen(listen_sd, COLA_CLIENTES) < 0) {
        close(listen_sd);
        exit(-1);
    }
    while (1) {
        if ((accept_sd = accept(listen_sd, NULL, NULL)) < 0) {
            close(listen_sd);
            exit(-1);
        }
        if (pthread_create(&id_hilo, NULL, client_menu, (void *) &accept_sd) != 0) {
            close(accept_sd);
            exit(-1);
        }
    }
}

void *client_menu(void *accept_sd) {
    char buffermessage[TAM_BUFFER];
    FILE *file;
    int flag = 0;
    while (flag == 0) {
        SendToSocket(&*(int *) accept_sd, "Que desea realizar?");
        SendToSocket(&*(int *) accept_sd, "1)Iniciar Sesion");
        SendToSocket(&*(int *) accept_sd, "2)Registrarse");
        SendToSocket(&*(int *) accept_sd, "3)Terminar la aplicacion");
        RecievFSocket(&*(int *) accept_sd, &buffermessage);
        int caseOption = atoi(buffermessage);
        switch (caseOption) {
            case 1:
            case 2:
                if (VerifyOrRegisterUser(&*((int *) accept_sd), &file, caseOption)) {
                    if (caseOption == 1)
                        SendToSocket(&*((int *) accept_sd), "Se ha logeado exitosamente");
                    else
                        SendToSocket(&*((int *) accept_sd), "Se ha registrado exitosamente");
                } else {
                    if (caseOption == 1)
                        SendToSocket(&*((int *) accept_sd), "Usuario o contrasena erroneos");
                    else
                        SendToSocket(&*((int *) accept_sd), "Usuario ya existe, intente registrar uno nuevo");
                }
                break;
            case 3:
                flag = 1;
                break;
            default:
                break;
        }
    }
    SendToSocket(&*((int *) accept_sd), "quit()");
    close(*((int *) accept_sd));
    pthread_exit(NULL);
}

void RecievFSocket(void *accept_sd, void *variable) {
    bzero(variable, TAM_BUFFER);
    if ((recv(*(int *) accept_sd, variable, TAM_BUFFER, 0)) < 0) {
        close(*(int *) accept_sd);
        pthread_exit(NULL);
    }
}

void SendToSocket(void *accept_sd, void *bufferMessage) {
    int rc;
    pthread_mutex_lock(&sendMutex);
    if ((rc = send(*(int *) accept_sd, (char *) bufferMessage, strlen((char *) bufferMessage), 0)) < 0) {
        close(*(int *) accept_sd);
        pthread_exit(NULL);
    }
    sleep(1);
    if (rc == strlen((char *) bufferMessage))
        pthread_mutex_unlock(&sendMutex);
}

int VerifyOrRegisterUser(void *cliente_sockfd, void *File, int option) {
    char registry[TAM_BUFFER];
    SendToSocket(&*(int *) cliente_sockfd, "Nombre de usuario: ");
    RecievFSocket(&*(int *) cliente_sockfd, &username);
    SendToSocket(&*(int *) cliente_sockfd, "Password: ");
    RecievFSocket(&*(int *) cliente_sockfd, &password);
    strcat(password, "\n");
    File = fopen("Users.txt", "r");
    while (fgets(registry, sizeof(registry), File)) {
        char *results = strtok(registry, " ");
        if (strcmp(results, username) != 0)
            continue;
        if (option == 2) {
            fclose(File);
            return 0;
        }
        results = strtok(NULL, " ");
        if (strcmp(results, password) != 0) {
            fclose(File);
            return 0;
        }
        fclose(File);
        return 1;
    }
    fclose(File);
    if (option == 1)
        return 0;
    File = fopen("Users.txt", "a");
    pthread_rwlock_wrlock(&rwlock);
    fprintf(File, "%s %s", username, password);
    pthread_rwlock_unlock(&rwlock);
    fclose(File);
    return 1;
}