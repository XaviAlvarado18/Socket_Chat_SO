# Chat Application

## Descripción

Esta es una aplicación de chat simple basada en sockets TCP, implementada en C. Permite a los usuarios conectarse a un servidor de chat, enviar mensajes públicos y privados, y consultar información de otros usuarios.

## Características

- Registro de usuarios con nombre único.
- Envío de mensajes públicos a todos los usuarios conectados.
- Envío de mensajes privados a usuarios específicos.
- Consulta de la lista de usuarios conectados y sus estados.
- Actualización del estado del usuario (en línea, ocupado, fuera de línea).

## Protocolo

El protocolo de comunicación entre el cliente y el servidor está basado en Google Protocol Buffers (protobuf). Los mensajes se serializan y deserializan utilizando protobuf para garantizar una comunicación eficiente y estructurada.

### Definición del Protocolo

```protobuf
syntax = "proto3";

package chat;

enum UserStatus {
    ONLINE = 0;
    BUSY = 1;
    OFFLINE = 2;
}

message User {
    string username = 1;
    UserStatus status = 2;
}

message NewUserRequest {
    string username = 1;
}

message SendMessageRequest {
    string recipient = 1;
    string content = 2;
}

enum MessageType {
    BROADCAST = 0;
    DIRECT = 1;
}

message IncomingMessageResponse {
    string sender = 1;
    string content = 2;
    MessageType type = 3;
}

enum UserListType {
    ALL = 0;
    SINGLE = 1;
}

message UserListRequest {
    string username = 1;
}

message UserListResponse {
    repeated User users = 1;
    UserListType type = 2;
}

message UpdateStatusRequest {
    string username = 1;
    UserStatus new_status = 2;
}

enum Operation {
    REGISTER_USER = 0;
    SEND_MESSAGE = 1;
    UPDATE_STATUS = 2;
    GET_USERS = 3;
    UNREGISTER_USER = 4;
    INCOMING_MESSAGE = 5;
}

message Request {
    Operation operation = 1;
    oneof payload {
        NewUserRequest register_user = 2;
        SendMessageRequest send_message = 3;
        UpdateStatusRequest update_status = 4;
        UserListRequest get_users = 5;
        User unregister_user = 6;
    }
}

enum StatusCode { 
    UNKNOWN_STATUS = 0;
    OK = 200;
    BAD_REQUEST = 400;
    INTERNAL_SERVER_ERROR = 500;
}

message Response {
    Operation operation = 1;
    StatusCode status_code = 2;
    string message = 3;
    oneof result {
        UserListResponse user_list = 4;
        IncomingMessageResponse incoming_message = 5;
    }
}
```

## Instalación

### Requisitos Previos

- GCC (GNU Compiler Collection)
- make
- [Protocol Buffers Compiler (protoc)](https://grpc.io/docs/protoc-installation/)
- Bibliotecas de Protocol Buffers para C

### Compilación

1. Clona este repositorio:
    ```bash
    git clone https://github.com/tu-usuario/chat-application.git
    cd chat-application
    ```

2. Compila el proyecto:
    ```bash
    make
    ```

## Uso

### Ejecutar el Servidor

Para iniciar el servidor de chat:
```bash
./server
```

## Comandos de Cliente

- **Registro de Usuario**: Al iniciar el cliente, se te pedirá un nombre de usuario único.
- **Mensajes Públicos**: Simplemente escribe tu mensaje y presiona Enter para enviarlo a todos los usuarios conectados.
- **Mensajes Privados**: Usa el comando `/priv <destinatario> <mensaje>` para enviar un mensaje privado.
- **Consultar Información de Usuario**: Usa el comando `/info <usuario>` para obtener información sobre un usuario específico.
- **Salir**: Usa el comando `/exit` para salir del chat.

