#include <commons/log.h>
#include <commons/config.h>
#include "utils/serializacion.h"

#define MAX_BUFFER_INSTRUCCION 128
#define MAX_INSTRUCTION_PARAMS 5
#define MAX_INSTRUCTION_PARAM_SIZE 128

t_log* log_cpu;

int conexion_ms;
int conexion_ks;
int conexion_km;

//Variables de configuracion:
char* env_str_log_level;
char* env_ip_ks;
char* env_puerto_ks;
char* env_ip_km;
char* env_puerto_km;
char* env_ip_ms;
char* env_puerto_ms;

struct cpu {
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
} cpu;

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

typedef struct {
    InstrucionCode icode;
    char params[MAX_INSTRUCTION_PARAMS][MAX_INSTRUCTION_PARAM_SIZE];
    int cant_params;
} t_instruccion;

t_instruccion instruccion_actual;

static char instruccion_buffer[128];
void * string_to_registro(char* s);
void* handle_ks(void* arg);
InstrucionCode string_to_instruction_code(char* s);

void* fetch();
void decode();
void* execute();
void* check_interrupt();

int conectar_modulo_logueando(char* ip, char* puerto, char* nombre_modulo);
int map_config_variables(t_config* config);

void ejecutar_noop();
void ejecutar_set(char* registro, char* valor_set);
void ejecutar_sum(char* registro_destino, char* registro_origen);
void ejecutar_sub(char* registro_destino, char* registro_origen);
void ejecutar_jnz(char* registro, char* str_nuevo_valor_pc);
void ejecutar_mov_in(char* registro);
void ejecutar_mov_out(char* registro);
void ejecutar_copy_mem(char* registro_tamanio);