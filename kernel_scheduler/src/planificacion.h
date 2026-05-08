#ifndef PLANIFICACION_H_
#define PLANIFICACION_H_

#include "kernel_scheduler.h"
#include "globales.h"

typedef enum {  
    FIFO,
    RR,
    CMN,
    INVALIDO
} t_algoritmo_planificacion;

void *handle_planificacion(void *arg);
void planificacion_fifo();
t_cpu* buscar_cpu_libre();
t_algoritmo_planificacion string_to_algoritmo(const char* algoritmo);
void enviar_proceso_a_cpu(int socket_cpu, int pid_proceso, int pc, int prioridad);

#endif /* PLANIFICACION_H_ */