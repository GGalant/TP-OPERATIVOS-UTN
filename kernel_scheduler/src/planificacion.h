#ifndef PLANIFICACION_H_
#define PLANIFICACION_H_
#include <commons/collections/list.h>
#include "cpu.h"
#include "globales.h"

// Funciones de inicialización y gestión del planificador
void inicializar_planificador();
void liberar_planificador();

// Hilo principal del planificador (consumidor)
void* hilo_planificador_principal(void* arg);

// Funciones auxiliares para manejo de CPUs
t_cpu* buscar_cpu_libre();
void iniciar_hilo_quantum(t_proceso_pcb* proceso, int cpu_id);
void cancelar_hilo_quantum_si_existe(t_proceso_pcb* proceso, const char* motivo);

// Funciones de utilidad
t_algoritmo_planificacion string_to_algoritmo(const char* algoritmo);
void enviar_proceso_a_cpu(int socket_cpu, pid_t pid_proceso);

// Legacy - se mantiene por compatibilidad pero será reemplazada
void *handle_planificacion(void *arg);
void* controlar_quantum(void* arg);

#endif /* PLANIFICACION_H_ */