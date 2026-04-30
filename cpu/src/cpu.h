#include <commons/log.h>
#include <commons/config.h>
#include <pthread.h>
#include "utils/serializacion.h"

#define MAX_BUFFER_INSTRUCCION 128

t_log* log_cpu;

int conexion_ms;
int conexion_ks;
int conexion_km;

struct cpu {
int pid;
uint32_t pc;
uint8_t ax, bx, cx, dx;
uint32_t eax;
uint32_t ebx;
uint32_t ecx;
uint32_t edx;
uint32_t si;
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
    char** params;
} t_instruccion;

t_instruccion instruccion_actual;

static char instruccion_buffer[128];

void* handle_ks(void* arg);