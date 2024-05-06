#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LENGTH 3000

// Global variables
int exit_status = 0;
int sockfd = 0;
char name[32];

void manage_exit(int sig) {
    exit_status = 1;
}



void receiver() {
    char message[LENGTH] = {};
    while (1) {
        int receive = recv(sockfd, message, LENGTH, 0);
        if (receive > 0) {
            printf("%s", message);
            printf("%s", "$ ");
            fflush(stdout);
        } else if (receive == 0) {
            break;
        } else {

        }
        memset(message, 0, sizeof(message));
    }
}


void trim_newline (char* str) {
    int i = 0;
    while (str[i] != '\0') {
        if (str[i] == '\n') {
            str[i] = '\0';
            break;
        }
        i++;
    }
}

void sender() {
    char message[LENGTH] = {};
    char buffer[LENGTH + 32] = {};

    while(1) {
        printf("%s", "$ ");
        fflush(stdout);
        fgets(message, LENGTH, stdin);
        trim_newline(message);

        if (strcmp(message, "/exit") == 0) {
            printf("Saliendo...");
            break;
        } else if (strcmp(message, "/activo") == 0){
            printf("Cambiar estado a activo\n");
            send(sockfd, message, strlen(message), 0);
        } else if (strcmp(message, "/inactivo") == 0) {
            printf("Cambiar estado a inactivo\n");
            send(sockfd, message, strlen(message), 0);
        } else if (strcmp(message, "/ocupado") == 0) {
            printf("Cambiar estado a ocupado\n");
            send(sockfd, message, strlen(message), 0);
        } else if (strcmp(message, "/list") == 0) {
            printf("Mostrar lista de usuarios\n");
            sprintf(buffer, "%s: %s\n", name, message);
            send(sockfd, buffer, strlen(buffer), 0);
        } else if (strstr(message, "/priv")) { // /priv <to> <message>
            printf("Mandar mensaje privado\n");
            send(sockfd, message, strlen(message), 0);
        } else if (strstr(message, "/info")) { // /info <user>
            printf("Buscar informacion de usuario\n");
            send(sockfd, message, strlen(message), 0); 
        } else {
            sprintf(buffer, "%s: %s\n", name, message);
            send(sockfd, buffer, strlen(buffer), 0);
        }

        memset(message, 0, LENGTH);
        memset(buffer, 0, LENGTH + 32);
    }
    manage_exit(2);
}



int main(int argc, char **argv){
    if(argc != 2){
        printf("Uso: %s <puerto>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = "127.0.0.1";
    int port = atoi(argv[1]);

    signal(SIGINT, manage_exit);

    printf("Cual es tu nombre?: ");
    fgets(name, 32, stdin);
    trim_newline(name);

    if (strlen(name) > 32 || strlen(name) < 2){
        printf("El nombre debe tener una longitud de maxima de 30 caracteres y minima de 2 caracteres\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    // Conectar al server
    int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == -1) {
        printf("ERROR: conectar al server\n");
        return EXIT_FAILURE;
    }

    // Send name
    send(sockfd, name, 32, 0);

    printf("\n");
	printf("+-----------------------+\n");
	printf("|   ¡Entrando al chat!  |\n");
	printf("+-----------------------+\n");
	printf("\n");


    pthread_t send_msg_thread;
    if(pthread_create(&send_msg_thread, NULL, (void *) sender, NULL) != 0){
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    pthread_t recv_msg_thread;
    if(pthread_create(&recv_msg_thread, NULL, (void *) receiver, NULL) != 0){
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    while (1){
        if(exit_status){
            printf("\nGracias por tu tiempo!\n");
            break;
        }
    }

    close(sockfd);

    return EXIT_SUCCESS;
}
