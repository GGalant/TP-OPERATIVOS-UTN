#include "cpu.h"

// No hace nada, solo consume el ciclo
void ejecutar_noop(){
    return;
};

/*
Asigna al registro el valor pasado como parámetro.
*/
void ejecutar_set(char* registro, char* valor_set) {

    void* cpu_reg = string_to_registro(registro);
    size_t reg_size = get_registro_size(registro);
    
    if (cpu_reg == NULL) {
        log_error(log_cpu, "SET: Registro invalido %s", registro);
        return;
    }

    uint32_t valor = (uint32_t) atoi(valor_set);

    set_reg_value(reg_size, cpu_reg, valor);

    log_debug(log_cpu, "SET: Ejecutado");  

    return;
}

/*
Suma al Registro Destino el Registro Origen y deja el resultado en el Registro Destino.
*/
void ejecutar_sum(char* registro_destino, char* registro_origen){

    void * cpu_reg_destino = string_to_registro(registro_destino);
    size_t reg_size_dest = get_registro_size(registro_destino);
    void * cpu_reg_origen = string_to_registro(registro_origen);
    size_t reg_size_orig = get_registro_size(registro_origen);


    if (cpu_reg_destino == NULL || cpu_reg_origen == NULL) {
        log_error(log_cpu, "SUM: Registro invalido (Dest: %s, Orig: %s)", registro_destino, registro_origen);
        return;
    }

    uint32_t val_orig = get_registro_value(get_registro_size(registro_origen), cpu_reg_origen);
    uint32_t val_dest = get_registro_value(get_registro_size(registro_destino), cpu_reg_destino);

    uint32_t resultado = val_dest + val_orig;

    set_reg_value(reg_size_dest, cpu_reg_destino, resultado);

    log_debug(log_cpu, "SUM: Ejecutado");

    return;
    
};

/*
Resta al Registro Destino el Registro Origen y deja el resultado en el Registro Destino.
*/
void ejecutar_sub(char* registro_destino, char* registro_origen){

    void * cpu_reg_destino = string_to_registro(registro_destino);
    void * cpu_reg_origen = string_to_registro(registro_origen);

    if (cpu_reg_destino == NULL || cpu_reg_origen == NULL) {
        log_error(log_cpu, "Sub: Registro invalido (Dest: %s, Orig: %s)", registro_destino, registro_origen);
        return;
    }

    size_t reg_size_orig = get_registro_size(registro_origen);
    size_t reg_size_dest = get_registro_size(registro_destino);

    uint32_t val_orig = get_registro_value(get_registro_size(registro_origen), cpu_reg_origen);
    uint32_t val_dest = get_registro_value(get_registro_size(registro_destino), cpu_reg_destino);

    uint32_t resultado = val_dest - val_orig;

    set_reg_value(reg_size_dest, cpu_reg_destino, resultado);

    log_debug(log_cpu, "SUB: Ejecutado");

    return;

};

/*
Si el valor del registro es distinto de cero, 
actualiza el program counter al número de instrucción pasada por parámetro.v
*/
void ejecutar_jnz(char* registro, char* str_nuevo_valor_pc){
    void * cpu_reg = string_to_registro(registro);

    if (cpu_reg == NULL) {
        log_error(log_cpu, "JNZ: Registro invalido (%s)", registro);
        return;
    }

    size_t reg_size = get_registro_size(registro);
    
    uint32_t val_reg = get_registro_value(reg_size, cpu_reg);

    //Si el valor es distinto de 0, entonces salta
    if (val_reg != 0) {
        uint32_t nuevo_valor_pc = (uint32_t) atoi(str_nuevo_valor_pc);
        
        cpu.pc = nuevo_valor_pc;
        
        log_debug(log_cpu, "JNZ: Salto tomado. Nuevo PC = %u", nuevo_valor_pc);
    } else {
        log_debug(log_cpu, "JNZ: Salto ignorado");
    }
}

// Lee de memoria hacia el registro pasado por parametro (usa SI como origen)
void ejecutar_mov_in(char* registro){
    void * cpu_reg_destino = string_to_registro(registro);
    
    if (cpu_reg_destino == NULL) {
        log_error(log_cpu, "MOV_IN: Registro destino invalido (%s)", registro);
        return;
    }

    size_t reg_size = get_registro_size(registro);

    uint32_t direccion_logica = cpu.si;

    //TODO implementar MMU y acceso a memoria
    uint32_t direccion_fisica = 0;
    uint32_t valor_leido = 0; 

    set_reg_value(reg_size, cpu_reg_destino, valor_leido);

    log_debug(log_cpu, "MOV_IN: Ejecutado");
}

// Escribe en memoria desde el registro pasado por parametro (usa DI como destino)
void ejecutar_mov_out(char* registro){
    void * cpu_reg_origen = string_to_registro(registro);
    
    if (cpu_reg_origen == NULL) {
        log_error(log_cpu, "MOV_OUT: Registro origen invalido (%s)", registro);
        return;
    }

    size_t reg_size = get_registro_size(registro);
    
    uint32_t valor_a_escribir = get_registro_value(reg_size, cpu_reg_origen);

    uint32_t direccion_logica = cpu.di;

    //TODO implementar MMU y acceso a memoria
    uint32_t direccion_fisica = 0; 

    log_debug(log_cpu, "MOV_OUT: Ejecutado");
}

/*
Toma la dirección apuntada por el registro SI y 
copia la cantidad de bytes indicadas en el registro tamaño 
a la posición de memoria apuntada por el registro DI
*/
void ejecutar_copy_mem(char* registro_tamanio) {
    void * cpu_reg_tamanio = string_to_registro(registro_tamanio);
    
    if (cpu_reg_tamanio == NULL) {
        log_error(log_cpu, "COPY_MEM: Registro de tamaño invalido (%s)", registro_tamanio);
        return;
    }

    size_t reg_size = get_registro_size(registro_tamanio);
    
    uint32_t bytes_a_copiar = get_registro_value(reg_size, cpu_reg_tamanio);

    uint32_t dir_logica_origen = cpu.si;
    uint32_t dir_logica_destino = cpu.di;

    // TODO: Implementar MMU y acceso a memoria
    
    uint32_t dir_fisica_origen = 0;
    uint32_t dir_fisica_destino = 0;
    uint32_t valor = 0; 

    log_debug(log_cpu, "COPY_MEM: Ejecutado");
}


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