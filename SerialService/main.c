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

// Variable para guardar las tramas provenientes del puerto serie
char trama_serial[20];

// Variable para guardar las tramas provenientes de la pagina web
char trama_tcp[20];

// File descriptor para la conexión TCP con InterfaceService
int fd_tcp_is;

// Se crean los hilos del módulo
pthread_t thread_1_th;

// Declaración de la función que manejará el thread_1_th
void *thread_1(void *args);

/**
 * @brief Handler de signal SIGINT y SIGTERM
 *
 * Aquí se indica la secuencia de terminación de los threads
 *
 * @param sig
 */
void close_conection(int sig)
{
	write(0, "Pedido de cierre de conexion\n\r", 30);
	// Se cirrea la conexion con cliente
    close(fd_tcp_is);
	// Se cirrea la conexión serie con el Emulador
	serial_close();
	// Se cancela el thread_1
	pthread_cancel(thread_1_th);
	// Se espera a que thread_1 retorne
	pthread_join(thread_1_th, NULL);
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

/**
 * @brief Hilo que se encargará de realizar la conexión con Emulador.py
 * 
 * @param args 
 * @return void* 
 */
void *thread_1(void * args)
{
	//printf("Debug: Entré a thread_1");
	while(true)
	{
		int bytes;
		bytes = serial_receive(trama_serial, 20);
		
		if(0 < bytes)
		{
			trama_serial[bytes] = 0;
			printf("Recibi del serial-port: %s\n\r", trama_serial);

			if(-1 == send(fd_tcp_is, trama_serial, strlen(trama_serial), 0))
			{
				perror("Error enviando a InterfaceService\n\r");
			}
		}
		else if(-1 == bytes)
		{
			perror("Error leyendo mensaje en serial port");
		}

		sleep(2);
	}
	return NULL;
}


/**
 * @brief 	Hilo principal que se encargará de lanzar los hilos secundarios
 * Además se encargará de la conexión TCP con InterfaceService
 * @return int 
 */
int main(void)
{
	// Se inicia el módulo SerialService
	printf("Inicio Serial Service\r\n");

	// Se crea la estructura de monitoreo de signals para SIGINT
	struct sigaction sigint_a;
	sigint_a.sa_handler = close_conection;
	sigint_a.sa_flags = 0;
	sigemptyset(&sigint_a.sa_mask);
	if(-1 == sigaction(SIGINT, &sigint_a, NULL))
	{
		perror("Error: Sigint\n\r");
		exit(1);
	}

	// Se crea la structura de monitoreo de signals para SIGTERM
	struct sigaction sigterm_a;
	sigterm_a.sa_handler = close_conection;
	sigterm_a.sa_flags = 0;
	sigemptyset(&sigterm_a.sa_mask);
	if(-1 == sigaction(SIGTERM, &sigterm_a, NULL))
	{
		perror("Error: Sigterm\n\r");
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
		pthread_create(&thread_1_th, NULL, thread_1, NULL);
		printf("thread_1 lanzado\n\r");
	}
	else
	{
		printf("Error abriendo puerto serie.\n\rFalló comunicación con emulador\r\n");
	}

	// Se libera el manejo de las signals
	release_sign();

	// Se inicia la configuración para la comunicación TCP
	// con el múdulo InterfaceService
	socklen_t addr_len;
	struct sockaddr_in clientaddr;
	struct sockaddr_in serveraddr;

	// Se crea el socket TCP
	int s = socket(PF_INET, SOCK_STREAM, 0);

	// Se cargan los datos de IP:PORT del server en la estructura serveraddr
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(10000);
    serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if(INADDR_NONE == serveraddr.sin_addr.s_addr)
    {
        fprintf(stderr,"ERROR servidor IP invalido\r\n");
        return 1;
    }

	// Se abre el puerto 
	if (-1 == bind(s, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) ) {
		close(s);
		perror("listener: bind\n\r");
		return 1;
	}

	// Se configura el socket en modo Listening
	if (listen (s, 10) == -1) // backlog=10
  	{
    	perror("Error en listen\n\r");
    	exit(1);
  	}

	// Número de bytes de la conexión con InterfaceService
	static int n;
	
	// Aceptación contínua de conexiones
	// Si un cliente que estuvo conectado y se desconecta,
	// el programa podrá recibir otra conexión. 
	while(true)
	{
		// Se acpetan conexiones entrantes continuamente.
		// Se pudo lanzar un thread por cada conexión entrante,
		// pero debido a la naturaleza de la aplicación,
		// me pareció más seguro limitar la generación a dos 
		// conexiones a saber: InterfaceService y EmuladorHardware.
		addr_len = sizeof(struct sockaddr_in);
    	if (-1 == (fd_tcp_is = accept(s, (struct sockaddr *)&clientaddr, &addr_len)))
      	{
		    perror("error en accept\n\r");
		    exit(1);
	    }
	 	printf  ("server:  conexion desde:  %s\n\r", inet_ntoa(clientaddr.sin_addr));
		
		// Lectura continua de datos desde el cliente
		while(true)
		{
			// Se lee el mensaje de cliente
			n = recv(fd_tcp_is, trama_tcp, 20, 0);

			//Si se lee algo es recibido, se re-transmite a EmuladorHardware
			if( 0 < n)
			{
				trama_tcp[20] = 0;
				printf("Recibi %d bytes.:%s\n\r", n, trama_tcp);

				// Se envía la trama recibida a EmuladorHardware
				serial_send(trama_tcp, strlen(trama_tcp));
			}
			else if (-1 == n)
			{
				perror("Error leyendo mensaje en socket\n\r");
				
			}
		}
	}

	
	return 0;
}
