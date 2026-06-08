#ifndef KERNEL_SCHEDULER_H_
#define KERNEL_SCHEDULER_H_

#include <utils/serializacion.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
#include "globales.h"
typedef enum {  
    NEW,
    READY,
    BLOCK,
    EXEC,
    EXIT,
    SUSP_READY,
    SUSP_BLOCK 
} t_estado_proceso;

typedef struct t_proceso_pcb {
    pid_t pid;
    int prioridad;
    int pc;
    int quantum;
    t_estado_proceso estado;
    char* nombre_mutex;
} t_proceso_pcb;

void* handle_connections(void* arg);
void* handle_km(void* arg);
void *handle_cpu(void *arg);
void *handle_io(void *arg);
void *handle_planificacion(void *arg);
//void* temporizador_timeout(void* arg);
void inicializar_ks(int argc, char* argv[]);
void enviar_proceso(int socket_km, pid_t pid, char* path_proceso);
void crear_proceso_inicial(int socket_km, char* path_proceso_inicial);
void finalizar_io(pid_t pid);
void cambiar_estado_proceso(t_proceso_pcb *proceso, t_estado_proceso estado_siguiente);
void liberar_recursos();
const char* estado_to_string(t_estado_proceso estado);
char* solicitar_contenido_a_km(int socket_km, pid_t pid, int direccion_logica, int tamanio);
t_proceso_pcb* crear_pcb(pid_t pid, int prioridad);
t_proceso_pcb* remover_proceso_por_pid(t_list* cola, pid_t pid);



#endif /* KERNEL_SCHEDULER_H_ */
