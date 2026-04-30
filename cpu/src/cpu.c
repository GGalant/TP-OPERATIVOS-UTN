#include "cpu.h"

int main(int argc, char *argv[]){

    if(argc<3){
        perror("No se proporcionaron todos los argumentos [Archivo Config] [Identificador]");
        return EXIT_FAILURE;
    }

    char* config_path = argv[1]; 
    char* id_cpu_str = argv[2];
    int id_cpu = atoi(id_cpu_str);
    char* str_log_level;
    char* ip_ks;
    char* puerto_ks;
    char* ip_km;
    char* puerto_km;
    char* ip_ms;
    char* puerto_ms;
    int log_level;
    t_config* config = config_create(config_path);
    pthread_t hilo_ks;

    if(config == NULL){
        return EXIT_FAILURE;
    }

    str_log_level = config_get_string_value(config, "LOG_LEVEL");
    log_level = log_level_from_string(str_log_level);

    ip_ks = config_get_string_value(config, "IP_KS");
    puerto_ks = config_get_string_value(config, "PUERTO_KS");

    ip_km = config_get_string_value(config, "IP_KM");
    puerto_km = config_get_string_value(config, "PUERTO_KM");

    ip_ms= config_get_string_value(config, "IP_MS");
    puerto_ms = config_get_string_value(config, "PUERTO_MS");



    log_cpu = log_create(("cpu_%d.log", id_cpu), ("CPU <%d>", id_cpu), 1, log_level);
    log_info(log_cpu, "Iniciando CPU <%d>", id_cpu);

    //conexiones con modulos

    conexion_ks = crear_conexion(ip_ks, puerto_ks);

    if(conexion_ks == -1){
        log_error(log_cpu, "Error al conectar con Kernel Scheduler en %s:%s", ip_ks, puerto_ks);
        log_destroy(log_cpu);
        config_destroy(config);
        return EXIT_FAILURE;
    }

    handshake handshake_cpu_ks = HANDSHAKE_CPU;
    send(conexion_ks, &handshake_cpu_ks, sizeof(handshake), 0);

    pthread_create(&hilo_ks, NULL, handle_ks, (void*)(int)conexion_ks);
	

    conexion_km = crear_conexion(ip_km, puerto_km);

    if(conexion_km == -1){
        log_error(log_cpu, "Error al conectar con Kernel Memory en %s:%s", ip_km, puerto_km);
        log_destroy(log_cpu);
        config_destroy(config);
        return EXIT_FAILURE;
    }

    handshake handshake_cpu_km = HANDSHAKE_CPU;
    send(conexion_km, &handshake_cpu_km, sizeof(handshake), 0);

    conexion_ms = crear_conexion(ip_ms, puerto_ms);

    if(conexion_ms == -1){
        log_error(log_cpu, "Error al conectar con Memory Stick en %s:%s", ip_ms, puerto_ms);
        log_destroy(log_cpu);
        config_destroy(config);
        return EXIT_FAILURE;
    }

    handshake handshake_cpu_ms = HANDSHAKE_CPU;
    send(conexion_ms, &handshake_cpu_ms, sizeof(handshake), 0);
    log_info(log_cpu, "Conectado a Memory Stick");

    while(true) {   
        op_code codigo_operacion = recibir_operacion(conexion_km);

        //* 1. Esperar PID de Kernel Scheduler
        //* 2. Solicitar Context al Memory Kernel con el PID
        //* 3. Recibir paquete del Memory Kernel
        //* 4. Deserializar paquete y popular los registros
        //* 5. Iniciar ciclo de instruccion

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

void* handle_ks(void* arg) {    
    int socket_ks = (int)(void*)arg;
    
    log_info(log_cpu, "Conectado a kernel scheduler");

    while(true) {   
        // aca iria la logica de operaciones con Kernel Schedule
        int codigo_operacion = recibir_operacion(socket_ks);

        switch(codigo_operacion) {
            case -1:
                log_error(log_cpu, "El servidor Kernel Scheduler se desconectó. Terminando hilo.");
                return NULL;
            default:
                log_warning(log_cpu, "operacion desconocida: %d", codigo_operacion);
                break;
        }
    }
}

/*
Envia un pedido de instruccion a Kernel Memory y la carga en el buffer de instruccion del CPU
Devuelve -1 ante error, o el buffer estatico ante ejecucion exitosa.
*/
int* fetch(){

    log_info(log_cpu, "## PID: %u - FETCH - Program Counter: %u", cpu.pid, cpu.pc);

    t_paquete* paquete = crear_paquete(PEDIDO_INSTRUCCION);
    agregar_a_paquete(paquete, &cpu.pid, sizeof(uint32_t));
    agregar_a_paquete(paquete, &cpu.pc, sizeof(uint32_t));

    enviar_paquete_send_all(paquete, conexion_km);
    eliminar_paquete(paquete);

    op_code operacion = recibir_operacion(conexion_km);
    
    if(operacion == -1){
        log_error(log_cpu, "Fetch: Error al recibir_operacion");
        return -1;
    }

    if(operacion != RTA_PEDIDO_INSTRUCCION){
        log_error(log_cpu, "Fetch: La operacion recibida de Kernel Memory no fue la esperada, se recibio: %d", operacion);
        return -1;
    }

    int size;
    void* temp = recibir_buffer(&size, conexion_km);

    if(temp == NULL){
        log_error(log_cpu, "Fetch: El buffer recibido de KM es NULL");
        return -1;
    }

    if(size >= MAX_BUFFER_INSTRUCCION){
        log_error(log_cpu, "Fetch: La instruccion recibida de KM supera el tamanio maximo para una instruccion");
        return -1;
    }

    memcpy(instruccion_buffer, temp, size);
    instruccion_buffer[size] = '\0';

    free(temp);
    log_debug(log_cpu, "Fetch: Finalizado exitosamente");

    return instruccion_buffer;
    
};
