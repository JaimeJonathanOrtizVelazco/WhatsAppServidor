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
#define TAM_BUFFER 1024

pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER; //Se inicializa el mutex para la escritura y lectura de archivos
// sincronizada, asi evitando problemas con los miltiples clientes
void client(void *accept_sd);

int LoginRegisterMenu(void *accept_sd, void *username);

int Login(void *accept_sd, void *username);

void Register(void *cliente_sockfd);

void LoggedUserMenu(void *accept_sd, void *username);

void ContactMenu(void *accept_sd, void *username);

void GetContactList(void *accept_sd, void *username);

void AddContact(void *accept_sd, void *username);

void DeleteContact(void *accept_sd, void *username);

void DeleteLineUser(void *fileRoute, void *value1, void *value2);

int UserExist(void *fileName, void *username);

void Chat(void *accept_sd, void *username);

void RecevMSG(void *args);

void SendMSG(void *args);

struct args {
    int accept_sd;
    char *username;
    char *contact;
};

void WritePipe(void *user, void *contact, void *message);

void ReadPipe(void *user, void *contact, void *buffer);

void SendToSocket(void *accept_sd, void *bufferMessage);

void RecievFSocket(void *accept_sd, void *variable);

pthread_mutex_t sendMutex;// Se inicializa el mutex para el envio de datos a los clientes, asi se evita que se traslapen
//mensajes y puedan llegar por separado

int main() {
    int option = 1;
    int sockfd, cliente_sockfd;
    pthread_t id_hilo;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));//Configuracion inicial del servidor para que acepte conexiones TCP y multiples
    addr.sin_family = AF_INET;//usuarios
    addr.sin_port = htons(PUERTO);
    addr.sin_addr.s_addr = INADDR_ANY;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        exit(-1);
    }
    if (setsockopt(sockfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *) &option, sizeof(option)) < 0) {
        return EXIT_FAILURE;
    }
    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        return EXIT_FAILURE;
    }
    if (listen(sockfd, COLA_CLIENTES) < 0) {
        return EXIT_FAILURE;
    }
    while (1) {
        if ((cliente_sockfd = accept(sockfd, NULL, NULL)) < 0) {
            return EXIT_FAILURE;
        }//Cada vez que es aceptado un cliente nuevo, este es asignado a un nuevo hilo
        if (pthread_create(&id_hilo, NULL, (void *) client, (void *) &cliente_sockfd) != 0) {
            return EXIT_FAILURE;
        }
    }
}

void client(void *accept_sd) {
    char username[TAM_BUFFER];
    int fd = *((int *) accept_sd);
    while (1) {
        if (LoginRegisterMenu(&fd, &username) == 0)
            break;
        LoggedUserMenu(&fd, &username);
    }
    close(fd);
    pthread_exit(NULL);
}

int LoginRegisterMenu(void *accept_sd, void *username) {
    char buffermessage[TAM_BUFFER];
    while (1) {
        int exit = 0;
        SendToSocket(&*(int *) accept_sd, "BIENVENIDO A TU CHAT");
        SendToSocket(&*(int *) accept_sd, "\t1)Iniciar Sesion");
        SendToSocket(&*(int *) accept_sd, "\t2)Registrarse");
        SendToSocket(&*(int *) accept_sd, "\t3)Terminar la aplicacion");
        while (exit == 0) {
            RecievFSocket(&*(int *) accept_sd, &buffermessage);
            int caseOption = atoi(buffermessage);
            switch (caseOption) {
                case 1:
                    if (Login(&*(int *) accept_sd, username) == 1)
                        return 1;
                    else
                        exit = 1;
                    break;
                case 2:
                    Register(&*(int *) accept_sd);
                    exit = 1;
                    break;
                case 3:
                    SendToSocket(&*(int *) accept_sd, "exit()");
                    return 0;
                default:
                    SendToSocket(&*(int *) accept_sd, "Selecciona un opcion correcta");
                    break;
            }
        }
    }
}

int Login(void *accept_sd, void *username) {
    FILE *File = NULL;
    char registry[TAM_BUFFER];
    char password[TAM_BUFFER];
    char response[1];
    while (1) {
        memset(username, 0, TAM_BUFFER);
        memset(password, 0, TAM_BUFFER);
        SendToSocket(&*(int *) accept_sd, "Nombre de usuario:");
        RecievFSocket(&*(int *) accept_sd, username);
        SendToSocket(&*(int *) accept_sd, "Password:");
        RecievFSocket(&*(int *) accept_sd, &password);
        pthread_rwlock_rdlock(&rwlock);//Se sincronizan los lectores y escritores de archivos
        File = fopen("Users.txt", "r");
        if (File == NULL) {
            fclose(File);
            pthread_rwlock_unlock(&rwlock);
            pthread_rwlock_wrlock(&rwlock);
            fopen("Users.txt", "a");
            fclose(File);
            pthread_rwlock_unlock(&rwlock);
            pthread_rwlock_rdlock(&rwlock);
            File = fopen("Users.txt", "r");
        }
        while (fgets(registry, sizeof(registry), File)) {
            char *results = strtok(registry, " ");
            if (strcmp(results, username) != 0)
                continue;
            results = strtok(NULL, " ");
            if (strcmp(results, password) != 0) {//Se valida si el usuario y cotrasena son correctos a los registrados
                SendToSocket(&*((int *) accept_sd), "Usuario o contrasena erroneos");
                break;
            }
            fclose(File);
            pthread_rwlock_unlock(&rwlock);
            SendToSocket(&*((int *) accept_sd), "Se ha loggeado exitosamente");
            return 1;
        }
        fclose(File);
        pthread_rwlock_unlock(&rwlock);
        SendToSocket(&*((int *) accept_sd), "Desea volver a intentarlo? s/n");
        RecievFSocket(&*(int *) accept_sd, &response);
        if (strcmp(response, "s") == 0)
            continue;
        return 0;
    }
}

void Register(void *cliente_sockfd) {
    char usernameFile[30];
    char username[TAM_BUFFER];
    char password[TAM_BUFFER];
    char response[1];
    while (1) {
        memset(username, 0, TAM_BUFFER);
        memset(password, 0, TAM_BUFFER);
        SendToSocket(&*(int *) cliente_sockfd, "Nombre de usuario:");
        RecievFSocket(&*(int *) cliente_sockfd, &username);
        SendToSocket(&*(int *) cliente_sockfd, "Password:");
        RecievFSocket(&*(int *) cliente_sockfd, &password);
        int exist = UserExist("Users.txt", username);//Con este se verifica si un usuario existo
        if (exist == 1) {
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
        FILE *File = fopen("Users.txt", "a");
        fprintf(File, "%s %s %s\n", username, password, usernameFile);//Se agrega el nuevo usuario a la lista de
        fclose(File);//usuarios existentes
        File = fopen(usernameFile, "w");
        fclose(File);
        pthread_rwlock_unlock(&rwlock);
        SendToSocket(&*(int *) cliente_sockfd, "Usuario registrado con exito");
        break;
    }
}

void LoggedUserMenu(void *accept_sd, void *username) {
    char buffermessage[TAM_BUFFER];
    char welcome[TAM_BUFFER] = "Bienvenido ";
    strcat(welcome, (char *) username);
    int out = 0;
    while (out == 0) {
        int exit = 0;
        SendToSocket(&*(int *) accept_sd, welcome);
        SendToSocket(&*(int *) accept_sd, "1)Mis contactos");
        SendToSocket(&*(int *) accept_sd, "2)Cerrar sesion");
        while (exit == 0) {
            RecievFSocket(&*(int *) accept_sd, &buffermessage);
            int caseOption = atoi(buffermessage);
            switch (caseOption) {
                case 1:
                    ContactMenu(&*(int *) accept_sd, username);
                    exit = 1;
                    break;
                case 2:
                    out = 1;
                    exit = 1;
                    break;
                default:
                    SendToSocket(&*(int *) accept_sd, "Selecciona un opcion correcta");
                    break;
            }
        }
    }
}

void ContactMenu(void *accept_sd, void *username) {
    char buffermessage[TAM_BUFFER];
    int out = 0;
    while (out == 0) {
        int exit = 0;
        SendToSocket(&*(int *) accept_sd, "Lista de amigos:");
        GetContactList(&*(int *) accept_sd, username);
        SendToSocket(&*(int *) accept_sd, "1)Iniciar conversacion\t2)Agregar contacto\t3)Eliminar Contacto\t4)Salir");
        while (exit == 0) {
            RecievFSocket(&*(int *) accept_sd, &buffermessage);
            int caseOption = atoi(buffermessage);
            switch (caseOption) {
                case 1:
                    Chat(&*(int *) accept_sd, username);
                    exit = 1;
                    break;
                case 2:
                    AddContact(&*(int *) accept_sd, username);
                    exit = 1;
                    break;
                case 3:
                    DeleteContact(&*(int *) accept_sd, username);
                    exit = 1;
                    break;
                case 4:
                    out = 1;
                    exit = 1;
                    break;
                default:
                    SendToSocket(&*(int *) accept_sd, "Selecciona un opcion correcta");
                    break;
            }
        }
    }
}

void Chat(void *accept_sd, void *username) {
    char buffermessage[TAM_BUFFER];
    char FileRoute[TAM_BUFFER];
    memset(buffermessage, 0, TAM_BUFFER);
    SendToSocket(&*(int *) accept_sd, "Nombre del usuario a chatear:");
    RecievFSocket(&*(int *) accept_sd, &buffermessage);
    strncpy(FileRoute, username, (strlen(username) + 1));
    strcat(FileRoute, ".txt");
    int exist = UserExist(FileRoute,
                          buffermessage);//De nuevo se valida si el usuario con el que se quiere chatear existe
    if (exist == 0) {//en la lista de amigos del cliente
        SendToSocket(&*(int *) accept_sd, "Usuario no encontrado en tu lista de amigos");
    } else {
        pthread_t RecevMSGT, SendMSGT;
        struct args *ChatStruct = (struct args *) malloc(sizeof(struct args));
        ChatStruct->accept_sd = *(int *) accept_sd;
        ChatStruct->username = username;
        ChatStruct->contact = buffermessage;//Se crea el hilo que va a estar mandando los mensajes entre clientes
        if (pthread_create(&SendMSGT, NULL, (void *) SendMSG, (void *) ChatStruct) != 0) {
            close(*(int *) accept_sd);
        }//Se crea el hilo que va a estar recibiendo mensajes entre clientes
        if (pthread_create(&RecevMSGT, NULL, (void *) RecevMSG, (void *) ChatStruct) != 0) {
            close(*(int *) accept_sd);
        }
        pthread_join(SendMSGT, NULL);
        pthread_cancel(RecevMSGT);
    }
}

void RecevMSG(void *args) {
    char buffer[TAM_BUFFER];
    char message[TAM_BUFFER];
    while (1) {
        memset(message, 0, TAM_BUFFER);//Se lee la tuberia que va del contacto al cliente, para asi obtener los mensajes
        ReadPipe(((struct args *) args)->username, ((struct args *) args)->contact, buffer);
        strcpy(message, buffer);//Posteriormente son enviados al cliente
        SendToSocket(&((struct args *) args)->accept_sd, message);
    }
    pthread_exit(NULL);
}

void ReadPipe(void *user, void *contact, void *buffer) {
    memset(buffer, 0, TAM_BUFFER);
    char *FifoName = (char *) malloc(strlen(contact) + strlen(user) + 1);
    strcpy(FifoName, contact);
    strcat(FifoName, user);
    int fd = open(FifoName, O_RDONLY | O_NONBLOCK);
    while ((read(fd, buffer, TAM_BUFFER)) <= 0) { sleep(1 / 3); };
    close(fd);
    free(FifoName);
}

void SendMSG(void *args) {
    char *message = (char *) malloc(TAM_BUFFER);
    strcpy(message, ((struct args *) args)->username);
    strcat(message, " se ha unido a la conversacion");//Se escribe en la tuberia del cliente que va hacia el contacto
    WritePipe(((struct args *) args)->username, ((struct args *) args)->contact, message);
    char *buffer = (char *) malloc(TAM_BUFFER);
    while (1) {
        memset(buffer, 0, TAM_BUFFER);
        memset(message, 0, TAM_BUFFER);
        RecievFSocket(&((struct args *) args)->accept_sd, buffer);//Si se recibe un mensaje del cliente, este es escrito
        if (strcmp(buffer, "exit()") ==
            0) {//en la tuberia que va del cliente al contacto y asi el contacto pueda recibirlo
            strcpy(message, ((struct args *) args)->username);
            strcat(message, " ha salido de la conversacion");
            WritePipe(((struct args *) args)->username, ((struct args *) args)->contact, message);
            break;
        }
        strcpy(message, ((struct args *) args)->username);
        strcat(message, ":");
        strcat(message, buffer);
        WritePipe(((struct args *) args)->username, ((struct args *) args)->contact,
                  message);//Se escribe el mensaje recibido
    }
    pthread_exit(NULL);
}

void WritePipe(void *user, void *contact, void *message) {
    char *FifoName = (char *) malloc(strlen(user) + strlen(contact) + 1);
    strcpy(FifoName, user);
    strcat(FifoName, contact);
    int fd = open(FifoName, O_WRONLY | O_NONBLOCK);
    write(fd, message, strlen(message));
    close(fd);
    free(FifoName);
}

void AddContact(void *accept_sd, void *username) {
    char buffer[TAM_BUFFER];
    memset(buffer, 0, TAM_BUFFER);
    SendToSocket(&*(int *) accept_sd, "Nombre del usuario a agregar:");
    RecievFSocket(&*(int *) accept_sd, &buffer);
    char *Fifo1 = (char *) malloc(strlen(username) + strlen(buffer) + 1);
    strcpy(Fifo1, username);
    strcat(Fifo1, buffer);
    char *Fifo2 = (char *) malloc(strlen(username) + strlen(buffer) + 1);
    strcpy(Fifo2, buffer);
    strcat(Fifo2, username);
    char *FileRoute = (char *) malloc(strlen(username) + 5);
    strcpy(FileRoute, username);
    strcat(FileRoute, ".txt");
    pthread_rwlock_wrlock(&rwlock);
    FILE *File = fopen(FileRoute, "a");
    fprintf(File, "%s %s\n", buffer, Fifo1);//Se agrega el nuevo contacto a la lista de contactos del cliente
    mkfifo(Fifo1, 0666);//Se crean la tuberia de comunicacion entre pares, tanto de ida
    fclose(File);
    pthread_rwlock_unlock(&rwlock);
    free(FileRoute);
    FileRoute = (char *) malloc(strlen(buffer) + 5);
    strcpy(FileRoute, buffer);
    strcat(FileRoute, ".txt");
    pthread_rwlock_wrlock(&rwlock);
    File = fopen(FileRoute, "a");
    fprintf(File, "%s %s\n", (char *) username, Fifo2);
    mkfifo(Fifo2, 0666);// como de regreso de los mensajes
    fclose(File);
    pthread_rwlock_unlock(&rwlock);
    free(FileRoute);
}

void DeleteContact(void *accept_sd, void *username) {
    char UserToDelete[TAM_BUFFER];
    char FileRoute[TAM_BUFFER];
    SendToSocket(&*(int *) accept_sd, "Que usuario desea eliminar?");
    RecievFSocket(&*(int *) accept_sd, &UserToDelete);
    strncpy(FileRoute, username, (strlen(username) + 1));
    strcat(FileRoute, ".txt");
    int exist = UserExist(FileRoute, UserToDelete);//Se valida que el usuario que se quiere eliminar exista
    if (exist == 0) {
        SendToSocket(&*(int *) accept_sd, "El usuario no existe");
    } else {
        DeleteLineUser(FileRoute, username, UserToDelete);
        strncpy(FileRoute, UserToDelete, (strlen(UserToDelete) + 1));
        strcat(FileRoute, ".txt");
        DeleteLineUser(FileRoute, UserToDelete, username);
        SendToSocket(&*(int *) accept_sd, "El usuario fue eliminado exitosamente");
    }
}

void DeleteLineUser(void *fileRoute, void *value1, void *value2) {
    char tempName[TAM_BUFFER];
    char tempRegistry[TAM_BUFFER];
    char registry[TAM_BUFFER];
    pthread_rwlock_wrlock(&rwlock);
    FILE *srcFile = fopen(fileRoute, "r");
    strncpy(tempName, value1, (strlen(value1) + 1));
    strcat(tempName, ".tmp");//Se crea un archivo temporal y se compara linea a linea si el usuario que se quiere
    FILE *tempFile = fopen(tempName, "w");//eliminar se encuentra en determinada linea, sino es asi se escriben
    while (fgets(registry, sizeof(registry), srcFile)) {//en el archivo temporal, asi si la encuentra no la escribe
        strcpy(tempRegistry, registry);
        char *results = strtok(tempRegistry, " ");
        if (strcmp(results, value2) != 0)
            fputs(registry, tempFile);
    }
    fclose(srcFile);
    fclose(tempFile);
    remove(fileRoute);// Se elimina el archivo original y el archivo temporal es renombrado como el original
    rename(tempName, fileRoute);
    pthread_rwlock_unlock(&rwlock);
}

int UserExist(void *fileName, void *username) {// Metodo generico para buscar usuarios dentro de archivos
    char registry[TAM_BUFFER];
    int exist = 0;
    pthread_rwlock_rdlock(&rwlock);//Sincronizacion de lectura de archivos
    FILE *File = fopen(fileName, "r");
    if (File == NULL) {
        fclose(File);
        pthread_rwlock_unlock(&rwlock);
        pthread_rwlock_wrlock(&rwlock);
        fopen(fileName, "a");
        fclose(File);
        pthread_rwlock_unlock(&rwlock);
        pthread_rwlock_rdlock(&rwlock);
        File = fopen(fileName, "r");
    }
    while (fgets(registry, sizeof(registry), File)) {
        char *results = strtok(registry, " ");
        if (strcmp(results, username) != 0)
            continue;
        exist = 1;
    }
    fclose(File);
    pthread_rwlock_unlock(&rwlock);
    return exist;
}

void GetContactList(void *accept_sd, void *username) {// Metodo para obtener la lista de amigos de determinado usuario
    char registry[TAM_BUFFER];
    char FileRoute[TAM_BUFFER];
    strncpy(FileRoute, username, (strlen(username) + 1));
    strcat(FileRoute, ".txt");
    pthread_rwlock_rdlock(&rwlock);
    FILE *File = fopen(FileRoute, "r");
    while (fgets(registry, sizeof(registry), File)) {
        char *results = strtok(registry, " ");
        char NameUser[strlen(results) + 1];
        strcpy(NameUser, results);
        SendToSocket(&*(int *) accept_sd, NameUser);
    }
    fclose(File);
    pthread_rwlock_unlock(&rwlock);
}

void RecievFSocket(void *accept_sd, void *variable) {//Metodo que se encarga de recibir un mensaje a determinado cliente
    memset(variable, 0, sizeof(variable));
    int reciv = 0;
    while (reciv == 0) {
        reciv = recv(*(int *) accept_sd, variable, TAM_BUFFER, 0);//Se espera hasta que se reciba un mensaje
        sleep(1 / 3);
    }
}

void SendToSocket(void *accept_sd, void *bufferMessage) {
    pthread_mutex_lock(&sendMutex);//Se utilizan mutex para evitar el traslape de mensajes enviados desde el servidor,
    if ((send(*(int *) accept_sd, bufferMessage, strlen((char *) bufferMessage), 0)) < 0) {//en caso de que se
        close(*(int *) accept_sd);//quieran enviar mensajes de manera consecutiva sin tener un receptor antes del envio
        pthread_exit(NULL);
    }
    sleep(1 / 3);
    pthread_mutex_unlock(&sendMutex);
}