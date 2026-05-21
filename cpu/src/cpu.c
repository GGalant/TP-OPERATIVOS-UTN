#include "cpu.h"

int main(int argc, char *argv[]){

    if(argc<3){
        perror("No se proporcionaron todos los argumentos [Archivo Config] [Identificador]");
        return EXIT_FAILURE;
    }

    char* config_path = argv[1]; 
    char* id_cpu_str = argv[2];
    int id_cpu = atoi(id_cpu_str);
    int log_level;

    t_config* config = config_create(config_path);

    if(config == NULL){
        return EXIT_FAILURE;
    }

    map_config_variables(config);

    log_level = log_level_from_string(env_str_log_level);
    log_cpu = log_create(("cpu_%d.log", id_cpu), ("CPU <%d>", id_cpu), 1, log_level);
    log_info(log_cpu, "Iniciando CPU <%d>", id_cpu);

    //conexiones con modulos
    conexion_ks = conectar_modulo_logueando(env_ip_ks, env_puerto_ks, "Kernel Scheduler");
    conexion_km = conectar_modulo_logueando(env_ip_km, env_puerto_km, "Kernel Memory");
    conexion_ms = conectar_modulo_logueando(env_ip_ms, env_puerto_ms, "Memory Stick");

    if(conexion_ks == -1 || conexion_km == -1 || conexion_ms == -1 ){
        config_destroy(config);
        log_destroy(log_cpu);
        return EXIT_FAILURE;
    }
    
    //Envio de handshakes
    handshake handshake_cpu = HANDSHAKE_CPU;
    
    send(conexion_ks, &handshake_cpu, sizeof(handshake), 0);
    send(conexion_ks, &id_cpu,        sizeof(handshake), 0);

    send(conexion_km, &handshake_cpu, sizeof(handshake), 0);
    
    send(conexion_ms, &handshake_cpu, sizeof(handshake), 0);

    while(true) {

        op_code codigo_operacion = recibir_operacion(conexion_ks);

        switch(codigo_operacion) {  
            case -1:
                log_error(log_cpu, "El servidor Kernel Memory se desconectó.");
                return EXIT_FAILURE;  
            default:
                log_warning(log_cpu, "operacion desconocida: %d", codigo_operacion);
                break;
        }
    }
    
    log_destroy(log_cpu);
    config_destroy(config);
    
    return 0;
}

/*
Envia un pedido de instruccion a Kernel Memory y la carga en el buffer de instruccion del CPU
Devuelve NULL ante error, o el buffer estatico (instruccion_buffer) ante ejecucion exitosa.
*/
void* fetch(){

    log_info(log_cpu, "## PID: %u - FETCH - Program Counter: %u", cpu.pid, cpu.pc);

    t_paquete* paquete = crear_paquete(PEDIDO_INSTRUCCION);
    agregar_a_paquete(paquete, &cpu.pid, sizeof(uint32_t));
    agregar_a_paquete(paquete, &cpu.pc, sizeof(uint32_t));

    enviar_paquete_send_all(paquete, conexion_km);
    eliminar_paquete(paquete);

    op_code operacion = recibir_operacion(conexion_km);
    
    if(operacion == -1){
        log_error(log_cpu, "Fetch: Error al recibir_operacion");
        return NULL;
    }

    if(operacion != RTA_PEDIDO_INSTRUCCION){
        log_error(log_cpu, "Fetch: La operacion recibida de Kernel Memory no fue la esperada, se recibio: %d", operacion);
        return NULL;
    }

    int size;
    void* temp = recibir_buffer(&size, conexion_km);

    if(temp == NULL){
        log_error(log_cpu, "Fetch: El buffer recibido de KM es NULL o el tamanio es invalido");
        return NULL;
    }

    if(size <= 0){
        log_error(log_cpu, "Fetch: El size obtenido es invalido");
    }

    if(size >= MAX_BUFFER_INSTRUCCION){
        log_error(log_cpu, "Fetch: La instruccion recibida de KM supera el tamanio maximo para una instruccion");
        return NULL;
    }

    memcpy(instruccion_buffer, temp, size);
    instruccion_buffer[size] = '\0';

    free(temp);
    log_debug(log_cpu, "Fetch: Finalizado exitosamente");

    return instruccion_buffer;
    
};

/*
Lee del Buffer Estatico de Instruccion (instruccion_buffer) y escribe la instruccion decodificada en el struct estatico instruccion_actual
el icode de la instruccion puede ser ERROR.
*/
void decode(){

    log_debug(log_cpu, "Decode: Iniciando decodificacion");

    char ** splitted_instruction = string_split(instruccion_buffer, " ");

    if (splitted_instruction[0] == NULL) {
        instruccion_actual.icode = ERROR;
        string_array_destroy(splitted_instruction);
        return;
    }

    instruccion_actual.icode = string_to_instruction_code(splitted_instruction[0]);

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

void* execute(){
    switch(instruccion_actual.icode){
    case NOOP:
    break;
    case SET:
    break;
    case MOV_IN:
    break;
    case MOV_OUT:
    break;
    case SUM:
    break;
    case SUB:
    break;
    case JNZ:
    break;
    case COPY_MEM:
    break;
    case MUTEX_CREATE:
    break;
    case MUTEX_LOCK:
    break;
    case MUTEX_UNLOCK:
    break;
    case MEM_ALLOC:
    break;
    case MEM_FREE:
    break;
    case SLEEP:
    break;
    case STDOUT:
    break;
    case STDIN:
    break;
    case INIT_PROC:
    break;
    case EXIT:
    break;
    case ERROR:
    break;
    }

};

void* check_interrupt(){

};



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

    return ERROR;

}

/*
Devuelve un puntero al registro de la CPU que coincida con la string o NULL en caso default.
*/
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


//Conexion con modulo + log,
int conectar_modulo_logueando(char* ip, char* puerto, char* nombre_modulo) {
    int socket_conexion = crear_conexion(ip, puerto);

    if(socket_conexion == -1) {
        log_error(log_cpu, "Error al conectar con %s en %s:%s", nombre_modulo, ip, puerto);
        return -1; // Retornamos -1 para que el main sepa que falló
    }

    log_info(log_cpu, "Conectado exitosamente a %s", nombre_modulo);

    return socket_conexion;

}

//Mapea las variables del config a las variables delfinidas en el header
int map_config_variables(t_config* config){

    env_str_log_level = config_get_string_value(config, "LOG_LEVEL");

    env_ip_ks = config_get_string_value(config, "IP_KS");
    env_puerto_ks = config_get_string_value(config, "PUERTO_KS");

    env_ip_km = config_get_string_value(config, "IP_KM");
    env_puerto_km = config_get_string_value(config, "PUERTO_KM");

    env_ip_ms= config_get_string_value(config, "IP_MS");
    env_puerto_ms = config_get_string_value(config, "PUERTO_MS");

    return 0;
}
