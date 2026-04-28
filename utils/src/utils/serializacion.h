#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<commons/log.h>
#include<commons/config.h>
#include<commons/collections/list.h>
#include "utils/conexiones.h"


#include<errno.h>
typedef struct{
    int size;
    void* stream;
} t_buffer;
typedef struct{
    op_code codigo_operacion;
    t_buffer* buffer;
} t_paquete;


void enviar_mensaje(char* mensaje, int socket_cliente);
void* serializar_paquete(t_paquete* paquete, int bytes);
void eliminar_paquete(t_paquete* paquete);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
t_paquete* crear_paquete(op_code codigo_operacion);
void crear_buffer(t_paquete* paquete);
void enviar_paquete_send_all(t_paquete* paquete, int socket_cliente);
int recibir_operacion(int socket_cliente);
void recibir_mensaje(int socket_cliente, t_log *logger);
//static int send_all(int fd, const void *buf, size_t len);
void *recibir_buffer(int *size, int socket_cliente);
t_list *recibir_paquete(int socket_cliente);