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
    INSTRUCTION_ERROR = -1,

} InstrucionCode;

//region: STRUCTS

typedef struct {
//Process Id
pid_t pid;
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
//Tabla de segmentos
t_list* tabla_segmentos;
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

//* las unicas variables declaradas en el header las declaron como extern para evitar dobles definiciones si el .h se incluye en 2 archivos
//* estan aqui porque se usan en mas de un .c, y el header corresponde a mas de un .c
extern t_cpu cpu; 
extern bool se_ejecuto_syscall;
extern t_log* log_cpu;
extern t_instruccion instruccion_actual;
extern int conexion_ks;
extern int conexion_km;
extern int conexion_ms;
extern int id_cpu;

//region: DECLARACIONES DE FUNCIONES

int inicializacion_y_pasos_iniciales(int argc, char* argv[]);

/* 
* Con el log_level crea un log y lo devuelve.
* El invocador es responsable de destruirlo con log_destroy(log)
*/
t_log inicializar_log(int log_level);

/*
* Devuelve un puntero al registro de la CPU que coincida con la string o NULL en caso default.
*/
void * string_to_registro(char* s);

void* handle_ks(void* arg);

/* 
* Limpia el buffer de instruccion y el struct de instruccion
*/
void flush_buffers();

/* 
* Limpia el contexto.
* Incluyendo la tabla de segmentos a null.
*/
void limpiar_contexto();

/* 
* Convierte string a enum de instruccion, default es INSTRUCTION_ERROR
*/
InstrucionCode string_to_instruction_code(char* s);

/*
* Envia un pedido de instruccion a Kernel Memory y la carga en el buffer de instruccion del CPU
* Devuelve NULL ante error, o el buffer de instruccion (instruccion_buffer) ante ejecucion exitosa.
*/
void* fetch();

/*
* Lee del Buffer Estatico de Instruccion (instruccion_buffer) y escribe la instruccion decodificada en el struct estatico instruccion_actual
* el icode de la instruccion puede ser ERROR.
*/
void decode();
void execute();
bool check_interrupt();

pid_t recibir_pid_de_ks();
/* 
* Espera un tamanio y un string de Kernel Memory y lo carga en el buffer de instruccion.
* Pertenece al Fetch.
*/
void recibir_instruccion_de_km();
/* 
* Conecta con el modulo indicado y devuelve el file descriptor.
* Ademas, loguea el resultado de la conexion.
*/
int conectar_modulo(char* ip, char* puerto, char* nombre_modulo);

//* Mapea las variables del config a al struct de cpu pasado
int map_config_variables(t_config* config, t_cpu_env* env);

/*
 * Solicita el contexto del proceso actual a Kernel Memoty y lo integra en la CPU.
 * Retorna 0 (éxito) o -1 (error);
 */
int pull_contexto(pid_t pid_a_pedir);

/*
 * Empaqueta el contexto de ejecución actual de la CPU y lo envía a Kernel Memory.
 */
void push_contexto();

/* 
* Toma un paquete e integra todo su contenido al contexto de la cpu. 
* Incluye la tabla de segmentos
*/
void integrar_contexto(t_list* paquete_contexto);

/* 
* Toma los registros de la cpu y los empaqueta listo para enviar
* El invocador debera encargarse de invocar eliminar_paquete(t_paquete paquete);
*/
t_paquete *empaquetar_contexto();

void enviar_pid_actual_a_KS();

// utils de instrucciones
int get_registro_size(char* s);
uint32_t get_registro_value(size_t registro_size, void* registro);
void set_reg_value(size_t reg_size, void* registro, uint32_t valor);

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