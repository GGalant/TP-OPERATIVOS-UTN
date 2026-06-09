#ifndef IO_H_
#define IO_H_

/*
* Se reemplazan los includes para evitar la redefinicion de enums y structs
* que se da por la falta de include guards en los headers de utils.
* Al incluir solo kernel_scheduler.h, que ya tiene los demas includes y si tiene
* include guards, se soluciona el problema.
* Tambien se define tipo_io localmente para evitar incluir el header ../../io/src/io.h
* que contiene errores de declaracion.
*/
#include "kernel_scheduler.h"
typedef enum {
    SLEEP, STDIN, STDOUT
} tipo_io;

typedef enum {  
    LIBRE,
    OCUPADA
} t_estado_io;

typedef struct {  
    int socket_io;
    int io_id;
    pid_t pid_actual; // PID del proceso que esta utilizando la IO, -1 si no hay ninguno
    tipo_io tipo;
    t_estado_io estado;
} t_io;

void* handle_io(void* arg);
void finalizar_io(pid_t pid);
void handle_io_sleep(t_io *io);
void handle_io_stdout(t_io *io);
t_io* crear_io(int socket_io);
bool esperar_y_procesar_respuesta_io(t_io* io, pid_t pid_esperado);

#endif /* IO_H_ */