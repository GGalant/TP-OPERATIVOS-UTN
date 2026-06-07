#include <commons/log.h>
#include <commons/config.h>
#include "utils/serializacion.h"

#define MAX_BUFFER_INSTRUCCION 128
#define MAX_INSTRUCTION_PARAMS 5
#define MAX_INSTRUCTION_PARAM_SIZE 128

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

//* las unicas variables definidas en el header las defino como extern para evitar dobles definiciones si el .h se incluye en 2 archivos
//* estan aqui porque se usan en mas de un .c, y el header corresponde a mas de un .c
extern t_cpu cpu; 
extern bool interrupcion_activa;
extern t_log* log_cpu;
extern t_instruccion instruccion_actual;
extern int conexion_ks;
extern int conexion_km;
extern int conexion_ms;

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



int get_registro_size(char* s);
uint32_t get_registro_value(size_t registro_size, void* registro);
void set_reg_value(size_t reg_size, void* registro, uint32_t valor);

static char instruccion_buffer[128];
void * string_to_registro(char* s);
void* handle_ks(void* arg);
InstrucionCode string_to_instruction_code(char* s);

void* fetch();
void decode();
void* execute();
bool check_interrupt();

int conectar_modulo_logueando(char* ip, char* puerto, char* nombre_modulo);
int map_config_variables(t_config* config, t_cpu_env* env);
int pull_contexto();
void push_contexto();
void integrar_contexto(t_list* paquete_contexto);
t_paquete *empaquetar_contexto();

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