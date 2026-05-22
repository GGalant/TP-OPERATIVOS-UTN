#ifndef IO_H_
#define IO_H_

#include <utils/serializacion.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
#include "globales.h"
#include "cpu.h"
#include "planificacion.h"

void* handle_io(void* arg);

#endif /* IO_H_ */