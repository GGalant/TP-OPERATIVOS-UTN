#ifndef IO_H_
#define IO_H_

#include <utils/serializacion.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
#include "globales.h"
#include "cpu.h"
#include "planificacion.h"
#include "../../io/src/io.h"

typedef enum {  
  LIBRE,
  OCUPADA
} t_estado_io;

typedef struct {  
  int socket_io;
  int io_id;
  tipo_io tipo;
  t_estado_io estado;
} t_io;

void* handle_io(void* arg);

#endif /* IO_H_ */