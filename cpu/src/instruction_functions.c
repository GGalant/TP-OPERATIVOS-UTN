#include "cpu.h"

// No hace nada, solo consume el ciclo
void ejecutar_noop(){
    return;
};

void ejecutar_set(char* registro_set, char* valor_set) {

    void* cpu_reg = string_to_registro(registro_set);
    size_t reg_size = get_registro_size(registro_set);
    
    if (cpu_reg == NULL) {
        log_error(log_cpu, "Set: Registro inválido %s", registro_set);
        return;
    }

    uint32_t valor = (uint32_t) atoi(valor_set);

    set_reg_value(reg_size, cpu_reg, valor);

    log_debug(log_cpu, "Set: Ejecutado");  

    return;
}

void ejecutar_sum(char* registro_destino, char* registro_origen){

    void * cpu_reg_destino = string_to_registro(registro_destino);
    size_t reg_size_dest = get_registro_size(registro_destino);
    void * cpu_reg_origen = string_to_registro(registro_origen);
    size_t reg_size_orig = get_registro_size(registro_origen);


    if (cpu_reg_destino == NULL || cpu_reg_origen == NULL) {
        log_error(log_cpu, "Sum: Registro invalido (Dest: %s, Orig: %s)", registro_destino, registro_origen);
        return;
    }

    uint32_t val_orig = get_registro_value(get_registro_size(registro_origen), cpu_reg_origen);
    uint32_t val_dest = get_registro_value(get_registro_size(registro_destino), cpu_reg_destino);

    uint32_t resultado = val_dest + val_orig;

    set_reg_value(reg_size_dest, cpu_reg_destino, resultado);

    log_debug(log_cpu, "Sum: Ejecutado");

    return;
    
};

void ejecutar_sub(char* registro_destino, char* registro_origen){

    void * cpu_reg_destino = string_to_registro(registro_destino);
    size_t reg_size_dest = get_registro_size(registro_destino);
    void * cpu_reg_origen = string_to_registro(registro_origen);
    size_t reg_size_orig = get_registro_size(registro_origen);


    if (cpu_reg_destino == NULL || cpu_reg_origen == NULL) {
        log_error(log_cpu, "Sub: Registro invalido (Dest: %s, Orig: %s)", registro_destino, registro_origen);
        return;
    }

    uint32_t val_orig = get_registro_value(get_registro_size(registro_origen), cpu_reg_origen);
    uint32_t val_dest = get_registro_value(get_registro_size(registro_destino), cpu_reg_destino);

    uint32_t resultado = val_dest - val_orig;

    set_reg_value(reg_size_dest, cpu_reg_destino, resultado);

    log_debug(log_cpu, "Sub: Ejecutado");

    return;

};

void ejecutar_jnz(char* registro_jnz, char* nuevo_valor_pc);

// Lee de memoria hacia el registro pasado por parametro (usa SI como origen)
void ejecutar_mov_in(char* registro_datos);

// Escribe en memoria desde el registro pasado por parametro (usa DI como destino)
void ejecutar_mov_out(char* registro_datos);

/*
Toma la dirección apuntada por el registro SI y 
copia la cantidad de bytes indicadas en el registro tamaño 
a la posición de memoria apuntada por el registro DI
*/
void ejecutar_copy_mem(char* registro_tamanio);


void ejecutar_mutex_create(char* nombre_mutex);
void ejecutar_mutex_lock(char* nombre_mutex);
void ejecutar_mutex_unlock(char* nombre_mutex);
void ejecutar_mem_alloc(char* id_segmento, char* tamanio);
void ejecutar_mem_free(char* id_segmento);
void ejecutar_sleep(char* tiempo);
void ejecutar_stdout(char* reg_direccion_logica, char* reg_tamanio);
void ejecutar_stdin(char* reg_direccion_logica, char* reg_tamanio);
void ejecutar_init_proc(char* path_instrucciones, char* prioridad);
void ejecutar_exit();

// utils:

int get_registro_size(char* s) {
    if (s[0] == 'E' || strcmp(s, "PC") == 0 || strcmp(s, "SI") == 0 || strcmp(s, "DI") == 0) {
        return sizeof(uint32_t);
    }
    return sizeof(uint8_t);
}

uint32_t get_registro_value( size_t registro_size, void* registro){
    
    uint32_t val_dest;
    if (registro_size == sizeof(uint32_t)) {
        val_dest = *((uint32_t*) registro);
    } else {
        val_dest = (uint32_t) *((uint8_t*) registro);
    }
    return val_dest;

}

void set_reg_value(size_t reg_size, void* registro, uint32_t valor){

    if (reg_size == sizeof(uint32_t)) {
        *((uint32_t*) registro) = valor;
    } else {
        *((uint8_t*) registro) = (uint8_t) valor;
    }
}