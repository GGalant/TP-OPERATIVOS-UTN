#ifndef CPU_H_
#define CPU_H_

typedef enum {    
    CPU_LIBRE,
    CPU_OCUPADA
} t_estado_cpu;

typedef struct t_cpu {  
    int socket_cpu;
    int cpu_id;
    pid_t pid_actual;
    t_estado_cpu estado;
} t_cpu;

void* handle_cpu(void* arg);
void enviar_proceso_a_io_sleep(pid_t pid, int tiempo_sleep);
void enviar_proceso_a_io_stdout(pid_t pid, char* contenido);
void iniciar_temporizador_timeout(pid_t pid);
const char* operacion_to_string(op_code operacion);
int recibir_cpu_id(int socket);
int generar_pid();
t_cpu *crear_cpu(int socket_cpu, int cpu_id);
t_mutex* buscar_mutex(const char* nombre_mutex);
t_mutex* crear_mutex(char* nombre_mutex);


#endif /* CPU_H_ */