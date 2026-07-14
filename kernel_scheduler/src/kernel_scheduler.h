#ifndef KERNEL_SCHEDULER_H_
#define KERNEL_SCHEDULER_H_

#include <utils/serializacion.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
#include <semaphore.h>
#include "globales.h"
#include "planificacion.h"

void* handle_connections(void* arg);
void* handle_km(void* arg);
void *handle_cpu(void *arg);
void *handle_io(void *arg);
void *handle_planificacion(void *arg);
void* hilo_planificador_principal(void* arg);
//void* temporizador_timeout(void* arg);
void inicializar_ks(int argc, char* argv[]);
void enviar_proceso(int socket_km, pid_t pid, char* path_proceso);
void crear_proceso_inicial(int socket_km, char* path_proceso_inicial);
void finalizar_io(pid_t pid);
void cambiar_estado_proceso(t_proceso_pcb *proceso, t_estado_proceso estado_siguiente);
void liberar_recursos();
const char* estado_to_string(t_estado_proceso estado);
char* solicitar_contenido_a_km(int socket_km, pid_t pid, uint32_t direccion_fisica, uint32_t tamanio);
t_proceso_pcb* crear_pcb(pid_t pid, int prioridad);
t_proceso_pcb* remover_proceso_por_pid(t_list* cola, pid_t pid);
void* temporizador_timeout(void* arg);
void iniciar_temporizador_timeout(pid_t pid);


void solicitar_mem_free_a_km(int socket_km, pid_t pid, uint32_t id_segmento);


#endif /* KERNEL_SCHEDULER_H_ */
