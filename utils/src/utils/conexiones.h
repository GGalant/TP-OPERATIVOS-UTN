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
    CONECTAR_MS,
    PEDIDO_INSTRUCCION,
    RTA_PEDIDO_INSTRUCCION,
    STDOUT_LECTURA,
    STDIN_ESCRITURA,
    CREACION_DE_PROCESO,
    SOLICITUD_TRADUCCION
}op_code;

/* typedef enum {
    INICIALIZAR_PROCESO,
    INICIALIZAR_PROCESO_OK
} op_code_ks_km; */
 
typedef enum {  
    EJECUTAR_PROCESO,
    SYSCALL_EXIT,
    SYSCALL_SLEEP,
    SYSCALL_STDIN,
    SYSCALL_STDOUT,
    INTERRUPCION_FIN_QUANTUM,
    INTERRUPCION_DESALOJO,
    SYSCALL_MUTEX_LOCK,
    SYSCALL_MUTEX_UNLOCK,
    SYSCALL_INIT_PROC
} op_code_ks_cpu;

typedef enum {
    CREAR_IO,
    SOLICITUD_IO_SLEEP,
    FIN_IO
} op_code_ks_io;

typedef enum {
    SOLICITAR_CONTENIDO_MEMORIA,
    RESPUESTA_CONTENIDO_MEMORIA
} op_code_ks_km;

int crear_conexion(char *ip, char* puerto);
int iniciar_servidor(t_log *logger, char *puerto);
int esperar_cliente(int socket_servidor, t_log* logger);
    