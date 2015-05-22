/* Ritardatore.c  */

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

#define MAXLENRCVMSG 10
#define MAXCASUALE   100000L
/*#define MAXDONOTHING  99970L*/
int MAXDONOTHING;
#define MAXDODELAY   100000L
#define RITARDOPKT 100000L
#define PERC_ERR 0.001

char string_remote_ip_address[MAXNUMCONNECTIONS][100];
/* short int remote_port_number[MAXNUMCONNECTIONS]; */
short int remote_port_number[MAXNUMCONNECTIONS];
int listenfd[MAXNUMCONNECTIONS];
int usalistenfd[MAXNUMCONNECTIONS];
pthread_mutex_t mutex[MAXNUMCONNECTIONS]; 
pthread_mutex_t mutex_blocca;
int stop[MAXNUMCONNECTIONS];
int status[MAXNUMCONNECTIONS];
int chiusi=0;
int numconndausare=0;
int Ricevuti[MAXNUMCONNECTIONS][2];
int printed=0;
long int numritardi=0;

int BloccaConnessione=0;

typedef struct {
	int clientfd;
	int indice;
} parametri;

typedef struct {
	int fd1;
	int fd2;
	int indice;
} param;

typedef struct nodo {
	char* data;
	int len;
	struct timeval istsend;
	struct nodo *next;
} NODO;


int interpreta(char *str, fd_set *all);

void sig_print(int signo)
{
	if(printed==0)
	{
		int i;
		printed=1;
		if(signo==SIGINT)		printf("SIGINT\n");
		else if(signo==SIGHUP)	printf("SIGHUP\n");
		else if(signo==SIGTERM)	printf("SIGTERM\n");
		else					printf("other signo\n");
		printf("\n");
		for(i=0;i<MAXNUMCONNECTIONS;i++)
		{
			printf("canale %d: sent %d received %d  tot %d\n",
					i, Ricevuti[i][0],Ricevuti[i][1], Ricevuti[i][0]+Ricevuti[i][1] );
		}
		fflush(stdout);
	}
	exit(0);
	return;
}

int iszero(struct timeval *t)
{
	if( (t->tv_sec==0) && (t->tv_usec==0) ) return(1);
	else return(0);
}

int	AvailableBytes(int s, int *pnum)
{
	if (ioctl (s,FIONREAD,pnum)==0) 
		return(1);
	else {
		perror("fcntl failed: ");
		exit(0);
		return(0);
	}
}


int leggispedisci(int fd1, int fd2, int indice, int offset)
{
	int ris,len;
	struct timeval delay;
	char data[MAXLENRCVMSG];

	do {
		ris=read(fd1, data,MAXLENRCVMSG);
	} while ( (ris<0) && (errno==EINTR) );
	if(ris<0) 
	{	perror("read failed: ");
		/*	close(fd2);	close(fd1);	*/
		return(-1);
	} 
	else if(ris==0) return(0);
	else
	{
		unsigned long int casuale, stato;

		pthread_mutex_lock( &mutex_blocca );
		if(BloccaConnessione==1)
		{
			BloccaConnessione=0;
			pthread_mutex_unlock( &mutex_blocca );

			/* per bloccare questo pthread e anche quello nella opposta direzione */
			pthread_mutex_lock( &(mutex[indice]) );
			printf("superata prima mutex di indice %d\n", indice);
			printf("spiacente, bloccata connessione di indice %d\n", indice);
			fflush(stdout);
			sleep(10000);
		}
		else
			pthread_mutex_unlock( &mutex_blocca );

		len=ris;
		casuale = random()%MAXCASUALE;

		pthread_mutex_lock( &(mutex[indice]) );
		stato=status[indice];
		/*
		if(stato==1) status[indice]=0;
		*/
		pthread_mutex_unlock( &(mutex[indice]) );

		if(stato!=0)
		{
			/* RITARDO una volta */
			delay.tv_sec =0;
			delay.tv_usec=500000; /* mezzo sec */
			printf("INTRODOTTO RITARDO SU canale indice %d offset %d\n", indice, offset );
			select(1,NULL,NULL,NULL,&delay);
		}
		else if( casuale <= MAXDONOTHING ) { ; }
		else if( casuale <= MAXDODELAY ) 
		{
			/* RITARDO */
			delay.tv_sec =0;
			delay.tv_usec=RITARDOPKT; /* 150 msec */
			numritardi++;
			printf("PRODUCO RITARDO %ld SU canale indice %d offset %d di %d us\n", numritardi, indice, offset,delay.tv_usec );
			fflush(stdout);
			select(1,NULL,NULL,NULL,&delay);
		}
		else /* QUASI BLOCCO */
		{
			/* RITARDO */
			delay.tv_sec =0;
			delay.tv_usec=10000000; /* 10 sec */
			printf("PRODUCO BLOCCO SU canale indice %d offset %d\n", indice, offset );
			fflush(stdout);
			select(1,NULL,NULL,NULL,&delay);
		}
		ris=Sendn(fd2,data,len);
		if(ris<0) 
		{	printf("leggispedisci: Sendn failed: \n");
			fflush(stdout);
			/*	close(fd2);	close(fd1);	*/
			return(-1);
		}

		pthread_mutex_lock( &(mutex[indice]) );
		Ricevuti[indice][offset]+=len;
		pthread_mutex_unlock( &(mutex[indice]) );

		
		return(1);
	}
}


/* mando da fd1 verso fd2 */
void* Simple_thread_monodirez(void *p)
{
	int fd1, fd2, indice, offset;
	struct timeval	istnextrecv;
	int ris;

	fd1=((param*)p)->fd1;
	fd2=((param*)p)->fd2;
	indice=((param*)p)->indice/2;
	offset=((param*)p)->indice%2;
	free(p);
	p=NULL;

	gettimeofday(&istnextrecv,NULL);
	normalizza(&istnextrecv);

	for(;;)
	{
		/* attendo l'arrivo di dati */
		ris=leggispedisci(fd1,fd2, indice,offset );
		if(ris==0) /* chiuso fd1 dall'altro end-system */
		{
			close(fd1);sleep(2);close(fd2);
			pthread_exit(NULL);
		}
		else if(ris<0)
		{
			printf("leggi failed: \n");
			/*	close(fd2);	close(fd1);	*/
			pthread_exit(NULL);
		}
		else
		{
			/*
			ris>0 tutto ok
			ho aggiornato istnextrecv
			dentro leggi
			*/
			continue;
		}
	}
	return(NULL);
}

void* thread_bidirez(void *p)
{
	int ris;
	int clientfd, indice, serverfd;
	pthread_attr_t attr;
	pthread_t thid;

	clientfd=((parametri*)p)->clientfd;
	indice=((parametri*)p)->indice;
	free(p);
	p=NULL;

	ris= TCP_setup_connection( &serverfd, string_remote_ip_address[indice], 
								remote_port_number[indice], /* SndBuf */ 65536, 
								/* RcvBuf */ 65536,   /* TCPNoDelay */ 1 );
	if (!ris)  {
		printf ("impossibile connettersi al server %s %d\nTERMINO pthread !!\n", 
				string_remote_ip_address[indice], remote_port_number[indice] );
		fflush(stdout);
		pthread_exit(NULL);
	}

#ifdef MYDEBUG
	{
		int n;
		ris=GetsockoptRcvBuf(serverfd,&n);
		if (!ris)  exit(5);
		else printf ("RcvBuf serverfd %d\n", n);
		ris=GetsockoptRcvBuf(clientfd,&n);
		if (!ris)  exit(5);
		else printf ("RcvBuf clientfd %d\n", n);
	}
	printf ("dopo indice %d Connect()\n", indice);
	fflush(stdout);
#endif

	/* definisco i parametri per una direzione */
	p=malloc(sizeof(param));
	if(p==NULL) { perror("malloc failed: "); exit(2); }
	((param*)p)->fd1=clientfd;
	((param*)p)->fd2=serverfd;
	((param*)p)->indice=2*indice;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	printf ("creatingh Simple_thread_monodirez fd1 %d fd2 %d indice %d\n", ((param*)p)->fd1,((param*)p)->fd2,((param*)p)->indice);
	ris=pthread_create( &thid, &attr, (ptr_thread_routine) &Simple_thread_monodirez, (void *) p );
	/* ris=pthread_create( &thid, &attr, (ptr_thread_routine) &Queue_thread_monodirez, (void *) p ); */
	if(ris)
	{	perror("pthread_create failed:");
		pthread_attr_destroy(&attr);
		exit(5);
	}
#ifdef MYDEBUG
	else
		printf ("OK: created Queue_thread_monodirez\n");
#endif

	/* definisco i parametri per l'altra direzione */
	p=malloc(sizeof(param));
	if(p==NULL) { perror("malloc failed: "); exit(2); }
	((param*)p)->fd1=serverfd;
	((param*)p)->fd2=clientfd;
	((param*)p)->indice=2*indice+1;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	/*
	printf ("creatingh Queue_thread_monodirez fd1 %d fd2 %d indice %d\n", ((param*)p)->fd1,((param*)p)->fd2,((param*)p)->indice);
	ris=pthread_create( &thid, &attr, (ptr_thread_routine) &Queue_thread_monodirez, (void *) p );
	*/
	ris=pthread_create( &thid, &attr, (ptr_thread_routine) &Simple_thread_monodirez, (void *) p );
	if(ris)
	{	perror("pthread_create failed:"); 
		pthread_attr_destroy(&attr);
		exit(5);
	}
#ifdef MYDEBUG
	else
		printf ("OK: created Simple_thread_monodirez\n");
#endif
	pthread_attr_destroy(&attr);
	pthread_exit(NULL);
	return(NULL);
}

void command_usage(void) 
{  printf ("modifica configurazione via stdin: NUMERO_CANALE STATO <ENTER>\n"
				"\tNUMERO_CANALE in  {0,1,2}\n"
				"\tSTATO in  {0=NODELAY, 1=RITARDA, 2=BLOCCA }\n"
				);
}

#define PARAMETRIDEFAULT "./Ritardatore.exe 7001 7002 7003 127.0.0.1 8001 127.0.0.1 8002 127.0.0.1 8003 0.0008"
void usage(void) 
{  printf ("usage: ./Ritardatore.exe LOCALPORT1 LOCALPORT2 LOCALPORT3 REMOTE_IP1 REMOTE_PORT1 REMOTE_IP2 REMOTE_PORT2 REMOTE_IP3 REMOTE_PORT3"
		   " PERC_RITARDI\n"
				"esempio: "  PARAMETRIDEFAULT "\n");
}

int main(int argc, char *argv[])
{
	short int local_port_number[MAXNUMCONNECTIONS];
	int i, ris, maxfd;
	pthread_t thid;
	pthread_attr_t attr;
	fd_set rdset, all;
	double PERCENTUALE_ERRORE;

	if	( (sizeof(int)!=sizeof(long int)) ||  (sizeof(int)!=4)  )
	{
		printf ("dimensione di int e/o long int != 4  -> TERMINO\n");
		fflush(stdout);
		exit(1);
	}

	if(argc==1) { 
		printf ("uso i parametri di default \n%s\n", PARAMETRIDEFAULT );
		local_port_number[0] = 7001;
		local_port_number[1] = 7002;
		local_port_number[2] = 7003;
		strncpy(string_remote_ip_address[0], "127.0.0.1", 99);
		remote_port_number[0] = 8001;
		strncpy(string_remote_ip_address[1], "127.0.0.1", 99);
		remote_port_number[1] = 8002;
		strncpy(string_remote_ip_address[2], "127.0.0.1", 99);
		remote_port_number[2] = 8003;
		PERCENTUALE_ERRORE=PERC_ERR;
		status[0]= 0;
		status[1]= 0;
		status[2]= 0;

		numconndausare=MAXNUMCONNECTIONS;
	}
	else if(argc!=11) { printf ("necessari 11 parametri\n"); usage(); exit(1);  }
	else {
		local_port_number[0] = atoi(argv[1]);
		local_port_number[1] = atoi(argv[2]);
		local_port_number[2] = atoi(argv[3]);
		strncpy(string_remote_ip_address[0], argv[4], 99);
		remote_port_number[0] = atoi(argv[5]);
		strncpy(string_remote_ip_address[1], argv[6], 99);
		remote_port_number[1] = atoi(argv[7]);
		strncpy(string_remote_ip_address[2], argv[8], 99);
		remote_port_number[2] = atoi(argv[9]);
		PERCENTUALE_ERRORE=atof(argv[10]);
		for(i=0;i<MAXNUMCONNECTIONS;i++)
			/* status[i]=atoi(argv[10+i]); */
			status[i]=0;
		/*
		numconndausare = atoi(argv[17]);
		if((numconndausare<=0)||(numconndausare>MAXNUMCONNECTIONS)) {
			printf("serve un numconndausare compreso tra 1 e 3\n"); exit(1);
		}
		*/
		numconndausare=MAXNUMCONNECTIONS;
	}
  
	BloccaConnessione=0;
	init_random();
	MAXDONOTHING= MAXCASUALE - (int)(MAXCASUALE*PERCENTUALE_ERRORE);
	printf("PERC %f  MAXDONOTHING %d\n", PERCENTUALE_ERRORE, MAXDONOTHING);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	pthread_mutex_init( &mutex_blocca, NULL );
	for(i=0;i<MAXNUMCONNECTIONS;i++) pthread_mutex_init (&(mutex[i]), NULL);
	chiusi=0;
	for(i=0;i<MAXNUMCONNECTIONS;i++) stop[i]=0;
	
	if ((signal (SIGHUP, sig_print)) == SIG_ERR) { perror("signal (SIGHUP) failed: "); exit(2); }
	if ((signal (SIGINT, sig_print)) == SIG_ERR) { perror("signal (SIGINT) failed: "); exit(2); }
	if ((signal (SIGTERM, sig_print)) == SIG_ERR) { perror("signal (SIGTERM) failed: "); exit(2); }

	for(i=0;i<numconndausare;i++)
	{
		ris=TCP_setup_socket_listening( &(listenfd[i]), local_port_number[i], 65535, 65536, 1);
		if (!ris)
		{	printf ("setup_socket_listening() failed\n");
			exit(1);
		}
		usalistenfd[i]=1;
	}

	/* sleep(1000); */

	FD_ZERO(&all);
	/* stdin */
	FD_SET(0,&all);

	maxfd=0;
	for(i=0;i<numconndausare;i++) if(maxfd<listenfd[i]) maxfd=listenfd[i];
	for(i=0;i<numconndausare;i++) FD_SET(listenfd[i],&all);
	for(;;)
	{
		rdset=all;
		do {
			ris=select(maxfd+1,&rdset,NULL,NULL,NULL);
		} while( (ris<0) && (errno==EINTR) );
		if(ris<0) {
			perror("select failed: ");
			exit(1);
		} 
		else
		{
			for(i=0;i<numconndausare;i++) 
			{
				if( (usalistenfd[i]) && (FD_ISSET(listenfd[i],&rdset )) )
				{
					/* accetto connessione e creo il thread */
					struct sockaddr_in Cli;
					int clientfd;
					unsigned int len;
					parametri *p;
					do {
						memset (&Cli, 0, sizeof (Cli));		
						len = sizeof (Cli);
						clientfd = accept ( listenfd[i], (struct sockaddr *) &Cli, &len);
					} while ( (clientfd<0) && (errno==EINTR) );
					if (clientfd < 0 )
					{	
						perror("accept() failed: \n");
						usalistenfd[i]=0;
						FD_CLR(listenfd[i],&all);
						/* exit (1);*/
					}
					else
					{	
						printf ("dopo accept(%d) generato clientfd %d\n", listenfd[i],clientfd );
						usalistenfd[i]=0;
						FD_CLR(listenfd[i],&all);
						close(listenfd[i]);
						/* definisco i parametri */
						p=malloc(sizeof(parametri));
						if(p==NULL) { perror("malloc failed: "); exit(2); }
						p->clientfd=clientfd;
						p->indice=i;
	
						ris=pthread_create( &thid, &attr, (ptr_thread_routine) &thread_bidirez, (void *) p );
						if(ris)
						{	perror("pthread_create failed:"); exit(3); }
					}
				}
			}
			/* */
			if( FD_ISSET(0,&rdset) )
			{
				char str[100];

				
  				printf("serve input da tastiera: ");
				fflush(stdout);
				

				fgets(str,100,stdin);

				
				switch(interpreta(str,&all))
				{
				case -1:
					printf("sbagliati parametri passati\n");
					command_usage();
					break;
				case 0:
					printf("corretti i parametri passati, ma impossibile effettuare comando\n");
					command_usage();
					break;
				case 1:
					printf("comando effettuato\n\n");
					break;
				}
				
				
			}
		}
	}
	pthread_attr_destroy(&attr);
	pthread_mutex_destroy(&mutex_blocca);
	for(i=0;i<MAXNUMCONNECTIONS;i++) pthread_mutex_destroy (&(mutex[i]));
	exit(0);
}


int interpreta(char *str, fd_set *all)
{
	int ris, indice, stato;
	ris=sscanf(str,"%d %d", &indice, &stato );
	if(ris!=2)
	{
		printf("sscanf %s failed: \n", str);
		return(-1);
	}
	else
	{
		printf("canale %d stato %d\n", indice, stato );
		if( (indice<0)||(indice>(MAXNUMCONNECTIONS-1)))
		{
			printf("parametro indice sbagliato\n");
			return(-1);
		}
		else if( (stato<0) || (stato>2) )
		{
			printf("parametro stato sbagliato\n");
			return(-1);
		}
		else
		{
			pthread_mutex_lock( &(mutex[indice]) );
			status[indice]=stato;
			pthread_mutex_unlock( &(mutex[indice]) );
		}
		return(1);
	}
}

