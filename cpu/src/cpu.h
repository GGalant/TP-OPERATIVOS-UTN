#include <commons/log.h>
#include <commons/config.h>
#include "utils/serializacion.h"

#define MAX_BUFFER_INSTRUCCION 128
#define MAX_INSTRUCTION_PARAMS 5
#define MAX_INSTRUCTION_PARAM_SIZE 128

//region: ENUMS

typedef enum {
    //OPERACIONES ORDINARIAS:
    NOOP,
    SET,
    MOV_IN,
    MOV_OUT,
    SUM,
    SUB,
    JNZ,
    COPY_MEM,
    //SYSCALLS:
    MUTEX_CREATE,
    MUTEX_LOCK,
    MUTEX_UNLOCK,
    MEM_ALLOC,
    MEM_FREE,
    SLEEP,
    STDOUT,
    STDIN,
    INIT_PROC,
    EXIT,
    //Creada por nosotros
    ERROR = -1,

} InstrucionCode;

typedef enum {
    ENVIO_PID
} op_code_cpu_ks; //TODO mover a conexiones, coordinar con KS
typedef enum { //TODO MOVER A CONEXIONES, coordinar con KS
    ENCADENADOR_DE_ENUMS = SYSCALL_INIT_PROC, 
    SYSCALL_MUTEX_CREATE,
    SYSCALL_MEM_ALLOC,
    SYSCALL_MEM_FREE
} ExtendedOperations; 

//region: STRUCTS

typedef struct {
//Process Id
int pid;
//Program Counter
uint32_t pc;
//Registro Numerico de propósito general
uint8_t ax, bx, cx, dx;
//Registro Numérico de propósito general
uint32_t eax, ebx, ecx, edx;
//Direccion logica de memoria de origen
uint32_t si;
//Direccion logica de memoria de destino
uint32_t di;
} t_cpu;

typedef struct {
    InstrucionCode icode;
    char params[MAX_INSTRUCTION_PARAMS][MAX_INSTRUCTION_PARAM_SIZE];
    int cant_params;
} t_instruccion;

typedef struct {
    char* str_log_level;
    char* ip_ks;
    char* puerto_ks;
    char* ip_km;
    char* puerto_km;
    char* ip_ms;
    char* puerto_ms;
} t_cpu_env;

//region: VARIABLES GLOBALES (finiquitar)

//* las unicas variables definidas en el header las defino como extern para evitar dobles definiciones si el .h se incluye en 2 archivos
//* estan aqui porque se usan en mas de un .c, y el header corresponde a mas de un .c
extern t_cpu cpu; 
extern bool se_ejecuto_syscall;
extern t_log* log_cpu;
extern t_instruccion instruccion_actual;
extern int conexion_ks;
extern int conexion_km;
extern int conexion_ms;

//region: DECLARACIONES DE FUNCIONES

int get_registro_size(char* s);
uint32_t get_registro_value(size_t registro_size, void* registro);
void set_reg_value(size_t reg_size, void* registro, uint32_t valor);

void * string_to_registro(char* s);
void* handle_ks(void* arg);
InstrucionCode string_to_instruction_code(char* s);

void* fetch();
void decode();
void execute();
bool check_interrupt();

int conectar_modulo_logueando(char* ip, char* puerto, char* nombre_modulo);
int map_config_variables(t_config* config, t_cpu_env* env);

/*
 * Solicita el contexto del proceso actual a Kernel Memoty y lo integra en la CPU.
 * Retorna 0 (éxito) o -1 (error);
 */
int pull_contexto();

/*
 * Empaqueta el contexto de ejecución actual de la CPU y lo envía a Kernel Memory.
 */
void push_contexto();
void integrar_contexto(t_list* paquete_contexto);
t_paquete *empaquetar_contexto();
void enviar_pid_actual_a_KS();

//INSTRUCCIONES ORDINARIAS:

void ejecutar_noop();
void ejecutar_set(char* registro, char* valor_set);
void ejecutar_sum(char* registro_destino, char* registro_origen);
void ejecutar_sub(char* registro_destino, char* registro_origen);
void ejecutar_jnz(char* registro, char* str_nuevo_valor_pc);
void ejecutar_mov_in(char* registro);
void ejecutar_mov_out(char* registro);
void ejecutar_copy_mem(char* registro_tamanio);

//INSTRUCCIONES SYSCALLS:

void ejecutar_mutex_create(char* nombre_mutex);
void ejecutar_mutex_lock(char* nombre_mutex);
void ejecutar_mutex_unlock(char* nombre_mutex);
void ejecutar_mem_alloc(char* id_segmento, char* tamanio);
void ejecutar_mem_free(char* id_segmento);
void ejecutar_sleep(char* parametro_tiempo);
void ejecutar_stdout(char* reg_direccion, char* reg_tamanio);
void ejecutar_stdin(char* reg_direccion_logica, char* reg_tamanio);
void ejecutar_init_proc(char* path_instrucciones, char* prioridad);
void ejecutar_exit();