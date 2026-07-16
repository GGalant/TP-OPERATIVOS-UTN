#include <commons/log.h>
#include <commons/config.h>
#include "utils/serializacion.h"
#include <pthread.h>
#include <commons/bitarray.h>




// GLOBALES

int contador_sticks;
int posicion_memoria;
t_log* log_km;
int server_km;
int socket_memory_stick;
int socket_swap;
int socket_cpu;
t_bitarray* bitmap_bloques;

///////////////////////


void* handle_connections(void* arg);
void* handle_ks(void* arg);
void* handle_cpu(void* arg);
void* handle_ms(void* arg);


typedef struct {
    int id_segmento;
    uint32_t base;
    uint32_t limite;
} t_segmento; // revisar 



typedef struct {
    pid_t pid;
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

t_dictionary* diccionario_instrucciones; 

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
    int socket;
} t_memory_stick;

typedef struct{
    pid_t pid;
    int id_segmento;
    uint32_t limite;
    uint32_t bloque_swap;
}t_metadata_swap;

t_dictionary* diccionario_procesos_suspendidos;

typedef struct{
    char* puerto;
    char* log_level;
    int segment_max_size;
    char* alloc_strategy;
    int instr_delay;
    int compaction_delay;
    char* scripts_basepath;
}t_config_km;

extern t_config_km config_km;

// FUNCIONES
bool acceso_valido_en_segmentos(pid_t pid, uint32_t direccion_fisica, uint32_t cantidad_bytes);

t_memory_stick* encontrar_stick_por_base_del_segmento(uint32_t base_segmento);


t_list* cargar_instrucciones(char* path_instrucciones);
char* obtener_instruccion(t_list* instrucciones, uint32_t pc_actual);
char* armar_argumentos(char** tokens);


t_segmento* buscar_best_bloque(char* pid_string, int id_segmento, int tamanio_segmento);
t_segmento* buscar_worst_bloque(char* pid_string, int id_segmento, int tamanio_segmento);
t_segmento* crear_segmento(t_hueco_libre* hueco_hallado,char* pid, uint32_t id_segmento, uint32_t tamanio_segmento);
bool verificar_tamanio_valido(int tamanio_seg);
uint32_t calcular_memoria_libre();
uint32_t calcular_memoria_total();
bool existe_hueco_contiguo(uint32_t tamanio_segmento);
t_segmento* elegir_algoritmo(char* pid_string, int id_segmento, int tamanio_segmento);

t_list* obtener_todos_los_segmentos();
void obtener_segmentos(char* pid_str, void* contexto_ejec);
bool comparar_segmentos_por_base(void* a, void* b);
void reconstruir_huecos_despues_de_compactar(uint32_t proxima_base_libre);
void *obtener_contenido_segmento(t_segmento *segmento);
bool solicitar_lectura_a_stick(t_memory_stick *stick, uint32_t base_segmento_en_stick, uint32_t tamanio, void *destino);
void escribir_segmento_en_ms(t_segmento* segmento_restaurado, void* contenido);
char* obtener_contenido_bloque_swap(uint32_t bloque_swap);
t_segmento* iniciar_creacion_segmento(pid_t pid, int id_segmento, uint32_t tamanio_segmento, int socket_ks);
uint32_t enviar_segmento_a_swap(pid_t pid, t_segmento* segmento, char* contenido);
uint32_t obtener_bloque_libre_swap();

void iniciar_compactacion(int socket_ks);
