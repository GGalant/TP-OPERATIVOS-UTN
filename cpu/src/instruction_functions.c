#include "cpu.h"

// No hace nada, solo consume el ciclo
void ejecutar_noop(){
    return;
};

/*
Asigna al registro el valor pasado como parámetro.
*/
void ejecutar_set(char* registro, char* valor_set) {
    
    log_debug(log_cpu, "SET: Iniciando Ejecucion");

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

    log_debug(log_cpu, "SUM: Iniciando Ejecucion");

    void * cpu_reg_destino = string_to_registro(registro_destino);
    void * cpu_reg_origen = string_to_registro(registro_origen);
    
    size_t reg_size_dest = get_registro_size(registro_destino);
    size_t reg_size_orig = get_registro_size(registro_origen);


    if (cpu_reg_destino == NULL || cpu_reg_origen == NULL) {
        log_error(log_cpu, "SUM: Algun registro es invalido (Dest: %s, Orig: %s)", registro_destino, registro_origen);
        return;
    }

    uint32_t val_orig = get_registro_value(reg_size_orig, cpu_reg_origen);
    uint32_t val_dest = get_registro_value(reg_size_dest, cpu_reg_destino);

    uint32_t resultado = val_dest + val_orig;

    set_reg_value(reg_size_dest, cpu_reg_destino, resultado);

    log_debug(log_cpu, "SUM: Ejecutado");

    return;
    
};

/*
Resta al Registro Destino el Registro Origen y deja el resultado en el Registro Destino.
*/
void ejecutar_sub(char* registro_destino, char* registro_origen){
    
    log_debug(log_cpu, "SUB: Iniciando Ejecucion");

    void * cpu_reg_destino = string_to_registro(registro_destino);
    void * cpu_reg_origen = string_to_registro(registro_origen);

    if (cpu_reg_destino == NULL || cpu_reg_origen == NULL) {
        log_error(log_cpu, "Sub: Algun Registro es invalido (Dest: %s, Orig: %s)", registro_destino, registro_origen);
        return;
    }

    size_t reg_size_orig = get_registro_size(registro_origen);
    size_t reg_size_dest = get_registro_size(registro_destino);

    uint32_t val_orig = get_registro_value(reg_size_orig, cpu_reg_origen);
    uint32_t val_dest = get_registro_value(reg_size_dest, cpu_reg_destino);

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

    log_debug(log_cpu, "JNZ: Iniciando Ejecucion");

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
        //TODO revisar si este ++ puede dar problemas
        cpu.pc++;
        log_debug(log_cpu, "JNZ: Salto ignorado");
    }
}

// Lee de memoria hacia el registro pasado por parametro (usa SI como origen)
void ejecutar_mov_in(char* registro){

    log_debug(log_cpu, "MOV_IN: Iniciando Ejecucion");

    void * cpu_reg_destino = string_to_registro(registro);
    
    if (cpu_reg_destino == NULL) {
        log_error(log_cpu, "MOV_IN: Registro destino invalido (%s)", registro);
        return;
    }

    size_t reg_size = get_registro_size(registro);

    uint32_t direccion_logica = cpu.si;

    uint32_t direccion_fisica = mmu_traducir_direccion(direccion_logica, reg_size); 

    if (direccion_fisica == ERROR) {
        log_error(log_cpu, "MOV_IN - SEG_FAULT: Dirección lógica invalida (%u)", direccion_logica);
        return;
    }

    uint32_t valor_leido = 0;

    int resultado = dma_leer_memoria(direccion_fisica, reg_size, (uint8_t*)&valor_leido);
    
    if (resultado == ERROR) {
        log_warning(log_cpu, "MOV_IN: OCURRIÓ UN ERROR EN EL DMA.");
        return;
    }

    set_reg_value(reg_size, cpu_reg_destino, valor_leido);

    log_debug(log_cpu, "MOV_IN: Ejecutado");
}

// Escribe en memoria desde el registro pasado por parametro (usa DI como destino)
void ejecutar_mov_out(char* registro){

    log_debug(log_cpu, "MOV_OUT: Iniciando Ejecucion");

    void * cpu_reg_origen = string_to_registro(registro);
    
    if (cpu_reg_origen == NULL) {
        log_error(log_cpu, "MOV_OUT: Registro origen invalido (%s)", registro);
        return;
    }

    size_t reg_size = get_registro_size(registro);
    
    uint32_t valor_a_escribir = get_registro_value(reg_size, cpu_reg_origen);

    uint32_t direccion_logica = cpu.di;

    uint32_t direccion_fisica = mmu_traducir_direccion(direccion_logica, sizeof(valor_a_escribir)); 

    if (direccion_fisica == ERROR) {
        log_error(log_cpu, "MOV_IN - SEG_FAULT: Dirección lógica invalida (%u)", direccion_logica);
        return;
    }

    int resultado = dma_escribir_memoria(direccion_fisica, reg_size, (uint8_t*)&valor_a_escribir);
    
    if (resultado == ERROR) {
        log_warning(log_cpu, "MOV_OUT: OCURRIÓ UN ERROR EN EL DMA.");
        return;
    }

    log_debug(log_cpu, "MOV_OUT: Ejecutado");
}

/*
Toma la dirección apuntada por el registro SI y 
copia la cantidad de bytes indicadas en el registro tamaño 
a la posición de memoria apuntada por el registro DI
*/
void ejecutar_copy_mem(char* registro_tamanio) {

    log_debug(log_cpu, "COPY_MEM: Iniciando Ejecucion");

    void * cpu_reg_tamanio = string_to_registro(registro_tamanio);
    
    if (cpu_reg_tamanio == NULL) {
        log_error(log_cpu, "COPY_MEM: Registro de tamaño invalido (%s)", registro_tamanio);
        return;
    }

    size_t reg_size = get_registro_size(registro_tamanio);
    
    uint32_t bytes_a_copiar = get_registro_value(reg_size, cpu_reg_tamanio);

    if (bytes_a_copiar == 0) {
        log_warning(log_cpu, "COPY_MEM: Intento de copiar 0 bytes. Operación omitida.");
        return;
    }

    uint32_t dir_logica_origen = cpu.si;
    uint32_t dir_logica_destino = cpu.di;

    uint32_t dir_fisica_origen = mmu_traducir_direccion(dir_logica_origen, bytes_a_copiar);
    uint32_t dir_fisica_destino = mmu_traducir_direccion(dir_logica_destino, bytes_a_copiar);

    if (dir_fisica_origen == ERROR) {
        log_error(log_cpu, "COPY_MEM - SEG_FAULT: Dirección lógica de origen invalida (%u)", dir_logica_origen);
        return;
    }

    if (dir_fisica_destino == ERROR) {
        log_error(log_cpu, "COPY_MEM - SEG_FAULT: Dirección lógica de destino invalida (%u)", dir_logica_origen);
        return;
    }

    int resultado = dma_copiar_memoria(dir_fisica_origen, dir_fisica_destino, bytes_a_copiar);

    if (resultado == ERROR) {
        log_warning(log_cpu, "COPY_MEM: OCURRIÓ UN ERROR EN EL DMA.");
        return;
    }

    log_debug(log_cpu, "COPY_MEM: Ejecutado");
}

void ejecutar_sleep(char* parametro_tiempo) {

    log_debug(log_cpu, "SLEEP: Iniciando Ejecucion");

    int tiempo = atoi(parametro_tiempo);
    uint32_t pc_actualizado = cpu.pc;
    pid_t pid = cpu.pid;

    t_paquete* paquete = crear_paquete(SYSCALL_SLEEP); 

    agregar_a_paquete(paquete, &pid, sizeof(pid_t));
    agregar_a_paquete(paquete, &pc_actualizado, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tiempo, sizeof(int));

    enviar_paquete_send_all(paquete, conexion_ks);
    eliminar_paquete(paquete);

    se_ejecuto_syscall = true; 

    return;
}

void ejecutar_stdout(char* reg_direccion, char* reg_tamanio) {

    log_debug(log_cpu, "STDOUT: Iniciando Ejecucion");

    void * cpu_reg_direcc = string_to_registro(reg_direccion);
    void * cpu_reg_tamanio = string_to_registro(reg_tamanio);

    size_t reg_size_dire = get_registro_size(reg_direccion);
    size_t reg_size_tam = get_registro_size(reg_tamanio);
       
    uint32_t direccion_logica = get_registro_value(reg_size_dire, cpu_reg_direcc);
    uint32_t tamanio = get_registro_value(reg_size_tam, cpu_reg_tamanio);

    t_paquete* paquete = crear_paquete(SYSCALL_STDOUT);
    
    agregar_a_paquete(paquete, &direccion_logica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));

    enviar_paquete_send_all(paquete, conexion_ks);
    eliminar_paquete(paquete);

    se_ejecuto_syscall = true; 

    return;

}

void ejecutar_exit() {

    log_debug(log_cpu, "EXIT: Iniciando Ejecucion");
    
    op_code cod_op = SYSCALL_EXIT;
    send(conexion_ks, &cod_op, sizeof(op_code), 0);

    se_ejecuto_syscall = true; 

    return;
}

void ejecutar_mutex_create(char* nombre_mutex) {

    log_debug(log_cpu, "MUTEX_CREATE: Iniciando Ejecucion");
    
    if(nombre_mutex == NULL){
        log_error(log_cpu, "MUTEX_CREATE: El nombre mutex es null");
        return;
    }

    t_paquete* paquete = crear_paquete(SYSCALL_MUTEX_CREATE);
    
    //TODO: Agregue strlen+1 por las dudas, revisar si hace falta
    agregar_a_paquete(paquete, nombre_mutex, string_length(nombre_mutex) + 1);
    
    enviar_paquete_send_all(paquete, conexion_ks);
    eliminar_paquete(paquete);
    se_ejecuto_syscall = true;
    return;
}

void ejecutar_mutex_lock(char* nombre_mutex) {

    log_debug(log_cpu, "MUTEX_LOCK: Iniciando Ejecucion");

    if(nombre_mutex == NULL){
        log_error(log_cpu, "MUTEX_LOCK: El nombre mutex es null");
        return;
    }

    t_paquete* paquete = crear_paquete(SYSCALL_MUTEX_LOCK);
    
    //TODO: Agregue strlen+1 por las dudas, revisar si hace falta
    agregar_a_paquete(paquete, nombre_mutex, string_length(nombre_mutex) + 1);
    
    enviar_paquete_send_all(paquete, conexion_ks);
    eliminar_paquete(paquete);
    se_ejecuto_syscall = true;
}

void ejecutar_mutex_unlock(char* nombre_mutex) {
    
    log_debug(log_cpu, "MUTEX_UNLOCK: Iniciando Ejecucion");

    if(nombre_mutex == NULL){
        log_error(log_cpu, "MUTEX_UNLOCK: El nombre mutex es null");
        return;
    }

    t_paquete* paquete = crear_paquete(SYSCALL_MUTEX_UNLOCK);

    //TODO: Agregue strlen+1 por las dudas, revisar si hace falta
    
    agregar_a_paquete(paquete, nombre_mutex, string_length(nombre_mutex) + 1);
    
    enviar_paquete_send_all(paquete, conexion_ks);
    eliminar_paquete(paquete);
    se_ejecuto_syscall = true;
}

void ejecutar_mem_alloc(char* param_id_segmento, char* param_tamanio) {

    log_debug(log_cpu, "MEM_ALLOC: Iniciando Ejecucion");

    uint32_t id_segmento = (uint32_t)atoi(param_id_segmento);
    uint32_t tamanio = (uint32_t)atoi(param_tamanio);

    t_paquete* paquete = crear_paquete(SYSCALL_MEM_ALLOC);
    
    agregar_a_paquete(paquete, &id_segmento, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
    
    enviar_paquete_send_all(paquete, conexion_ks);
    eliminar_paquete(paquete);
    se_ejecuto_syscall = true;
}

void ejecutar_mem_free(char* param_id_segmento) {

    log_debug(log_cpu, "MEM_FREE: Iniciando Ejecucion");

    uint32_t id_segmento = (uint32_t)atoi(param_id_segmento);

    t_paquete* paquete = crear_paquete(SYSCALL_MEM_FREE);
    
    agregar_a_paquete(paquete, &id_segmento, sizeof(uint32_t));
    
    enviar_paquete_send_all(paquete, conexion_ks);
    eliminar_paquete(paquete);
    se_ejecuto_syscall = true;
}

void ejecutar_stdin(char* reg_direccion, char* reg_tamanio) {

    log_debug(log_cpu, "STDIN: Iniciando Ejecucion");

    void * cpu_reg_direcc = string_to_registro(reg_direccion);
    void * cpu_reg_tamanio = string_to_registro(reg_tamanio);

    size_t reg_size_dire = get_registro_size(reg_direccion);
    size_t reg_size_tam = get_registro_size(reg_tamanio);
       
    uint32_t direccion_logica = get_registro_value(reg_size_dire, cpu_reg_direcc);
    uint32_t tamanio = get_registro_value(reg_size_tam, cpu_reg_tamanio);

    t_paquete* paquete = crear_paquete(SYSCALL_STDIN);
    
    agregar_a_paquete(paquete, &direccion_logica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));

    enviar_paquete_send_all(paquete, conexion_ks);
    eliminar_paquete(paquete);

    se_ejecuto_syscall = true; 

    return;
}

void ejecutar_init_proc(char* archivo_instrucciones, char* param_prioridad) {

    log_debug(log_cpu, "INIT_PROC: Iniciando Ejecucion");

    log_debug(log_cpu, "ejecutar_init_proc(%s, %s)", archivo_instrucciones, param_prioridad);

    int prioridad = atoi(param_prioridad);

    t_paquete* paquete = crear_paquete(SYSCALL_INIT_PROC);
    
    agregar_a_paquete(paquete, archivo_instrucciones, strlen(archivo_instrucciones) + 1);
    agregar_a_paquete(paquete, &prioridad, sizeof(int));
    
    enviar_paquete_send_all(paquete, conexion_ks);
    eliminar_paquete(paquete);

    se_ejecuto_syscall = true;

    return;
}

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