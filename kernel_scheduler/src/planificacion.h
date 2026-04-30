#include "kernel_scheduler.h"

typedef enum {  
    FIFO,
    RR,
    COLAS_MULTINIVEL
} t_algoritmo_planificacion;

t_cpu* buscar_cpu_libre();