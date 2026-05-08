#ifndef KERNEL_SCHEDULER_H_
#define KERNEL_SCHEDULER_H_

#include <utils/serializacion.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
#include "globales.h"
#include "cpu.h"
#include "planificacion.h"
typedef enum {  
    NEW,
    READY,
    BLOCK,
    EXEC,
    EXIT,
    SUSP_READY,
    SUSP_BLOCK 
} t_estado_proceso;

typedef struct {    
    char* nombre_mutex;
    int estado; // 0 "libre" o 1 "ocupado"
    int pid_proceso;
    t_list* cola_procesos; 
} t_mutex;

typedef struct {
    int pid;
    int prioridad;
    int pc;
    int quantum;
    t_estado_proceso estado;
    char* nombre_mutex;
} t_proceso_pcb;

void inicializar_ks(int argc, char* argv[]);
void* handle_connections(void* arg);
void* handle_km(void* arg);
void* handle_io(void* arg);
void enviar_proceso(int socket_km, int pid, char* path_proceso);
void crear_proceso_inicial(int socket_km, char* path_proceso_inicial);
t_proceso_pcb* crear_pcb(int pid, int prioridad);
const char* estado_to_string(t_estado_proceso estado);
void cambiar_estado_proceso(t_proceso_pcb *proceso, t_estado_proceso estado_siguiente);
void liberar_recursos();

#endif /* KERNEL_SCHEDULER_H_ */
