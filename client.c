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
#include <stdbool.h> 
#include "chat.pb-c.h"

#define LENGTH 3000
#define BUFFER_SIZE 1024
#define MAX_QUEUE_SIZE 100 

#define MAX_HISTORY_SIZE 100


// Global variables
int exit_status = 0;
int sockfd = 0;
char name[32];
bool printSend = false;
bool isInGeneral = false;

void manage_exit(int sig) {
    exit_status = 1;
}

struct ThreadArgs {
    int sock;
    const char *username;
};




void receiver() {
    uint8_t message[LENGTH] = {};
    while (1) {
        int receive = recv(sockfd, message, LENGTH, 0);
        if (receive > 0) {


            // Intenta desempaquetar como un mensaje Protobuf
            Chat__SendMessageRequest *send_message = chat__send_message_request__unpack(NULL, receive, message);
            if (send_message == NULL) {
                // Si falla el desempaquetado, asume que es un mensaje de texto sin procesar
                message[receive] = '\0';  // Asegúrate de que el mensaje esté null-terminated
                
                printf("%s\n", message);
            } else {
                // Maneja el mensaje Protobuf
                printf("[Mensaje general]: %s\n", send_message->content);
                chat__send_message_request__free_unpacked(send_message, NULL);
            }
            //printf("%s", "$ ");
            fflush(stdout);


            // Intenta desempaquetar como un mensaje Protobuf
            Chat__Response *response = chat__response__unpack(NULL, receive, message);
            if (response == NULL) {
                // Si falla el desempaquetado, asume que es un mensaje de texto sin procesar
                message[receive] = '\0';  // Asegúrate de que el mensaje esté null-terminated
                //printf("%s\n", message);
            } else {
                // Maneja el mensaje Protobuf
                if (response->operation == CHAT__OPERATION__INCOMING_MESSAGE && response->incoming_message) {
                    Chat__IncomingMessageResponse *incoming_message = response->incoming_message;
                    printf("[%s]: %s\n", incoming_message->sender, incoming_message->content);
                } else if (response->operation == CHAT__OPERATION__GET_USERS && response->user_list) {
                    printf("Usuarios|Estado\n");
                    for (size_t i = 0; i < response->user_list->n_users; i++) {
                        Chat__User *user = response->user_list->users[i];
                        const char *status;
                        switch (user->status) {
                            case 0:
                                status = "activo";
                                break;
                            case 1:
                                status = "ocupado";
                                break;
                            case 2:
                                status = "inactivo";
                                break;
                            default:
                                status = "desconocido";
                                break;
                        }
                        printf("%s|%s\n", user->username, status);
                    }
                }

                chat__response__free_unpacked(response, NULL);
            }
            printf("%s", "$ ");
            fflush(stdout);
        } else if (receive == 0) {
            break;
        } else {
            // Error receiving message
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


void sender(void *args_ptr) {
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
        }

        Chat__Request request = CHAT__REQUEST__INIT;

        if (strcmp(message, "/activo") == 0) {
            printf("Cambiar estado a activo\n");
            request.operation = CHAT__OPERATION__UPDATE_STATUS;
            Chat__UpdateStatusRequest update_status_request = CHAT__UPDATE_STATUS_REQUEST__INIT;
            request.payload_case = CHAT__REQUEST__PAYLOAD_UPDATE_STATUS;
            update_status_request.username = name;
            update_status_request.new_status = CHAT__USER_STATUS__ONLINE;
            request.update_status = &update_status_request;
        } else if (strcmp(message, "/inactivo") == 0) {
            printf("Cambiar estado a inactivo\n");
            request.operation = CHAT__OPERATION__UPDATE_STATUS;
            Chat__UpdateStatusRequest update_status_request = CHAT__UPDATE_STATUS_REQUEST__INIT;
            request.payload_case = CHAT__REQUEST__PAYLOAD_UPDATE_STATUS;
            update_status_request.username = name;
            update_status_request.new_status = CHAT__USER_STATUS__OFFLINE;
            request.update_status = &update_status_request;
        } else if (strcmp(message, "/ocupado") == 0) {
            printf("Cambiar estado a ocupado\n");
            request.operation = CHAT__OPERATION__UPDATE_STATUS;
            Chat__UpdateStatusRequest update_status_request = CHAT__UPDATE_STATUS_REQUEST__INIT;
            request.payload_case = CHAT__REQUEST__PAYLOAD_UPDATE_STATUS;
            update_status_request.username = name;
            update_status_request.new_status = CHAT__USER_STATUS__BUSY;
            request.update_status = &update_status_request;
        } else if (strcmp(message, "/list") == 0) {
            printf("Mostrar lista de usuarios\n");
            request.operation = CHAT__OPERATION__GET_USERS;
            Chat__UserListRequest user_list_request = CHAT__USER_LIST_REQUEST__INIT;
            user_list_request.username = "";
            request.get_users = &user_list_request;

            size_t request_size = chat__request__get_packed_size(&request);
            uint8_t *request_buffer = malloc(request_size);
            chat__request__pack(&request, request_buffer);
            send(sockfd, request_buffer, request_size, 0);
            free(request_buffer);
        } else if (strncmp(message, "/priv ", 6) == 0) { // /priv <to> <message>
            // Inicializar la solicitud
            Chat__Request request = CHAT__REQUEST__INIT;
            request.operation = CHAT__OPERATION__SEND_MESSAGE;

            // Preparar el mensaje
            Chat__SendMessageRequest send_message_request = CHAT__SEND_MESSAGE_REQUEST__INIT;
            request.payload_case = CHAT__REQUEST__PAYLOAD_SEND_MESSAGE;
            send_message_request.recipient = (char *)"Usuario"; // Mensaje de difusión
            send_message_request.content = (char *)message; // Contenido del mensaje

            request.send_message = &send_message_request;

            // Empaquetar la solicitud
            size_t request_size = chat__request__get_packed_size(&request);
            uint8_t *request_buffer = malloc(request_size);
            chat__request__pack(&request, request_buffer);

            // Enviar la solicitud al servidor
            if (send(sockfd, request_buffer, request_size, 0) < 0) {
                perror("ERROR: No se pudo enviar el mensaje");
            }

            // Limpiar la memoria
            free(request_buffer);
            //free(send_message_request.recipient);
            //free(send_message_request.content);

        } else if (strstr(message, "/info")) { // /info <user>
            printf("\nSe busca info de usuario\n");
            request.operation = CHAT__OPERATION__GET_USERS;
            Chat__UserListRequest user_list_request = CHAT__USER_LIST_REQUEST__INIT;
            request.payload_case = CHAT__REQUEST__PAYLOAD_GET_USERS;
            user_list_request.username = (char *)message;
            request.get_users = &user_list_request;

            size_t request_size = chat__request__get_packed_size(&request);
            uint8_t *request_buffer = malloc(request_size);
            chat__request__pack(&request, request_buffer);
            send(sockfd, request_buffer, request_size, 0);
            free(request_buffer);
        } else {
            // Inicializar la solicitud
            Chat__Request request = CHAT__REQUEST__INIT;
            request.operation = CHAT__OPERATION__SEND_MESSAGE;

            // Preparar el mensaje
            Chat__SendMessageRequest send_message_request = CHAT__SEND_MESSAGE_REQUEST__INIT;
            request.payload_case = CHAT__REQUEST__PAYLOAD_SEND_MESSAGE;
            send_message_request.recipient = (char *)""; // Mensaje de difusión
            send_message_request.content = (char *)message; // Duplicar el contenido del mensaje

            request.send_message = &send_message_request;

            // Empaquetar la solicitud
            size_t request_size = chat__request__get_packed_size(&request);
            uint8_t *request_buffer = malloc(request_size);
            chat__request__pack(&request, request_buffer);

            // Verificar el contenido del buffer antes de enviar
            //printf("Contenido del buffer a enviar: %s\n", message);

            send(sockfd, request_buffer, request_size, 0);
            // Enviar la solicitud al servidor
            if (send(sockfd, request_buffer, request_size, 0) < 0) {
                perror("ERROR: No se pudo enviar el mensaje");
            }

            free(request_buffer);
            //free(send_message_request.content);
        }

        // Empaquetar y enviar la solicitud
        size_t request_size = chat__request__get_packed_size(&request);
        uint8_t *request_buffer = malloc(request_size);
        chat__request__pack(&request, request_buffer);

        send(sockfd, request_buffer, request_size, 0);

        free(request_buffer);

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
