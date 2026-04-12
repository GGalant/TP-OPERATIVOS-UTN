#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<commons/log.h>
#include<commons/config.h>
#include<errno.h>

typedef enum {  
    HANDSHAKE_SWAP,
    HANDSHAKE_KM,
    HANDSHAKE_KS,
    HANDSHAKE_CPU,
    HANDSHAKE_IO,
    HANDSHAKE_MEMORYSTICK
}handshake;

typedef enum {  

}op_code;

int crear_conexion(char *ip, char* puerto);
int iniciar_servidor(t_log *logger, char *puerto);
int esperar_cliente(int socket_servidor, t_log* logger);