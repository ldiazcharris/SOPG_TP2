#include "SerialManager.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

static int s;

int serial_open(int pn,int baudrate)
{
   	struct sockaddr_in serveraddr,addr2;
   	int clnt_addr_size;
	char buf[128];
	s = socket(PF_INET,SOCK_STREAM, 0);
	int flags = fcntl(s, F_GETFL);
	fcntl(s, F_SETFL, flags | O_NONBLOCK);
    	bzero((char *) &serveraddr, sizeof(serveraddr));
    	serveraddr.sin_family = AF_INET;
    	serveraddr.sin_port = htons(4040);
    	if(inet_pton(AF_INET, "127.0.0.1", &(serveraddr.sin_addr))<=0)
    	{
        	fprintf(stderr,"ERROR invalid server IP\r\n");
        	return -1;
    	}	

	while(1)
	{	
		printf("conectando a emulador...\n");
		int connectRes= connect(s, (const struct sockaddr *)&serveraddr, sizeof(serveraddr));
		printf("connectRes:%d\n",connectRes);
		if(connectRes>=0)
		{
			usleep(100000);
			break;
		}
		sleep(1);
	}
	printf("Emulador conectado\n");
    	return 0;
}


void serial_send(char* pData,int size)
{
	write(s, pData, size);
}

void serial_close(void)
{
	close(s);
}

int serial_receive(char* buf,int size)
{
	int n = read(s, buf, size);

	return n;
}

