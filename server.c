#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include "chat.pb-c.h"

#define LIMIT_CLIENT 10
#define MSG_LIMIT 3000

static _Atomic unsigned int cli_count = 0;
static int uid = 10;
#define LENGTH 3000

// Estructura del cliente
typedef struct{
	struct sockaddr_in address;
	int state; // 0="activo", 1="ocupado" o 2="inactivo"
	char name[32];
	int sockfd;
	int uid;
} client_obj;

client_obj *clients[LIMIT_CLIENT+1]; // Array de clientes

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

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


void print_client_addr(struct sockaddr_in addr){
    printf("%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

// Registro de usuarios
void register_user(client_obj *cl){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < LIMIT_CLIENT; ++i){
		if(!clients[i]){
			clients[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Liberacion de usuarios
void free_user(int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < LIMIT_CLIENT; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

//Mensaje global
void send_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<LIMIT_CLIENT; i++){
		if(clients[i]){
			if(clients[i]->uid != uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: escritura fallida");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}



void send_one_message(char *msg, int uid_sender, int uid_receiver) {


    printf("Entra a send_one_message\n");

    for (int i = 0; i < LIMIT_CLIENT; i++) {
        if (clients[i] && clients[i]->uid == uid_receiver) {
            printf("Sigue\n");
            Chat__Response server_response = CHAT__RESPONSE__INIT;
            server_response.operation = CHAT__OPERATION__INCOMING_MESSAGE;
            server_response.status_code = CHAT__STATUS_CODE__OK;

            // Create and populate the incoming message response
            Chat__IncomingMessageResponse incoming_message = CHAT__INCOMING_MESSAGE_RESPONSE__INIT;
            incoming_message.sender = strdup("SERVER");  // You can set the sender as desired
            incoming_message.content = strdup(msg);

            // Assign the incoming message to the response
            server_response.incoming_message = &incoming_message;
            server_response.result_case = CHAT__RESPONSE__RESULT_INCOMING_MESSAGE;

            printf("Paso chavales\n");

            // Pack the response into a buffer
            size_t response_size = chat__response__get_packed_size(&server_response);
            uint8_t *response_buffer = malloc(response_size);
            chat__response__pack(&server_response, response_buffer);

            // Send the response buffer to the client
            if (write(clients[i]->sockfd, response_buffer, response_size) < 0) {
                perror("Error: Fallo de mensaje privado");
                free(response_buffer);  // Free the response buffer in case of error
                break;
            }

            free(response_buffer);  // Free the response buffer after sending

            // Exit the loop after sending the message to the intended recipient
            break;
        }
    }

    // Ensure the mutex is unlocked even in case of errors

}




void send_private_msg(char *msg, char *receiver_name) {
	pthread_mutex_lock(&clients_mutex);
	

	for(int i = 0; i<LIMIT_CLIENT; i++){
		if(clients[i]){
			if(strcmp(clients[i]->name, receiver_name) == 0){
				if(write(clients[i]->sockfd, msg, strlen(msg))<0){
					perror("Error: Fallo de mensaje privado generado por user");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);

}




void list_all_users(int uid) {
    //printf("Entra en list_all_users\n");
    pthread_mutex_lock(&clients_mutex);
    //printf("Mutex bloqueado\n");

    Chat__UserListResponse response = CHAT__USER_LIST_RESPONSE__INIT;
    response.users = malloc(LIMIT_CLIENT * sizeof(Chat__User*));
    response.n_users = 0;
    response.type = CHAT__USER_LIST_TYPE__ALL;

    //printf("LIMIT_CLIENT: %d\n", LIMIT_CLIENT);
    for (int i = 0; i < LIMIT_CLIENT; ++i) {
        if (clients[i]) {
            //printf("Cliente UID: %d\n", clients[i]->uid);
            //printf("Cliente Name: %s\n", clients[i]->name);
            //printf("Cliente State: %d\n", clients[i]->state);
            //printf("Cliente Socket FD: %d\n", clients[i]->sockfd);

            if (clients[i]->uid != uid) {
                printf("Procesando usuario con uid: %d\n", clients[i]->uid);
                Chat__User *user = malloc(sizeof(Chat__User));
                *user = (Chat__User) CHAT__USER__INIT;
                user->username = strdup(clients[i]->name);
                user->status = clients[i]->state;

                // Print the user information for debugging
                printf("User: %s | Status: %d\n", user->username, user->status);

                response.users[response.n_users++] = user;
            }
        } else {
            printf("Cliente es NULL\n");
        }
    }

    // Convertir la lista de usuarios a una cadena de caracteres
    size_t list_length = 0;
    for (int i = 0; i < response.n_users; ++i) {
        list_length += strlen(response.users[i]->username) + 1; // +1 para el separador
    }

    // Asegurarse de incluir espacio para el carácter nulo terminador
    list_length++; 

    char *user_list_str = malloc(list_length);
    strcpy(user_list_str, "");
    for (int i = 0; i < response.n_users; ++i) {
        strcat(user_list_str, response.users[i]->username);
        strcat(user_list_str, ",");
    }

    printf("Preparando respuesta\n");
    Chat__Response server_response = CHAT__RESPONSE__INIT;
    server_response.operation = CHAT__OPERATION__GET_USERS;
    server_response.status_code = CHAT__STATUS_CODE__OK;
    server_response.user_list = &response;
    server_response.result_case = CHAT__RESPONSE__RESULT_USER_LIST;

    size_t response_size = chat__response__get_packed_size(&server_response);
    uint8_t *response_buffer = malloc(response_size);
    chat__response__pack(&server_response, response_buffer);

    printf("Enviando respuesta\n");
    // Enviar la lista de usuarios a través de send_one_message
    send_one_message(user_list_str, strlen(user_list_str), uid);

    // Liberar la memoria asignada para la cadena de usuarios
    free(user_list_str);

    for (size_t i = 0; i < response.n_users; ++i) {
        free(response.users[i]->username);
        free(response.users[i]);
    }
    free(response.users);

    printf("Desbloqueando mutex\n");
    pthread_mutex_unlock(&clients_mutex);
    printf("Mutex desbloqueado\n");
}




void info_user(char *receiver, int uid){
	pthread_mutex_lock(&clients_mutex);

	char list_users[MSG_LIMIT] = {};

	strcat(list_users, "\nUsuarios|Estado|Uid|Sockfd\n");

	char buffer_state[12];

	for(int i = 0; i<LIMIT_CLIENT; ++i){
		if(clients[i]) {
			if(strcmp(clients[i] -> name, receiver) == 0) {
				strcat(list_users, clients[i] -> name);
				strcat(list_users, "|");
				sprintf(buffer_state, "%d", clients[i]->state);
				if (strcmp(buffer_state, "0") == 0){//activo
					strcat(list_users, "activo");
				} else if (strcmp(buffer_state, "1") == 0){ //ocupado
					strcat(list_users, "ocupado");
				} else if (strcmp(buffer_state, "2") == 0) { //inactivo
					strcat(list_users, "inactivo");
				}

				char temp_uid[10];
				char temp_sockfd[10];
				strcat(list_users, "|");
				sprintf(temp_uid, "%d", clients[i]->uid);
				strcat(list_users, temp_uid);
				strcat(list_users, "|");
				sprintf(temp_sockfd, "%d", clients[i]->sockfd);
				strcat(list_users, temp_sockfd);

				strcat(list_users, "\n");
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
	send_one_message(list_users, uid, uid);
}


void handle_get_users_request(client_obj *cli, Chat__UserListRequest *request) {
    //printf("Entra en handle_get_users_request\n");
    //printf("uid: %d\n", cli->uid);
    list_all_users(cli->uid);
}


void handle_send_message_request(client_obj *cli, Chat__SendMessageRequest *request) {
    printf("Entra en handle_send_message_request\n");

    // Verificar si la solicitud es NULL
    if (request == NULL) {
        printf("Error: request es NULL\n");
        return;
    }

    // Verificar si el contenido del mensaje y el destinatario son NULL
    if (request->content == NULL) {
        printf("Error: El contenido del mensaje es NULL\n");
        return;
    }

    if (request->recipient == NULL) {
        printf("Error: El destinatario del mensaje es NULL\n");
        return;
    }

    // Extraer los detalles de la solicitud
    char *message = request->content;
    char *receiver_name = request->recipient;

    // Imprimir los detalles para depuración
    printf("Mensaje: %s\n", message);
    printf("Destinatario: %s\n", receiver_name);

    // Enviar el mensaje privado
    send_private_msg(message, receiver_name);
}



static int uid_counter = 10;

//Manejo de comunicacion con el cliente
void *handle_client(void *arg) {
    char name[32];
    int leave_flag = 0;

    client_obj *cli = (client_obj *)arg;

    // Asignar un UID único al cliente
    cli->uid = uid_counter++;
    printf("Nuevo cliente conectado con UID: %d\n", cli->uid);
    char buff_out[MSG_LIMIT];
    // Name
    if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1) {
        printf("Nombre invalido.\n");
        leave_flag = 1;
    } else {
        for (int i = 0; i < LIMIT_CLIENT; ++i) {
            if (clients[i]) {
                if (strcmp(clients[i]->name, name) == 0) {
                    printf("Usuario ya en uso\n");
                    leave_flag = 1;
                }
            }
        }
        if (leave_flag == 0) {
            strcpy(cli->name, name);
            
            sprintf(buff_out, "%s se ha unido!\n", cli->name);
            printf("%s", buff_out);
            send_message(buff_out, cli->uid);
        }
    }

    uint8_t buffer[LENGTH];
    bzero(buff_out, MSG_LIMIT);

    while (1) {
        if (leave_flag) {
            break;
        }

        int receive1 = recv(cli->sockfd, buff_out, LENGTH, 0);
        if (receive1 > 0) {
        
            
            if (strstr(buff_out, "/info")){ //Comando de mensaje info
					printf("\nSe busca info de usuario\n");
					char buffer_string[MSG_LIMIT + 32];					

					char *token = strtok(buff_out, " ");
					while (token != NULL){
						if (strcmp(token, "/info") !=0) {
							strcat(buffer_string, token);
							strcat(buffer_string, " ");
						}
						token = strtok(NULL , " ");
					}

					trim_newline(buffer_string);
					char *space_pos = strchr(buffer_string, ' ');
					char *receiver = "";
					char *message_buffer = "";


					if(space_pos != NULL) {
						*space_pos = '\0';
						receiver = buffer_string;
						message_buffer = space_pos + 1;
					}

					info_user(receiver, cli->uid);

					memset(buffer_string, '\0', sizeof(buffer_string));
					
		
				}else if (strstr(buff_out, "/priv")){ //Comando de mensaje privado
					printf("\nSe enviara un mensaje privado\n");
					char buffer_string[MSG_LIMIT + 32];					

					char *token = strtok(buff_out, " ");
					while (token != NULL){
						if (strcmp(token, "/priv") !=0) {
							strcat(buffer_string, token);
							strcat(buffer_string, " ");
						}
						token = strtok(NULL , " ");
					}
					strcat(buffer_string, ". De: ");
					strcat(buffer_string, cli->name);
					strcat(buffer_string, " \n");
					trim_newline(buffer_string);

					char *space_pos = strchr(buffer_string, ' ');
					char *receiver = "";
					char *message_buffer = "";


					if(space_pos != NULL) {
						*space_pos = '\0';
						receiver = buffer_string;
						message_buffer = space_pos + 1;
					}

					printf("destinatario: %s\n", receiver);
					printf("mensaje: %s\n", message_buffer);
					strcat(message_buffer, "\n");

					send_private_msg(message_buffer, receiver);


					//clearing buffer_string
					memset(buffer_string, '\0', sizeof(buffer_string));


				}
        }

            int receive = recv(cli->sockfd, buffer, LENGTH, 0);
            if (receive > 0) {

                Chat__Request *request = chat__request__unpack(NULL, receive, buffer);
                if (request == NULL) {
                    fprintf(stderr, "Error unpacking request.\n");
                    continue;
                }
			//printf("Tamaño de request->operation: %zu bytes\n", sizeof(request->operation));
			//printf("Tamaño de CHAT__OPERATION__GET_USERS: %zu bytes\n", sizeof(CHAT__OPERATION__GET_USERS));

				switch (request->operation) {
					case CHAT__OPERATION__GET_USERS:
						handle_get_users_request(cli, request->get_users);
						break;
					case CHAT__OPERATION__SEND_MESSAGE:
                        // Depuración: Verificar contenido del request antes de llamar a la función
                        printf("\nSe enviara un mensaje privado\n");
                        char buffer_string[MSG_LIMIT + 32];					

                        char *token = strtok(buff_out, " ");
                        while (token != NULL){
                            if (strcmp(token, "/priv") !=0) {
                                strcat(buffer_string, token);
                                strcat(buffer_string, " ");
                            }
                            token = strtok(NULL , " ");
                        }
                        strcat(buffer_string, ". De: ");
                        strcat(buffer_string, cli->name);
                        strcat(buffer_string, " \n");
                        trim_newline(buffer_string);

                        char *space_pos = strchr(buffer_string, ' ');
                        char *receiver = "";
                        char *message_buffer = "";


                        if(space_pos != NULL) {
                            *space_pos = '\0';
                            receiver = buffer_string;
                            message_buffer = space_pos + 1;
                        }

                        printf("destinatario: %s\n", receiver);
                        printf("mensaje: %s\n", message_buffer);
                        strcat(message_buffer, "\n");

                        send_private_msg(message_buffer, receiver);


                        //clearing buffer_string
                        memset(buffer_string, '\0', sizeof(buffer_string));
                        break;
					//case CHAT__OPERATION__UPDATE_STATUS:
					//	handle_update_status_request(cli, request->update_status);
					//	break;
					// Añade más operaciones aquí
					default:
						fprintf(stderr, "Unknown operation.\n");
						break;
				}

            chat__request__free_unpacked(request, NULL);
        } else if (receive == 0 || (receive > 0 && strcmp((char *)buffer, "/exit") == 0)) {
            sprintf(buffer, "%s se ha pirado.\n", cli->name);
            printf("%s", buffer);
            send_message((char *)buffer, cli->uid);
            leave_flag = 1;
        } else {
            printf("ERROR: -1\n");
            leave_flag = 1;
        }

        memset(buffer, 0, LENGTH);
    }

    // Liberar usuario y thread
    close(cli->sockfd);
    free_user(cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

    return NULL;
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Uso: %s <puerto>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);
	int option = 1;
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	pthread_t tid;

  // Settings del socket
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(ip);
  serv_addr.sin_port = htons(port);

	signal(SIGPIPE, SIG_IGN);

	if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
		perror("ERROR: setsockopt fallido");
    return EXIT_FAILURE;
	}

  if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR: Bindeo del socket fallido");
    return EXIT_FAILURE;
  }

  if (listen(listenfd, 10) < 0) {
    perror("ERROR: Fallo al escuchar al socket");
    return EXIT_FAILURE;
	}

	printf("\n");
	printf("+-----------------------+\n");
	printf("|   ¡Entrando al chat!  |\n");
	printf("+-----------------------+\n");
	printf("\n");

	while(1){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		//Chequeo de maximo de clientes
		if((cli_count + 1) == LIMIT_CLIENT){
			printf("Capacidad del clientes excedida ");
			print_client_addr(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}

		// Ajustes del cliente
		client_obj *cli = (client_obj *)malloc(sizeof(client_obj));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;
		cli->state = 0;

		//Añadir cliente a la fila
		register_user(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		sleep(1);
	}

	return EXIT_SUCCESS;
}