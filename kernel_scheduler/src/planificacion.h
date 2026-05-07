#include "kernel_scheduler.h"
#include "globales.h"

typedef enum {  
    FIFO,
    RR,
    COLAS_MULTINIVEL
} t_algoritmo_planificacion;

void planificacion_fifo();
t_cpu* buscar_cpu_libre();