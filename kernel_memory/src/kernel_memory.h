#include <commons/log.h>
#include <commons/config.h>
#include "utils/serializacion.h"
#include <pthread.h>

int contador_sticks;
int posicion_memoria;
t_log* log_km;
int server_km;
int socket_memory_stick;
int socket_swap;
int socket_cpu;

void* handle_connections(void* arg);
void* handle_ks(void* arg);
void* handle_cpu(void* arg);
void* handle_ms(void* arg);

t_list* cargar_instrucciones(char* path_instrucciones);
char* obtener_instruccion(t_list* instrucciones, uint32_t pc_actual);



typedef struct {
    int id_segmento;
    uint32_t base;
    uint32_t limite;
} t_segmento; // revisar 


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

    t_list* tabla_segmentos;

}t_contextoEjec;

//guarda los contextos de ejecucion por pid (registros)
t_dictionary* diccionario_contextos_ejecucion; //se crea al iniciar el servidor de km, antes de los hilos.

//guarda los paths de los archivos de instrucciones por pid
t_dictionary* diccionario_paths; 

typedef struct{
    uint32_t base;
    uint32_t tamanio;
}t_hueco_libre;

t_list* huecos;

t_list* sticks_conectados;

typedef struct {
    int id;
    uint32_t tamanio;
    uint32_t inicio;   // dentro del espacio total
} t_memory_stick;

