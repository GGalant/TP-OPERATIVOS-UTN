#include "utils/conexiones.h"

int crear_conexion(char *ip, char* puerto){
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);

	// crear el socket.
	int socket_cliente = socket(server_info->ai_family,
                         server_info->ai_socktype,
                         server_info->ai_protocol);

	if (socket_cliente == -1) {
		perror("Error al crear el socket");
		return -1;
	}

	// se conecta al servidor
	int conexion = connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen);

	if (conexion == -1) {
	    perror("Error al conectar");
	    pclose(socket_cliente);
		return -1;
	}

	freeaddrinfo(server_info);

	return socket_cliente;
}

int iniciar_servidor(t_log *logger, char *puerto)
{
	int socket_servidor;
    int err;
	struct addrinfo hints, *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, puerto, &hints, &server_info);

	// Creamos el socket de escucha del servidor
	socket_servidor = socket(server_info->ai_family,
							 server_info->ai_socktype,
							 server_info->ai_protocol);
    if(socket_servidor == -1){
        return -1;
    }

    //Configuramos socket optionns (reusar)
	int opt = 1;
	err = setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if(err == -1){
        return -1;
    }
    
    // Asociamos el socket a un puerto
    err = bind(socket_servidor, server_info->ai_addr, server_info->ai_addrlen);
    if(err == -1){
        return -1;
    }

	// Escuchamos las conexiones entrantes
	err = listen(socket_servidor, SOMAXCONN);
    if(err == -1){
        return -1;
    }

	freeaddrinfo(server_info);
	log_trace(logger, "Listo para escuchar a mi cliente");

	return socket_servidor;
}

int esperar_cliente(int socket_servidor, t_log *logger)
{

	// Aceptamos un nuevo cliente
	int socket_cliente = accept(socket_servidor, NULL, NULL);

	return socket_cliente;
} 