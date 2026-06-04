#ifndef PLANIFICACION_H_
#define PLANIFICACION_H_
#include <commons/collections/list.h>

// Forward declarations to avoid circular includes
typedef struct t_cpu t_cpu;
typedef struct t_proceso_pcb t_proceso_pcb;

typedef enum {  
    FIFO,
    RR,
    CMN,
    INVALIDO
} t_algoritmo_planificacion;

void *handle_planificacion(void *arg);
void planificacion_fifo();
void planificacion_rr();
void iniciar_quantum(t_cpu* cpu, int pid);
void* controlar_quantum(void* arg);
t_proceso_pcb* remover_proceso_por_pid(t_list* cola, int pid);

t_cpu* buscar_cpu_libre();
t_algoritmo_planificacion string_to_algoritmo(const char* algoritmo);
void enviar_proceso_a_cpu(int socket_cpu, int pid_proceso, int pc, int prioridad);

// No-op placeholder to ensure file exists
#endif /* PLANIFICACION_H_ */