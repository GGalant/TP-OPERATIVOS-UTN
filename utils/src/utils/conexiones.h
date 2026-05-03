#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<commons/log.h>
#include<commons/config.h>
#include<errno.h>
#include<netdb.h>
#include<string.h>
#include<unistd.h>
#include<commons/string.h>


typedef enum {  
    HANDSHAKE_SWAP,
    HANDSHAKE_KM,
    HANDSHAKE_KS,
    HANDSHAKE_MS,
    HANDSHAKE_CPU,
    HANDSHAKE_IO,
    HANDSHAKE_MEMORYSTICK,
}handshake;

        
typedef enum {
    MENSAJE,
    PAQUETE,
    ENVIAR_CONTEXTO, // de kernel memory, para recibir de cpu
    RECIBIR_CONTEXTO, // de kernel memory, para recibir de cpu
    INICIALIZAR_PROCESO,
    INICIALIZAR_PROCESO_OK,
    CONECTAR_MS
}op_code;

int crear_conexion(char *ip, char* puerto);
int iniciar_servidor(t_log *logger, char *puerto);
int esperar_cliente(int socket_servidor, t_log* logger);
    