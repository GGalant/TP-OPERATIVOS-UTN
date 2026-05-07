#ifndef KERNEL_SCHEDULER_H_
#define KERNEL_SCHEDULER_H_

#include <utils/serializacion.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
typedef enum {  
    NEW,
    READY,
    BLOCK,
    EXEC,
    EXIT,
    SUSP_READY,
    SUSP_BLOCK 
} t_estado_proceso;

typedef enum {    
    CPU_LIBRE,
    CPU_OCUPADA
} t_estado_cpu;

typedef struct {    
    char* nombre_mutex;
    int estado; // 0 "libre" o 1 "ocupado"
    int pid_proceso;
    t_list* cola_procesos; 
} t_mutex;

// mutex para los procesos simulados (t_mutex)
// mutex para los hilos 
typedef struct {
    int pid;
    int prioridad;
    int pc;
    int quantum;
    t_estado_proceso estado;
    char* nombre_mutex;
} t_proceso_pcb;

typedef struct {  
    int socket_cpu;
    int cpu_id;
    int pid_actual;
    t_estado_cpu estado;
} t_cpu;

void inicializar_ks(int argc, char* argv[]);
void* handle_connections(void* arg);
void* handle_km(void* arg);
void* handle_cpu(void* arg);
void* handle_io(void* arg);
void enviar_proceso(int socket_km, int pid, char* path_proceso);
void crear_proceso_inicial(int socket_km, char* path_proceso_inicial);
t_proceso_pcb* crear_pcb(int pid, int prioridad);
int recibir_cpu_id(int socket);

#endif /* KERNEL_SCHEDULER_H_ */