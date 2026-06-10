#include "cpu.h"

//TODO me gustaria no tener tantas variables globales :)

static char instruccion_buffer[128];
//Variables extern definidas en el header:
int conexion_ms = -1;
int conexion_ks = -1;
int conexion_km = -1;
int id_cpu = -1;
bool se_ejecuto_syscall = false;
t_cpu cpu = {0};
t_log* log_cpu = NULL;
t_instruccion instruccion_actual = {0};
bool error_fatal = false;

int main(int argc, char *argv[]){

    //El log se asigna, debe ser destruido con log_destroy
    int status = inicializacion_y_pasos_iniciales(argc, argv);

    if(status == -1){
        return EXIT_FAILURE;
    }

    while(true) {

        log_debug(log_cpu, "¡ Estoy libre !");

        op_code codigo_operacion = recibir_operacion(conexion_ks);

        log_debug(log_cpu, "Kernel Scheduler mandó una operación");

        switch(codigo_operacion) {  

            case EJECUTAR_PROCESO: {
                pid_t pid_recibido = recibir_pid(conexion_ks);

                pull_contexto(pid_recibido);

                do { 

                fetch();
                decode();
                execute();
                flush_buffers();

                } while (!check_interrupt());

                push_contexto();
                
                limpiar_contexto();

                //TODO hace falta enviar el pid a KS?
                //enviar_pid_actual_a_KS();

                break;
            }

            case ERROR:
                log_error(log_cpu, "El servidor Kernel Scheduler se desconectó.");
                log_destroy(log_cpu);
                return EXIT_FAILURE;  

            case INTERRUPCION_FIN_QUANTUM:
            case INTERRUPCION_DESALOJO:
                //Si el Kernel Scheduler envia una interrupcion en el mismo momento que la CPU ejecuta una SYSCALL, 
                //la interrupcion queda en el buffer y por lo tanto se lee en esta etapa, pero ya no tiene relevanica
                log_debug(log_cpu, "Se recibio una Interrupcion fuera del ciclo de instruccion, se desestima.");
                break;

            default: 
                log_warning(log_cpu, "Operacion desconocida: %d", codigo_operacion);
                break;
        }

        if(error_fatal){
            config_destroy(config);
            log_destroy(log_cpu);
            return EXIT_FAILURE;
        }

    }
    
    log_destroy(log_cpu);
    
    return 0;
}

t_log inicializar_log(int log_level){    //TODO2 ver si int puede ser enum de commons

    char* log_filename = string_from_format("cpu_%d.log", id_cpu);
    char* log_header   = string_from_format("CPU <%d>", id_cpu);
    t_log logger = log_create(log_filename, log_header, 1, log_level);
    free(log_filename);
    free(log_header);
    return logger

}

pid_t recibir_pid(int fd){

    pid_t pid_recibido;

    recv(conexion_ks, &pid_recibido, sizeof(pid_t), MSG_WAITALL);

    if(pid_recibido == -1){
        log_error(log_cpu, "Error al recibir el PID");
        error_fatal = true;
        return pid_recibido;
    } 
    
    log_debug(log_cpu, "Se recibio el PID: %u", pid_recibido);

    return pid_recibido;

}

void flush_buffers(){
    log_debug(log_debu, "Finalizando ciclo: Limpiando buffers");
    instruccion_buffer[0] = '\0';
    instruccion_actual = {0};
}

int inicializacion_y_pasos_iniciales(int argc, char* argv[]){
    
    if(argc<3){
        perror("No se proporcionaron todos los argumentos [Archivo Config] [Identificador]");
        return -1;
    }
    
    //Argumentos iniciales
    char* config_path = argv[1]; 
    char* id_cpu_str = argv[2];
    id_cpu = atoi(id_cpu_str);
    
    log_info(log_cpu, "Iniciando CPU <%d>", id_cpu);

    //Pase de "Archivo de configuracion" a estructura:
    t_cpu_env env = {0};

    t_config* config = config_create(config_path);

    if(config == NULL){
        return -1;
    }

    map_config_variables(config, &env);

    //TODO2 ver si int puede ser enum de commons
    int log_level = log_level_from_string(env.str_log_level);
    
    log_cpu = inicializar_log(env);

    //Conexiones con modulos
    conexion_ks = conectar_modulo(env.ip_ks, env.puerto_ks, "Kernel Scheduler");
    conexion_km = conectar_modulo(env.ip_km, env.puerto_km, "Kernel Memory");
    conexion_ms = conectar_modulo(env.ip_ms, env.puerto_ms, "Memory Stick");

    if(conexion_ks == -1 || conexion_km == -1 || conexion_ms == -1 ){
        config_destroy(config);
        log_destroy(log_cpu);
        return -1;
    }

    config_destroy(config);
    
    //Envio de handshakes
    op_code handshake_cpu = HANDSHAKE_CPU;
    
    send(conexion_ks, &handshake_cpu, sizeof(op_code), 0);
    send(conexion_ks, &id_cpu,        sizeof(int), 0);

    send(conexion_km, &handshake_cpu, sizeof(op_code), 0);
    send(conexion_km, &id_cpu,        sizeof(int), 0);
    
    send(conexion_ms, &handshake_cpu, sizeof(op_code), 0);

    return 0;
}

void enviar_pid_actual_a_KS(){ //TODO mejorar manejo

    
    op_code operacion_envio_pid = ENVIO_PID;
    pid_t pid_a_enviar = cpu.pid;

    log_debug(log_cpu, "Enviando el pid %d al Kernel Scheduler ( op_code: ENVIO_PID )", pid_a_enviar);

    send(conexion_ks, &operacion_envio_pid, sizeof(op_code), 0);
    send(conexion_ks, &pid_a_enviar,        sizeof(pid_t), 0);
    return;
}

void* fetch(){

    log_info(log_cpu, "## PID: %u - FETCH - Program Counter: %u", cpu.pid, cpu.pc);

    t_paquete* paquete = crear_paquete(PEDIDO_INSTRUCCION);
    agregar_a_paquete(paquete, &cpu.pid, sizeof(pid_t));
    agregar_a_paquete(paquete, &cpu.pc, sizeof(uint32_t));
    enviar_paquete_send_all(paquete, conexion_km);
    eliminar_paquete(paquete);

    op_code operacion = recibir_operacion(conexion_km);
    
    if(operacion == -1){
        log_error(log_cpu, "Fetch: Error al recibir_operacion");
        error_fatal = true;
        return NULL;
    }

    if(operacion != RTA_PEDIDO_INSTRUCCION){
        log_error(log_cpu, "Fetch: La operacion recibida de Kernel Memory no fue la esperada, se recibio: %d", operacion);
        error_fatal = true;
        return NULL;
    }

    recibir_instruccion_de_km();
    
    log_debug(log_cpu, "Fetch: Finalizado exitosamente");
    
    return instruccion_buffer;
    
};

void recibir_instruccion_de_km(){
    int largo;

    log_debug(log_cpu, "Fetch: recibiendo la longitud de la instruccion...");

    int res = recv(conexion_km, &largo, sizeof(int), MSG_WAITALL);

    if(res == -1 || res == 0){
        log_error(log_cpu, "Fetch: ERROR en el pedido del largo de instruccion, la conexion se perdió");
        error_fatal = true;
        return;
    }

    log_debug(log_cpu, "Fetch: largo de la instruccion obtenida.");

    if(largo <= 0){
        log_error(log_cpu, "Fetch: El largo de instruccion obtenido es invalido");
        error_fatal = true;
        return;
    }

    if(largo >= MAX_BUFFER_INSTRUCCION){
        log_error(log_cpu, "Fetch: La instruccion recibida de KM supera el largo maximo para una instruccion");
        error_fatal = true;
        return;
    }

    log_debug(log_cpu, "Fetch: recibiendo la instruccion...");
    
	int largo_recibido = recv(conexion_km, instruccion_buffer, largo, MSG_WAITALL);
    
    if(largo_recibido == -1 || largo_recibido == 0){
        log_error("Fetch: ERROR en el pedido de la instruccion la conexion se perdió");
        error_fatal = true;
        return;
    }
    
    if(largo_recibido != largo){
        log_error("Fetch: La instruccion recibida de KM no parece ser la prometida, no cumple con el largo informado justo antes");
        error_fatal = true;
        return;
    }

    instruccion_buffer[largo] = '\0';

    log_debug(log_cpu, "Fetch: Instruccion recibida y cargada en el buffer de instruccion");

    return;
}

void decode(){

    log_debug(log_cpu, "Decode: Iniciando decodificacion");

    if(instruccion_buffer[0] == '\0'){
        log_error(log_cpu, "Decode: No se encontraron instrucciones en el buffer");
        error_fatal = true;
        return;
    }

    char ** splitted_instruction = string_split(instruccion_buffer, " ");

    instruccion_actual.icode = string_to_instruction_code(splitted_instruction[0]);

    if (instruccion_actual.icode == INSTRUCTION_ERROR) {
        error_fatal = true;
        string_array_destroy(splitted_instruction);
        return;
    }

    int i = 0;
    while(splitted_instruction[i] != NULL && i < MAX_INSTRUCTION_PARAMS){
        strncpy(instruccion_actual.params[i], splitted_instruction[i], MAX_INSTRUCTION_PARAM_SIZE);
        instruccion_actual.params[i][MAX_INSTRUCTION_PARAM_SIZE - 1] = '\0';
        i++;
    } 
    //i vale 1 mas que el numero de parametros al salir del while
    //el comando es el parametro 0
    instruccion_actual.cant_params = i;

    string_array_destroy(splitted_instruction);

    log_debug(log_cpu, "Decode: Finalizado exitosamente");

    return;

};

void execute(){

    log_info(log_cpu, "## PID: %u - Ejecutando: %s - %s %s",
        cpu.pid,
        instruccion_actual.params[0],
        instruccion_actual.cant_params > 1 ? instruccion_actual.params[1] : "",
        instruccion_actual.cant_params > 2 ? instruccion_actual.params[2] : ""
    );

    switch(instruccion_actual.icode){
    case NOOP:
        ejecutar_noop();
        cpu.pc++;
    break;
    case SET:
        ejecutar_set(instruccion_actual.params[1], instruccion_actual.params[2]);
        cpu.pc++;
    break;
    case MOV_IN:
        ejecutar_mov_in(instruccion_actual.params[1]);
        cpu.pc++;
    break;
    case MOV_OUT:
        ejecutar_mov_out(instruccion_actual.params[1]);
        cpu.pc++;
    break;
    case SUM:
        ejecutar_sum(instruccion_actual.params[1], instruccion_actual.params[2]);
        cpu.pc++;
    break;
    case SUB:
        ejecutar_sub(instruccion_actual.params[1], instruccion_actual.params[2]);
        cpu.pc++;
    break;
    case JNZ:
        ejecutar_jnz(instruccion_actual.params[1], instruccion_actual.params[2]);
    break;
    case COPY_MEM:
        ejecutar_copy_mem(instruccion_actual.params[1]);
        cpu.pc++;
    break;
    case MUTEX_CREATE:
        cpu.pc++;
        ejecutar_mutex_create(instruccion_actual.params[1]);
    break;
    case MUTEX_LOCK:
        cpu.pc++;
        ejecutar_mutex_lock(instruccion_actual.params[1]);
    break;
    case MUTEX_UNLOCK:
        cpu.pc++;
        ejecutar_mutex_unlock(instruccion_actual.params[1]);
    break;
    case MEM_ALLOC:
        cpu.pc++;
        ejecutar_mem_alloc(instruccion_actual.params[1], instruccion_actual.params[2]);
    break;
    case MEM_FREE:
        cpu.pc++;
        ejecutar_mem_free(instruccion_actual.params[1]);
    break;
    case SLEEP:
        cpu.pc++;
        ejecutar_sleep(instruccion_actual.params[1]);
    break;
    case STDOUT:
        cpu.pc++;
        ejecutar_stdout(instruccion_actual.params[1], instruccion_actual.params[2]);
    break;
    case STDIN:
        cpu.pc++;
        ejecutar_stdin(instruccion_actual.params[1], instruccion_actual.params[2]);
    break;
    case INIT_PROC:
        cpu.pc++;
        ejecutar_init_proc(instruccion_actual.params[1], instruccion_actual.params[2]);
    break;
    case EXIT:
    //TODO HAY QUE HACER PC++ ACA??
        ejecutar_exit();
    break;
    case INSTRUCTION_ERROR:
        error_fatal = true;
    break;
    }
    return;

};

bool check_interrupt(){

    log_debug(log_cpu, "Chequeando interrupcion...");

    if (se_ejecuto_syscall) {
        log_debug(log_cpu, "Interrupcion sincronica recibida (SYSCALL ejecutada)");
        se_ejecuto_syscall = false;
        return true;
    }

    op_code codigo_operacion = recibir_operacion(conexion_ks);

    if(codigo_operacion == INTERRUPCION_DESALOJO || codigo_operacion == INTERRUPCION_FIN_QUANTUM){
        //TODO ver si hay que leer algo mas o realizar alguna accion.
        log_info(log_cpu, "## Interrupcion recibida");
        return true;
    }

    log_debug(log_cpu, "No se econtraron interrupciones");

    return false;
}

InstrucionCode string_to_instruction_code(char* s){

    if (strcmp(s, "SET") == 0) return SET;
    if (strcmp(s, "SUM") == 0) return SUM;
    if (strcmp(s, "SUB") == 0) return SUB;
    if (strcmp(s, "JNZ") == 0) return JNZ;
    if (strcmp(s, "MOV_IN") == 0) return MOV_IN;
    if (strcmp(s, "MOV_OUT") == 0) return MOV_OUT;
    if (strcmp(s, "COPY_MEM") == 0) return COPY_MEM;
    if (strcmp(s, "NOOP") == 0) return NOOP;
    if (strcmp(s, "MUTEX_CREATE") == 0) return MUTEX_CREATE;
    if (strcmp(s, "MUTEX_LOCK") == 0) return MUTEX_LOCK;
    if (strcmp(s, "MUTEX_UNLOCK") == 0) return MUTEX_UNLOCK;
    if (strcmp(s, "MEM_ALLOC") == 0) return MEM_ALLOC;
    if (strcmp(s, "MEM_FREE") == 0) return MEM_FREE;
    if (strcmp(s, "SLEEP") == 0) return SLEEP;
    if (strcmp(s, "STDOUT") == 0) return STDOUT;
    if (strcmp(s, "STDIN") == 0) return STDIN;
    if (strcmp(s, "INIT_PROC") == 0) return INIT_PROC;
    if (strcmp(s, "EXIT") == 0) return EXIT;

    return INSTRUCTION_ERROR;

}

void * string_to_registro(char* s){

    if (strcmp(s, "AX") == 0) return &cpu.ax;
    if (strcmp(s, "BX") == 0) return &cpu.bx;
    if (strcmp(s, "CX") == 0) return &cpu.cx;
    if (strcmp(s, "DX") == 0) return &cpu.dx;
    if (strcmp(s, "EAX") == 0) return &cpu.eax;
    if (strcmp(s, "EBX") == 0) return &cpu.ebx;
    if (strcmp(s, "ECX") == 0) return &cpu.ecx;
    if (strcmp(s, "EDX") == 0) return &cpu.edx;
    if (strcmp(s, "SI") == 0) return &cpu.si;
    if (strcmp(s, "DI") == 0) return &cpu.di;

    return NULL;
}

int conectar_modulo(char* ip, char* puerto, char* nombre_modulo) {
    int socket_conexion = crear_conexion(ip, puerto);

    if(socket_conexion == -1) {
        log_error(log_cpu, "Error al conectar con %s en %s:%s", nombre_modulo, ip, puerto);
        return -1; // Retornamos -1 para que el main sepa que falló
    }

    log_info(log_cpu, "Conectado exitosamente a %s", nombre_modulo);

    return socket_conexion;

}

int map_config_variables(t_config* config, t_cpu_env* env){

    env->str_log_level = config_get_string_value(config, "LOG_LEVEL");

    env->ip_ks = config_get_string_value(config, "IP_KS");
    env->puerto_ks = config_get_string_value(config, "PUERTO_KS");

    env->ip_km = config_get_string_value(config, "IP_KM");
    env->puerto_km = config_get_string_value(config, "PUERTO_KM");

    env->ip_ms= config_get_string_value(config, "IP_MS");
    env->puerto_ms = config_get_string_value(config, "PUERTO_MS");

    return 0;
}

void integrar_contexto(t_list* paquete_contexto) {

    cpu.pid = *(pid_t*)list_get(paquete_contexto, 0);
    cpu.pc = *(uint32_t*)list_get(paquete_contexto, 1);
    cpu.ax = *(uint8_t*) list_get(paquete_contexto, 2);
    cpu.bx = *(uint8_t*) list_get(paquete_contexto, 3);
    cpu.cx = *(uint8_t*) list_get(paquete_contexto, 4);
    cpu.dx = *(uint8_t*) list_get(paquete_contexto, 5);
    cpu.eax = *(uint32_t*)list_get(paquete_contexto, 6);
    cpu.ebx = *(uint32_t*)list_get(paquete_contexto, 7);
    cpu.ecx = *(uint32_t*)list_get(paquete_contexto, 8);
    cpu.edx = *(uint32_t*)list_get(paquete_contexto, 9);
    cpu.si = *(uint32_t*)list_get(paquete_contexto, 10);
    cpu.di = *(uint32_t*)list_get(paquete_contexto, 11);
    //cpu.tabla_segmentos = *(t_list**)list_get(paquete_contexto, 12);

}

t_paquete *empaquetar_contexto(){

    t_paquete *paquete_contexto = crear_paquete(RECIBIR_CONTEXTO);
    agregar_a_paquete(paquete_contexto,&(cpu.pid),sizeof(int));
    agregar_a_paquete(paquete_contexto,&(cpu.pc), sizeof(uint32_t));
    agregar_a_paquete(paquete_contexto,&(cpu.ax), sizeof(uint8_t));
    agregar_a_paquete(paquete_contexto,&(cpu.bx), sizeof(uint8_t));
    agregar_a_paquete(paquete_contexto,&(cpu.cx), sizeof(uint8_t));
    agregar_a_paquete(paquete_contexto,&(cpu.dx), sizeof(uint8_t));
    agregar_a_paquete(paquete_contexto,&(cpu.eax), sizeof(uint32_t));
    agregar_a_paquete(paquete_contexto,&(cpu.ebx), sizeof(uint32_t));
    agregar_a_paquete(paquete_contexto,&(cpu.ecx), sizeof(uint32_t));
    agregar_a_paquete(paquete_contexto,&(cpu.edx), sizeof(uint32_t));
    agregar_a_paquete(paquete_contexto,&(cpu.si), sizeof(uint32_t));
    agregar_a_paquete(paquete_contexto,&(cpu.di), sizeof(uint32_t));
    return paquete_contexto;

}

void push_contexto() { //TODO validar si cpu deberia encargarse de que esto no falle 
    
    log_debug(log_cpu, "Enviando el contexto a Kernel Memory");
    t_paquete * paquete_contexto = empaquetar_contexto();
    enviar_paquete_send_all(paquete_contexto, conexion_km);
    eliminar_paquete(paquete_contexto);

}

void limpiar_contexto(){

    cpu.pid = 0;
    cpu.pc = 0;
    cpu.ax = 0;
    cpu.bx = 0;
    cpu.cx = 0;
    cpu.dx = 0;
    cpu.eax = 0;
    cpu.ebx = 0;
    cpu.ecx = 0;
    cpu.edx = 0;
    cpu.si = 0;
    cpu.di = 0;
    cpu.tabla_segmentos = NULL;
    //cpu.tabla_segmentos = *(t_list**)list_get(paquete_contexto, 12);

}

int pull_contexto(pid_t pid_a_pedir) {

    op_code operacion = ENVIAR_CONTEXTO;

    send(conexion_km, &operacion, sizeof(op_code), 0);
    send(conexion_km, &pid_a_pedir, sizeof(pid_t), 0);

    op_code resp_km = recibir_operacion(conexion_km);

    if (resp_km == -1) {
        log_error(log_cpu, "pull_contexto: Error Kernel Memory se desconectó al pedir contexto.");
        error_fatal = true;
        return -1;
    }

    if (resp_km != ENVIAR_CONTEXTO) {
        log_error(log_cpu, "pull_contexto: Error KM devolvió operacion %d en vez de ENVIAR_CONTEXTO", resp_km);
        error_fatal = true;
        return -1;
    }

    t_list* paquete_contexto = recibir_paquete(conexion_km);

    if (paquete_contexto == NULL) {
        log_error(log_cpu, "pull_contexto: Error al recibir el buffer del contexto de Kernel Memory");
        error_fatal = true;
        return -1;
    }

    integrar_contexto(paquete_contexto);

    list_destroy_and_destroy_elements(paquete_contexto, free);

    if(pid_a_pedir != cpu.pid){
        log_error(log_cpu, "El contexto recibido e integrado no corresponde al pid solicitado !");
        error_fatal = true;
        return -1;
    }

    log_debug(log_cpu, "pull_contexto: El contexto fue recibido e integrado correctamente. ");

    return 0;

}
