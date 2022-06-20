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

// Se inicia la configuración para la comunicación TCP
// con el múdulo InterfaceService

socklen_t addr_len;
struct sockaddr_in clientaddr;
struct sockaddr_in serveraddr;
char buffer[128];
int newfd;



//Variable local tipo Mutex 
pthread_mutex_t	mutexData = PTHREAD_MUTEX_INITIALIZER;

//Variable comparitda para guardar las tramas
char trama[10];

/**
 * @brief Handler de signal SIGINT
 *
 * Aquí se puede colocar la secuencia de terminación de los threads
 *
 * @param sig
 */
void sig_int_handler(int sig)
{
	write(0, "Pedido de cierre de conexion\n", 2);
	// Cerramos conexion con cliente
    close(newfd);
	// Cerramos la conexión serie con el Emulador
	serial_close();
	//Cancelamos el thread_1
	pthread_cancel(thread_1);
	//Esperamos que thread_1 retorne
	pthread_join(thread_1, NULL);
	//Salimos del hilo principal
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
	write(0, "Pedido de cierre de conexion\n", 15);
	// Cerramos conexion con cliente
    close(newfd);
	// Cerramos la conexión serie con el Emulador
	serial_close();
	//Cancelamos el thread_1
	pthread_cancel(thread_1);
	//Esperamos que thread_1 retorne
	pthread_join(thread_1, NULL);
	//Salimos del hilo principal
	exit(EXIT_SUCCESS);
}

// Función para bloquear el manejo de las señales
void block_sign(void)
{
	sigset_t set;
	int s;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	// sigaddset(&set, SIGUSR1);
	pthread_sigmask(SIG_BLOCK, &set, NULL);
}

// Función para liberar el manejo las señales
void release_sign(void)
{
	sigset_t set;
	int s;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	// sigaddset(&set, SIGUSR1);
	pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}

// Hilo que se encargará de realizar la conexión con Emulador.py
void thread_1(void)
{
	
	if(serial_open(1, 115200) != 0)
	{
		printf("Error abriendo puerto serie\r\n");
	}

	while(true)
	{
		int bytes = 0;
		pthread_mutex_lock(&mutexData);
		if((bytes = serial_receive(trama, strlen(trama))) == -1)
		{
			perror("Error leyendo mensaje en socket");
			exit(1);
		}
		thread_mutex_unlock(&mutexData);
	}
}

int main(void)
{
	/**
	 * @brief Se inicia el módulo SerialService
	 *
	 */
	printf("Inicio Serial Service\r\n");

	/**
	 * @brief Se crea la estructura de monitoreo de signals para
	 * la signal ctrl+c
	 *
	 */
	struct sigaction sa;
	sa.sa_handler = sig_int_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	if(sigaction(SIGINT, &sa, NULL) == -1)
	{
		perror("Error: Sigaction");
		exit(1);
	}

	/**
	 * @brief Se crea la structura de monitoreo de signals para
	 * la signal SIGTERM
	 */
	struct sigaction sa;
	sa.sa_handler = sig_term_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	if(sigaction(SIGTERM, &sa, NULL) == -1)
	{
		perror("Error: Sigterm");
		exit(1);
	}

	// Se bloquea el manejo de las signals
	block_sign();

	/**
	 * @brief Se crean y se lanzan los hilos del módulo
	 *
	 */
	pthread_t hilo_1;

	pthread_create(&hilo_1, NULL, thread_1, NULL);

	// Se libera el manejo de las signals
	release_sign();

	
	// Se crea el socket
	int s = socket(PF_INET,SOCK_STREAM, 0);

	// Se cargan los datos de IP:PORT del server
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(4096);
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
    	perror("error en listen");
    	exit(1);
  	}

	// Número de bytes de la conexión con InterfaceService
	static int n;

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

		

		while(true)
		{
			
			// Leemos mensaje de cliente
			if( (n = recv(newfd, buffer,128, 0)) == -1 )
			{
				perror("Error leyendo mensaje en socket");
				exit(1);
			}
			buffer[n]=0;
			printf("Recibi %d bytes.:%s\n",n,buffer);

			/**
			 * @brief Aquí hay que definir las condiciones en las cuales 
			 * se debe enviar un mensaje a InterfaceService
			 */
			// Enviamos mensaje a cliente
			if (send(newfd, "hola", 5, 0) == -1)
			{
				perror("Error escribiendo mensaje en socket");
				exit (1);
    		}
	
		}
	}

	
	return 0;
}
