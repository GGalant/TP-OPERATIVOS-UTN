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
    // ======================================
    //* INICIO: enums generales:
    
    ERROR = -1,
    MENSAJE,
    PAQUETE,

    //* FIN: enums generales
    // ======================================
    //* INICIO: HANDSHAKES
    
    HANDSHAKE_SWAP,
    HANDSHAKE_KM,
    HANDSHAKE_KS,
    HANDSHAKE_MS,
    HANDSHAKE_CPU,
    HANDSHAKE_IO,
    HANDSHAKE_MEMORYSTICK,

    //* FIN: HANDSHAKES
    // ======================================
    //--------------------------------------
    // ======================================
    //*INICIO: KERNEL MEMORY -CPU

    ENVIAR_CONTEXTO,
    RECIBIR_CONTEXTO,
    PEDIDO_INSTRUCCION,
    RTA_PEDIDO_INSTRUCCION,

    //*FIN: KERNEL MEMORY - CPU
    // ======================================

    // ======================================
    //* INICIO: KERNEL MEMORY - MEMORY STICK

    CONECTAR_MS,
    DESCONECTAR_MS,
    ESCRITURA_OK,
    LECTURA_OK,
    SOLICITUD_LECTURA,
    SOLICITUD_ESCRITURA,

    //* FIN: KERNEL MEMORY - MEMORY STICK
    // ======================================

    // ======================================
    //* INICIO: KERNEL MEMORY - SWAP

    ESCRITURA_DE_BLOQUE,
    LECTURA_DE_BLOQUE,
    //* FIN: KERNEL MEMORY - SWAP
    // ======================================

    // ======================================
    //* INICIO: KERNEL SCHEDULER - CPU:

    EJECUTAR_PROCESO,
    SYSCALL_EXIT,
    SYSCALL_SLEEP,
    SYSCALL_STDIN,
    SYSCALL_STDOUT,
    INTERRUPCION_FIN_QUANTUM,
    INTERRUPCION_DESALOJO,
    SYSCALL_MUTEX_LOCK,
    SYSCALL_MUTEX_UNLOCK,
    SYSCALL_INIT_PROC,
    ENVIO_PID,
    SYSCALL_MUTEX_CREATE,
    SYSCALL_MEM_ALLOC,
    SYSCALL_MEM_FREE,

    //* FIN: KERNEL SCHEDULER - CPU
    // ======================================

    // ======================================
    //* INICIO: KERNEL SCHEDULER - IO

    CREAR_IO,
    SOLICITUD_IO_SLEEP,
    SOLICITUD_IO_STDOUT,
    FIN_IO,

    //* FIN: KERNEL SCHEDULER - IO
    // ======================================
    //* INICIO: KERNEL SCHEDULER - KERNEL MEMORY

    SOLICITAR_CONTENIDO_MEMORIA,
    RESPUESTA_CONTENIDO_MEMORIA,
    INICIALIZAR_PROCESO,
    INICIALIZAR_PROCESO_OK,
    STDOUT_LECTURA,
    STDIN_ESCRITURA,
    CREACION_DE_PROCESO,
    CREACION_DE_SEGMENTO,
    DESALOJAR_CPU,
    DESALOJAR_CPU_OK

    //* FIN: KERNEL SCHEDULER - KERNEL MEMORY
    // ======================================
    

}op_code;

int crear_conexion(char *ip, char* puerto);
int iniciar_servidor(t_log *logger, char *puerto);
int esperar_cliente(int socket_servidor, t_log* logger);
    