#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include "SerialManager.h"

char buffer[128];
int newfd;
	
// Se crean los hilos del módulo
pthread_t hilo_1;
void *thread_1(void *args);

// Variable local tipo Mutex para controlar acceso concurrente
pthread_mutex_t	mutexData = PTHREAD_MUTEX_INITIALIZER;

// Variable comparitda para guardar las tramas
char trama[10];

/**
 * @brief Handler de signal SIGINT
 *
 * Aquí se indica la secuencia de terminación de los threads
 *
 * @param sig
 */
void sig_int_handler(int sig)
{
	write(0, "Pedido de cierre de conexion\n", 2);
	// Se cirrea la conexion con cliente
    close(newfd);
	// Se cirrea la conexión serie con el Emulador
	serial_close();
	// Se cancela el thread_1
	pthread_cancel(hilo_1);
	// Se espera a que thread_1 retorne
	pthread_join(hilo_1, NULL);
	// Salida del hilo principal
	exit(EXIT_SUCCESS);
}

/**
 * @brief Handler de signal SIGTERM
 *
 * Aquí se puede colocar la secuencia de terminación de los threads
 *
 * @param sig
 */
void sig_term_handler(int sig)
{
	write(0, "Pedido de cierre de conexion\n", 2);
	// Se cirrea la conexion con cliente
    close(newfd);
	// Se cirrea la conexión serie con el Emulador
	serial_close();
	// Se cancela el thread_1
	pthread_cancel(hilo_1);
	// Se espera a que thread_1 retorne
	pthread_join(hilo_1, NULL);
	// Salida del hilo principal
	exit(EXIT_SUCCESS);
}

// Función para bloquear el manejo de las señales
void block_sign(void)
{
	sigset_t set;
	int s;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &set, NULL);
}

// Función para liberar el manejo las señales
void release_sign(void)
{
	sigset_t set;
	int s;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}

// Hilo que se encargará de realizar la conexión con Emulador.py
void *thread_1(void * args)
{
	printf("Debug: Entré a thread_1");
	while(true)
	{
		printf("Debug: Entré al while de thread_1");
		int bytes;
		bytes = serial_receive(trama, strlen(trama));
		//pthread_mutex_lock(&mutexData);
		if(0 < bytes)
		{
			printf("Recibi del serial-port: %s\n\r", trama);

			if(-1 == send(newfd, trama, strlen(trama), 0))
			{
				perror("Error enviando a InterfaceService");
				//exit(1);
			}
		}
		else if(0 == bytes)
		{
			perror("Error leyendo mensaje en serial port");
		}

		
		//thread_mutex_unlock(&mutexData);
		sleep(2);
	}
	return NULL;
}


// Hilo principal que se encargará de lanzar los hilos secundarios
// Además se encargará de la conexión TCP con InterfaceService
int main(void)
{
	// Se inicia el módulo SerialService
	printf("Inicio Serial Service\r\n");

	// Se crea la estructura de monitoreo de signals para
	struct sigaction sigint_a;
	sigint_a.sa_handler = sig_int_handler;
	sigint_a.sa_flags = 0;
	sigemptyset(&sigint_a.sa_mask);
	if(sigaction(SIGINT, &sigint_a, NULL) == -1)
	{
		perror("Error: Sigint");
		exit(1);
	}

	// Se crea la structura de monitoreo de signals para
	struct sigaction sigterm_a;
	sigterm_a.sa_handler = sig_term_handler;
	sigterm_a.sa_flags = 0;
	sigemptyset(&sigterm_a.sa_mask);
	if(sigaction(SIGTERM, &sigterm_a, NULL) == -1)
	{
		perror("Error: Sigterm");
		exit(1);
	}

	// Se bloquea el manejo de las signals para que
	// el hilo nuevo no herede el manejo de señales
	block_sign();

	// Si se logra establecer la conexión con el puerto serie
	// se lanza el thread_1() que ejecutará las comunicaciones 
	// con EmuladorHardware.
	if(0 == serial_open(1, 115200))
	{
		pthread_create(&hilo_1, NULL, thread_1, NULL);
		printf("Debug: se creó thread_1 sin problemas");
	}
	else
	{
		printf("Error abriendo puerto serie.\n\rFalló la comunicación con el hardware\r\n");
	}

	// Se libera el manejo de las signals
	release_sign();

	
	// Se inicia la configuración para la comunicación TCP
	// con el múdulo InterfaceService
	socklen_t addr_len;
	struct sockaddr_in clientaddr;
	struct sockaddr_in serveraddr;


	// Se crea el socket
	int s = socket(PF_INET,SOCK_STREAM, 0);

	// Se cargan los datos de IP:PORT del server con la estructura de
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(10000);
    serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if(INADDR_NONE == serveraddr.sin_addr.s_addr)
    {
        fprintf(stderr,"ERROR invalid server IP\r\n");
        return 1;
    }

	// Se abre el puerto 
	if (bind(s, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1) {
		close(s);
		perror("listener: bind");
		return 1;
	}

	// Se configura el socket en modo Listening
	if (listen (s, 10) == -1) // backlog=10
  	{
    	perror("Error en listen");
    	exit(1);
  	}

	// Número de bytes de la conexión con InterfaceService
	static int n;
	
	// Aceptación contínua de conexiones
	// Si un cliente que estuvo conectado se desconecta,
	// el programa podrá recibir otra conexión. 
	while(true)
	{
		// Se acpetan conexiones entrantes
		addr_len = sizeof(struct sockaddr_in);
    	if ( (newfd = accept(s, (struct sockaddr *)&clientaddr, &addr_len)) == -1)
      	{
		    perror("error en accept");
		    exit(1);
	    }
	 	printf  ("server:  conexion desde:  %s\n", inet_ntoa(clientaddr.sin_addr));

		// Lectura continua de datos desde el cliente

		while(true)
		{
			// Se lee el mensaje de cliente
			if( (n = recv(newfd, buffer,128, 0)) == -1 )
			{
				perror("Error leyendo mensaje en socket");
				exit(1);
			}
			buffer[n] = 0;
			printf("Recibi %d bytes.:%s\n",n,buffer);

			/**
			 * @brief Aquí hay que definir las condiciones en las cuales 
			 * se debe enviar un mensaje a InterfaceService
			 */

			serial_send(buffer, strlen(buffer));
			// Enviamos mensaje a cliente
			/*
			if (send(newfd, "hola", 5, 0) == -1)
			{
				perror("Error escribiendo mensaje en socket");
				exit (1);
    		}
			*/
		}
	}

	
	return 0;
}
