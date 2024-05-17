# Compilar los archivos de Protobuf-C
PROTO_FILES = chat.proto
PROTO_SRC = $(PROTO_FILES:.proto=.pb-c.c)
PROTO_HDR = $(PROTO_FILES:.proto=.pb-c.h)

# Compilar los archivos de tu proyecto
CLIENT_SRC = client.c $(PROTO_SRC)
SERVER_SRC = server.c $(PROTO_SRC)

# Objetos generados
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
SERVER_OBJ = $(SERVER_SRC:.c=.o)

# Compilador y opciones
CC = gcc
CFLAGS = -Wall -g3 -fsanitize=address -pthread
LDFLAGS = -lprotobuf-c -lpthread

# Directorios
INCLUDE_DIRS = -I/usr/local/include -I.

# Objetivos
all: client server

# Reglas para compilar los archivos cliente y servidor
client: $(CLIENT_OBJ)
	$(CC) $(CFLAGS) $(CLIENT_OBJ) -o $@ $(LDFLAGS)

server: $(SERVER_OBJ)
	$(CC) $(CFLAGS) $(SERVER_OBJ) -o $@ $(LDFLAGS)

# Regla para generar los archivos .pb-c.c y .pb-c.h
%.pb-c.c %.pb-c.h: %.proto
	protoc-c --c_out=. $<

# Reglas para compilar los archivos .o
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

# Limpieza
clean:
	rm -f client server $(CLIENT_OBJ) $(SERVER_OBJ) $(PROTO_SRC) $(PROTO_HDR)
