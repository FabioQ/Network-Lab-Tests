/* Sender.c  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "appdefs.h"
#include "Util.h"

#define PARAMETRIDEFAULT "./Sender.exe 127.0.0.1 6001"

void usage(void) 
{  printf ("usage: ./Sender.exe REMOTE_IP_NUMBER REMOTE_PORT_NUMBER\n"
				"esempio: " PARAMETRIDEFAULT "\n" );
}

MSG msg;

/*
int	AvailableSndBufferSpace(int s, int *pnum)
{
	if (ioctl (s,TIOCOUTQ,pnum)==0) 
		return(1);
	else {
		perror("fcntl TIOCOUTQ failed: ");
		exit(0);
		return(-1);
	}
}
*/

int main(int argc, char *argv[])
{
	char string_remote_ip_address[100];
	short int remote_port_number;
	int socketfd, ris;
	int n, nwrite, nread;
	unsigned long int i;

	struct timeval now, prima, dopo, tempoimpiegato;

	if	( (sizeof(int)!=sizeof(long int)) ||  (sizeof(int)!=4)  )
	{
		printf ("dimensione di int e/o long int != 4  -> TERMINO\n");
		fflush(stdout);
		exit(1);
	}

	if(argc==1) { 
		printf ("uso i parametri di default \n%s\n", PARAMETRIDEFAULT );
		strncpy(string_remote_ip_address, "127.0.0.1", 99);
		remote_port_number = 6001;
	}
	else if(argc!=3) { printf ("necessari 2 parametri\n"); usage(); exit(1);  }
	else {
		strncpy(string_remote_ip_address, argv[1], 99);
		remote_port_number = atoi(argv[2]);
	}

	gettimeofday(&prima,NULL);

	ris= TCP_setup_connection(&socketfd, string_remote_ip_address, remote_port_number, 
				100000, 100000,1 );
	if (!ris)  {
		printf ("impossibile connettersi al server %s %d\nTERMINO!!\n", 
				string_remote_ip_address, remote_port_number );
		fflush(stdout);
		return(0);
	}
	printf ("dopo prima Connect()\n");
	fflush(stdout);

	ris=SetsockoptTCPNODELAY(socketfd,1);
	if (!ris)  exit(5);
	ris=GetsockoptTCPNODELAY(socketfd,&n);
	if (!ris)  exit(5);
	else printf ("TcpNodelay attivato? %d\n", n);

	ris=SetsockoptSndBuf(socketfd,100000);
	if (!ris)  exit(5);
	ris=GetsockoptSndBuf(socketfd,&n);
	if (!ris)  exit(5);
	else printf ("SndBuf %d\n", n);

	ris=SetsockoptRcvBuf(socketfd,100000);
	if (!ris)  exit(5);
	ris=GetsockoptRcvBuf(socketfd,&n);
	if (!ris)  exit(5);
	else printf ("RcvBuf %d\n", n);

	for(i=0;i<NUMMSGS;i++)
	{
		/*
		int numbytes;
		AvailableSndBufferSpace(socketfd, &numbytes);
		printf("snd buff: %d\n", numbytes );
		fflush(stdout);
		*/

		now.tv_sec=0;
		now.tv_usec=1000*DELAYMSEC;
		select(1,NULL,NULL,NULL,&now);

		memset(&msg,0,LENMSG);
		gettimeofday(&now,NULL);
		memcpy( &(msg.tv), &now, sizeof(now) );
		/*
		printf("%ld : %ld sec %ld usec\n", i, now.tv_sec, now.tv_usec );
		fflush(stdout);
		*/
		sprintf( msg.buf+sizeof(now), " %ld : %ld sec %ld usec\n", i, now.tv_sec, now.tv_usec );
		/* scrittura dell'ultimo byte dal client */
		nwrite=LENMSG;
		n = Writen (socketfd, &(msg), nwrite);
		if (n != nwrite)
		{
			printf ("Writen() failed \n");
			fflush(stdout);
			return (1);
		}
		printf(".");
		fflush(stdout);
	}
	/* lettura della chiusura dal receiver */
	nread=1;
	n = Readn (socketfd, (char*)&(msg.buf), nread);

	gettimeofday(&dopo,NULL);
	tempoimpiegato=differenza(dopo,prima);
	stampa_timeval("tempoimpiegato ", tempoimpiegato);

	if( n==0 )
	{
		printf ("Readn() read connection closed\n");
		fflush(stdout);
		close(socketfd);
		return (0);
	}
	else 
	{
		printf ("Readn() failed \n");
		fflush(stdout);
		close(socketfd);
		return (1);
	}
}

