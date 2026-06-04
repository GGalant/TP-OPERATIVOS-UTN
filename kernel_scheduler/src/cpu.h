#ifndef CPU_H_
#define CPU_H_

typedef enum {    
    CPU_LIBRE,
    CPU_OCUPADA
} t_estado_cpu;

typedef struct t_cpu {  
    int socket_cpu;
    int cpu_id;
    int pid_actual;
    t_estado_cpu estado;
} t_cpu;

void* handle_cpu(void* arg);
const char* operacion_to_string(op_code_ks_cpu operacion);
int recibir_cpu_id(int socket);
t_cpu *crear_cpu(int socket_cpu);

#endif /* CPU_H_ */