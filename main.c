#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define PUERTO 5000
#define COLA_CLIENTES 50
#define TAM_BUFFER 200

void *client_menu(void *sockfd);

void main() {
    int sockfd, cliente_sockfd;
    pthread_t id_hilo;
    struct sockaddr_in direccion_servidor;
    memset(&direccion_servidor, 0, sizeof(direccion_servidor));
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_port = htons(PUERTO);
    direccion_servidor.sin_addr.s_addr = INADDR_ANY;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        exit(1);
    }
    if (bind(sockfd, (struct sockaddr *) &direccion_servidor, sizeof(direccion_servidor)) < 0) {
        exit(1);
    }
    if (listen(sockfd, COLA_CLIENTES) < 0) {
        exit(1);
    }
    while (1) {
        if ((cliente_sockfd = accept(sockfd, NULL, NULL)) < 0) {
            printf("Error al conectar con cliente");
        }
        if (pthread_create(&id_hilo, NULL, client_menu, (void *) &cliente_sockfd) != 0) {
            close(cliente_sockfd);
        }
    }
}

void *client_menu(void *sockfd) {
    char buffermessage[TAM_BUFFER];
    int cliente_sockfd;
    cliente_sockfd = *((int *) sockfd);
    int flag=0;
    while (flag==0) {
        strcpy(buffermessage, "\nQue desea realizar?\n1)Iniciar Sesion\n2)Registrarse\n3)Terminar la aplicacion\n");
        if (write(cliente_sockfd, buffermessage, strlen(buffermessage) + 1) < 0) {
            perror("Ocurrio un problema en el envio de un mensaje al cliente");
            exit(1);
        }
        memset(buffermessage, 0, sizeof(buffermessage));
        if (read(cliente_sockfd, &buffermessage, TAM_BUFFER) < 0) {
            perror("Ocurrio algun problema al recibir datos del cliente");
            exit(1);
        }
        switch (atoi(buffermessage)) {
            case 1:
                printf("Hola\n");
                break;
            case 2:
                printf("Holda2\n");
                break;
            case 3:
                flag=1;
                break;
            default:
                printf("%s",buffermessage);
                system("clear");
                break;
        }
    }
    if (write(cliente_sockfd, "quit()", 7) < 0) {
        perror("Ocurrio un problema en el envio de un mensaje al cliente");
        exit(1);
    }
    close(cliente_sockfd);
    pthread_exit(NULL);
}