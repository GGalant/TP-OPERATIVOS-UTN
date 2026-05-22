#include "kernel_scheduler.h"

typedef enum {    
    CPU_LIBRE,
    CPU_OCUPADA
} t_estado_cpu;

typedef struct {  
    int socket_cpu;
    int cpu_id;
    int pid_actual;
    t_estado_cpu estado;
} t_cpu;

void* handle_cpu(void* arg);
const char* operacion_to_string(op_code_ks_cpu operacion);
int recibir_cpu_id(int socket);