#include <commons/log.h>
#include <commons/config.h>
#include "utils/serializacion.h"
#include <pthread.h>

t_log* log_km;
int server_km;
int socket_memory_stick;
int socket_swap;

void* handle_connections(void* arg);
void* handle_ks(void* arg);
void* handle_cpu(void* arg);

typedef struct {
    uint32_t base;
    uint32_t limite;
} t_segmento; // revisar 

typedef struct {
    //comentamos el pid de esta estructura porque fue agregado en t_contextoEjec
    //int pid;
    t_list* segmentos; // lista de t_segmento
} t_tabla_segmentos;


typedef struct {
    int pid;
    uint32_t pc;
    uint8_t ax;
    uint8_t bx;
    uint8_t cx;
    uint8_t dx;
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t si;
    uint32_t di;

    t_tabla_segmentos* tabla_segmentos;

}t_contextoEjec;

/* typedef struct{
    int pid;
    t_contextoEjec contextoDeEjecucion;
}t_proceso; */

t_dictionary* diccionario_por_pid; //se crea al iniciar el servidor de km, antes de los hilos