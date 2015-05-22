/* ServSimple.c */



#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "appdefs.h"
#include "Util.h"

#define PARAMETRIDEFAULT "./Receiver.exe 9001"

void usage (void)
{
    printf ("usage: ./Receiver.exe LOCAL_PORT_NUMBER\n"
				"esempio: "  PARAMETRIDEFAULT "\n" );
}

struct timeval vt_sent[NUMMSGS];
struct timeval vt_recv[NUMMSGS];
int vt_delay[NUMMSGS];
MSG msg;

int main (int argc, char *argv[])
{
   	short int local_port_number;
   	int listenfd, connectedfd, ris, i, nread, n;
    struct sockaddr_in Cli;
	unsigned int len;
	struct timeval tempoimpiegato;
	long int msec;
	int totaleTempo;
	FILE *f;

	if    ( (sizeof(int)!=sizeof(long int)) ||  (sizeof(int)!=4)  )
	{
		printf ("dimensione di int e/o long int != 4  -> TERMINO\n");
		fflush(stdout);
		exit(1);
	}

    if(argc==1) { 
		printf ("uso i parametri di default \n%s\n", PARAMETRIDEFAULT );
		local_port_number = 9001;
	}
	else if (argc != 2)
	{	printf ("necessario 1 parametro\n");
		usage (); exit (1);
    }
    else
    {	local_port_number = atoi (argv[1]);    }
	
	init_random();

	ris=TCP_setup_socket_listening( &listenfd, local_port_number, 100000L, 100000L, 1);
	if (!ris)
	{	printf ("setup_socket_listening() failed\n");
		exit(1);
	}
	f=fopen("delay.txt","wt");
	if(f==NULL) { perror("fopen failed"); exit(1); }

	/* wait for connection request */
	do {
		memset (&Cli, 0, sizeof (Cli));
		len = sizeof (Cli);
		connectedfd = accept ( listenfd, (struct sockaddr *) &Cli, &len);
	} while ( (connectedfd<0) && (errno==EINTR) );

	if (connectedfd < 0 )
	{	perror("accept() failed: \n"); exit (1); }
	close(listenfd);

	ris=SetsockoptTCPNODELAY(connectedfd,1);
	if (!ris)  exit(5);
	ris=GetsockoptTCPNODELAY(connectedfd,&n);
	if (!ris)  exit(5);
	else printf ("TcpNodelay attivato? %d\n", n);

	ris=SetsockoptSndBuf(connectedfd,100000L);
	if (!ris)  exit(5);
	ris=GetsockoptSndBuf(connectedfd,&n);
	if (!ris)  exit(5);
	else printf ("SndBuf %d\n", n);

	ris=SetsockoptRcvBuf(connectedfd,100000L);
	if (!ris)  exit(5);
	ris=GetsockoptRcvBuf(connectedfd,&n);
	if (!ris)  exit(5);
	else printf ("RcvBuf %d\n", n);

	for(i=0;i<NUMMSGS;i++)
	{
		/* lettura del byte dal client */
		nread=LENMSG;
		n = Readn (connectedfd, (char*)&msg, nread );
		if (n != nread)
		{
			printf ("Readn() failed \n");
			fflush(stdout);
			return (0);
		}
		memcpy( &( vt_sent[i] ), &(msg.tv), sizeof(struct timeval) );
		gettimeofday(&( vt_recv[i] ),NULL);
		printf(".");
		fflush(stdout);

		/*
		printf("sent: %ld : %ld sec %ld usec\n", i, vt_sent[i].tv_sec, vt_sent[i].tv_usec );
		printf("recv: %ld : %ld sec %ld usec\n\n", i, vt_recv[i].tv_sec, vt_recv[i].tv_usec );
		*/
		tempoimpiegato=differenza(vt_recv[i],vt_sent[i]);
		msec=(tempoimpiegato.tv_sec*1000)+(tempoimpiegato.tv_usec/1000);
		vt_delay[i]=msec;
		/* stampa_timeval("tempoimpiegato\n", tempoimpiegato); */
		if(msec>500)
			printf("%d : delay msec %ld  TEMPO SUPERATO\n", i, msec);
		else
			printf("%d : delay msec %ld\n", i, msec);
		fprintf(f,"%d %ld\n", i, msec);

	}
	close(connectedfd);
	fclose(f);
	
	for(i=1;i<NUMMSGS;i++)
	{
		tempoimpiegato=differenza(vt_sent[i],vt_sent[i-1]);
		stampa_timeval("tempoimpiegato\n", tempoimpiegato);
		msec=(tempoimpiegato.tv_sec*1000)+(tempoimpiegato.tv_usec/1000);
		if(msec<=0) { printf("vaffa\n"); exit(2); }
	}

	totaleTempo=0;
	for(i=0;i<NUMMSGS;i++)
	{
		totaleTempo=totaleTempo+vt_delay[i];
	}
	printf("\nIl tempo totale Ritardo è stato %d con un ritardo MEDIO per pacchetto di %d",totaleTempo,((totaleTempo)/(i-1)));
	/* poi controllo se ogni pacchetto e' giunto non oltre due secondi dalla spedizione */
	
	for(i=1;i<NUMMSGS;i++)
	{
		tempoimpiegato=differenza(vt_recv[i],vt_sent[i]);
		msec=(tempoimpiegato.tv_sec*1000)+(tempoimpiegato.tv_usec/1000);
		stampa_timeval("tempoimpiegato\n", tempoimpiegato);
		printf("%d : delay msec %ld\n", i, msec);
		if(msec>2000) { printf("MAVAFFANCULOOOOOOOOOOO, TEMPO SUPERATO\n"); exit(2); }
	}
	
	
	printf ("Tutto OK\n");
	fflush(stdout);
	return (0);
}
