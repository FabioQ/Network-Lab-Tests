/*
	This file ProxyUtil.c is part of NetworkLabTest.

    NetworkLabTest is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    NetworkLabTest is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Nome-Programma.  If not, see <http://www.gnu.org/licenses/>.

    Authors: Fabio Quinzi, Alfredo Saglimbeni, Gabriele Verardi
*/


#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "appdefs.h"
#include "Util.h"

#define NPACKBUFF 45
#define NCONNECTION 3
#define SIZEBUF  100000

/* definizione protocollo pacchetto livello applicazione */
typedef struct protocollo {
	int seqNum;
	MSG msg;
}DATAGRAM;

/* definizione protocollo acknowledge */
typedef struct acknowledge{
	int seqNum;
}ACK;

// definizione struttura per la verfica di arrivo di ogni pacchetto da parte del ProxySender
typedef struct pacCheck{
	struct timeval tv;	
	int check;
	int canale;
	DATAGRAM dtg;
} PCHECK;

//definizione struttura di ordinamento dei pacchetti per il ProxyReciver.
typedef struct seqPac{
	int check;
	MSG msg;
} SEQPAC;

/* struttura qualità canale */
typedef struct channelQuality{
	unsigned int key;		//	chiave di qualità
	int timeout;	//	Controllo se il cannale è in timeout
	int buffer;		//	Verifica Quanto il buffer è pieno
	int broke;		//	Controllo se il cananle è rotto
	int delay;		//	Somma dei ritardi del canale
	int pCount;		//	Contatore pacchetti inviati sul canale
} CQUALITY;

 // definisco Lunghezze standard
#define LENDATAGRAM sizeof(DATAGRAM)
#define LENACK sizeof(ACK)
#define LENPCHECK sizeof(PCHECK)
#define LENSEQPAC sizeof(SEQPAC)


struct timeval attesa (PCHECK check[],int first, int last, int *ack, int isLast){
	struct timeval tv,now,appoggio,add_wait;
	int i;
	memset(&tv,0,sizeof(tv));
	gettimeofday(&now,NULL);
	*ack=-1;
	for(i=first;i<last;i++){
		if(check[i].check==0){
			appoggio=differenza(now,check[i].tv);
			if ((i==first) || !(minore(&appoggio,&tv))){
				tv=appoggio;
				(*ack)=i;
			}
		}
	} //end for
	
	if(isLast==1){
		appoggio.tv_sec=1;
	}else{
		appoggio.tv_sec=0;
	}	
	appoggio.tv_usec=390000; //390 ms	
	return (differenza(appoggio,tv));
}	

int timeZero(struct timeval tv){
	if((tv.tv_sec==0)&&(tv.tv_usec==0))
		return(1);
	return(0);
}

/*	Seleziona il cananle con la qualità migliore	*/
int selectChannel(CQUALITY channels[]){
	int channel,securityChannel;
	int i,key;
	securityChannel=-1;
	key=-1;
	for(i=0;i<NCONNECTION;i++){
		if(channels[i].broke<3){
			if(channels[i].timeout==0){
				if((channels[i].key<key )||(key<0)){
					key=channels[i].key;
					channel=i;
				}
			}else { channels[i].timeout--;securityChannel=i;}				
		}
	}
	if(key==-1) {
		printf("situazione critica, utilizzo canale %d in timeout ma non rotto\n",securityChannel);
		channel=securityChannel;
	}
	
	return(channel);
}

/*	Calcola la qualità del canale	*/
void keyquality(CQUALITY *channel){
	
	int AvgDelay;
	
	if(channel->pCount==0)	channel->pCount=1;
	if(channel->buffer<0)	channel->buffer=0;	
	AvgDelay=channel->delay/channel->pCount;
	if(((AvgDelay)>150)&&(channel->timeout<5)){
		channel->timeout=5;
	}
		
	channel->key=(unsigned int)channel->buffer*AvgDelay;
		
}

int isLast(CQUALITY channel[]){
	int i,count;
	count=0;
	for(i=0;i<NCONNECTION;i++){
		if(channel[i].broke<3)
			count++;
	}
	if(count>1)
		return(0);
	return(1);
}

/*	Read non bloccante	*/
int ReadNB(int fd,char *buf,int nbyte){
	int nleft,nread;
	nleft=nbyte;
	do {
		nread = recv(fd,buf,nleft,MSG_DONTWAIT);		
	} while ( (nread<0) && (errno==EINTR) );
	
	if (errno == EAGAIN) return -2;
	if(nread<0) /* errore */
	{	
		char msg[2000];
		sprintf(msg,"Ricezione: errore di lettura [%d] :",nread);
		perror(msg);
		return(-1);
	}
	nleft -=nread;
	return(nread);
}

/*	-Istaura la connessione tra Il Sender e il ProxySender 
	-Istaura per Tre connessioni sui tre canali con il ritardatore	*/
	
int istauraConnessioni(int *socketSender,int sockets[],char* argv[], int argc){
	
	/* variabili contenenti le informazioni di connessione dei diversi socket*/
	
	int local_port_number;				/*	contiene Porta Sender	*/
	char channel_ip[100];				/*	continene IP del Canale	*/
	int channel_port_number; 			/*	contiene Porta Canale	*/
	struct sockaddr_in socketfdSender;	/*	file descriptor del socket	*/
	int client;							/* 	socket di ascolto	*/
	
	int i;
	int ris;						/*	parametro di controllo delle funzioni di connessione	*/
	int len;						/*	parametro di appoggio per la lunghezza dei dati	*/
	
	/*Inizializziamo Variabili*/
	ris=0;len=0,client=0;
	memset(&channel_ip,0,sizeof(channel_ip));
	local_port_number=0;
	channel_port_number=0;
	
	/*Recupero informazioni dalla linea di comando e Connetto al Ritardatore ogni canale*/
	
	if(argc==1) {
		/*	setto le variabili di default per le connessioni con il ritardatore	*/
		strncpy(channel_ip,"127.0.0.1",99);
		channel_port_number=7001;		 
					
		for (i=0;i<NCONNECTION; i++){
				
				/*	istauro connessione sul cananle i con il Ritardatore	*/
				ris = TCP_setup_connection(&sockets[i], &channel_ip, (channel_port_number++),SIZEBUF,SIZEBUF,1);
				
				/*	controllo che non ci siano errori	*/
				if (!ris)	{
					printf ("Non è stato possibili connettersi al canale %d\n",i);
					return(0);
				}
		
		} //end for
	
		/*	setto le variabili di default per la connessione con il Sender	*/		
		local_port_number=6001;
		
		/*	creo socket per attendere la connessione dal Sender	*/
		ris=TCP_setup_socket_listening( &client, local_port_number, SIZEBUF, SIZEBUF, 1);
		
		/*	controllo che non ci siano errori	*/
		if (!ris)
		{	
			printf ("Non è possibile creare socket per connettere il Sender\n");
			return(0);
		}

		/* attendo connessioni, e ripeto se avviene EINTR come errore! */
		do {			
			memset (&socketfdSender, 0, sizeof (socketfdSender));
			len = sizeof (socketfdSender);
			*socketSender = accept ( client, (struct sockaddr *) &socketfdSender, &len);
		} while ( (*socketSender<0) && (errno==EINTR) );
		
		/*	chiudo canale di ascolto	*/
		close(client);
		
				
	} // end if
	else if(argc!=8) 
	{ 
	 	return(-1);  
	}
	else 
	{	
		/*	se arrivo quà vuol dire che uso i parametri inseriti dall'utente	*/		
		
		int k;  // contatore di appoggio
		
		k=0;	//inizializzo contatore				
		for (i=0;i<NCONNECTION; i++){
				
				/*	recupero informazioni inserite dall'utente	*/
				strncpy(channel_ip, argv[3+k], 99);
				channel_port_number= atoi(argv[2+k]);
				k+=2;								
				
				/*	istauro connessione sul cananle i con il Ritardatore	*/
				ris = TCP_setup_connection(&sockets[i], &channel_ip, (channel_port_number++),SIZEBUF,SIZEBUF,1);
				
				/*	controllo che non ci siano errori	*/
				if (!ris)	{
					printf ("Non è stato possibili connettersi al canale %d\n",i);
					return(0);
				}
		
		} //end for
		
		local_port_number = atoi(argv[1]);		
		
		/*	creo socket per attendere la connessione dal Sender	*/
		ris=TCP_setup_socket_listening( &client, local_port_number, SIZEBUF, SIZEBUF, 1);
		
		/*	controllo che non ci siano errori	*/
		if (!ris)
		{	
			printf ("Non è possibile creare socket per connettere il Sender\n");
			return(0);
		}

		/* attendo connessioni, e ripeto se avviene EINTR come errore! */
		do {			
			memset (&socketfdSender, 0, sizeof (socketfdSender));
			len = sizeof (socketfdSender);
			*socketSender = accept ( client, (struct sockaddr *) &socketfdSender, &len);
		} while ( (*socketSender<0) && (errno==EINTR) );
		
		/*	chiudo canale di ascolto	*/
		close(client);				

	}	//end else	
	
	
	/*	Setto il socket non bloccante	*/
	ris=SetNoBlocking(*socketSender);
	if (!ris)  return(0);
	
	/*	Setto le opzioni TCP del socket in modo da disabilitare l'algoritmo di NAGLE (TCPNODELAY) */
	ris=SetsockoptTCPNODELAY(*socketSender,1);
	if (!ris)  return(0);
	
	/* 	Setto il buffer di ricezione e di invio del socket con l'opzione TCP*/ 
	ris=SetsockoptSndBuf(*socketSender,SIZEBUF);
	if (!ris)  return(0);

	ris=SetsockoptRcvBuf(*socketSender,SIZEBUF);
	if (!ris)  return(0);
	
	/*	Connessioni effetuate con successo	*/
	return(1);
}	// end istauraConnessioni 



/*	-Istaura la connessione tra Il ProxyReciver e il Reciver 
	-Istaura Tre connessioni per i tre canali con il ritardatore	*/

int istauraConnessioniReciver(int *socketSender,int sockets[],char* argv[], int argc){
	
	/* variabili contenenti le informazioni di connessione dei diversi socket*/
	
	int local_port_number;				/*	contiene Porta del Ritardatore	*/
	char reciver_ip[100];				/*	continene IP del Ricevitore	*/
	int reciver_port_number; 			/*	contiene Porta del Ricevitore	*/
	struct sockaddr_in socketfdSender[NCONNECTION];	/*	file descriptor del socket	*/
	int client[NCONNECTION];			/* 	socket di ascolto	*/
	
	int i;							/*	variabile indice	*/
	int ris;						/*	parametro di controllo delle funzioni di connessione	*/
	int len;						/*	parametro di appoggio per la lunghezza dei dati	*/
	
	/*Inizializziamo Variabili*/
	
	memset(&reciver_ip,0,sizeof(reciver_ip));
	local_port_number=0;
	reciver_port_number=0;	
	
	/*Se non vengono inseriti i parametri per linea di comando vengono usati quelli di default*/	
	if(argc==1) {	
		/*	setto le variabili di default per la connessione con il Ritardatore	*/		
		local_port_number=8001;
		
		for(i=0;i<NCONNECTION;i++){
			/*	creo socket per attendere la connessione dal Ritardatore	*/
			ris=TCP_setup_socket_listening( &client[i], (local_port_number+i), SIZEBUF, SIZEBUF, 1);
			/*	controllo che non ci siano errori	*/
			if (!ris)
			{	
				printf ("Non è possibile creare socket per connettere il Ritardatore\n");
				return(0);
			}					
		}
		i=0;
		/* attendo connessioni, e ripeto se avviene EINTR come errore! */
		do {
						
			memset (&socketfdSender[i], 0, sizeof (socketfdSender[i]));
			len = sizeof (socketfdSender[i]);
			/*	attendo connessione	*/
			sockets[i] = accept ( client[i], (struct sockaddr *) &socketfdSender[i], &len);
			
			/*	in caso di successo attendo connessione per il canale successivo	*/
			if (sockets[i]>=0){
				/*	Setto il socket non bloccante	*/
				ris=SetNoBlocking(sockets[i]);
				if (!ris)  return(0);

				/*	Setto le opzioni TCP del socket in modo da disabilitare l'algoritmo di NAGLE (TCPNODELAY) */
				ris=SetsockoptTCPNODELAY(sockets[i],1);
				if (!ris)  return(0);

				/* 	Setto il buffer di ricezione e di invio del socket con l'opzione TCP*/ 
				ris=SetsockoptSndBuf(sockets[i],SIZEBUF);
				if (!ris)  return(0);

				ris=SetsockoptRcvBuf(sockets[i],SIZEBUF);
				if (!ris)  return(0);				
				/*	chiudo canale di ascolto	*/		
				close(client[i]);					
				i++;		
			}					
		} while ((sockets[i]<0)&&(errno==EINTR)||(i<NCONNECTION)); 
		//in caso di problemi con la connessione al ritardatore controllare questa riga
		
		
		/*	setto le variabili di default per la connessione con il Reciver	*/		
		strncpy(reciver_ip,"127.0.0.1",99);
		reciver_port_number=9001;		 
		
		/*	creo socket per connettermi al Reciver	*/
		ris = TCP_setup_connection(socketSender, &reciver_ip, reciver_port_number,SIZEBUF,SIZEBUF,1);
		
		/*	controllo che non ci siano errori	*/
		if (!ris)
		{	
			printf ("Non è possibile connettersi al Ricevitore\n");
			return(0);
		}		
				
	} // end if
	else if(argc!=8) 
	{ 
	 	return(-1);  
	}
	else 
	{				
			i=0;
			/* attendo connessioni, e ripeto se avviene EINTR come errore! */
			do {
				/*	recupero le informazioni da linea di comando per la connessione con il Ritardatore	*/		
				local_port_number=atoi(atoi(argv[1+i]));

				/*	creo socket per attendere la connessione dal Ritardatore	*/
				ris=TCP_setup_socket_listening( &client, (local_port_number+i), SIZEBUF, SIZEBUF, 1);

				/*	controllo che non ci siano errori	*/
				if (!ris)
				{	
					printf ("Non è possibile creare socket per connettere il Ritardatore\n");
					return(0);
				}			
				memset (&socketfdSender, 0, sizeof (socketfdSender));
				len = sizeof (socketfdSender);
				sockets[i] = accept ( client, (struct sockaddr *) &socketfdSender, &len);
				if (sockets[i]>=0){
					
					/*	Setto il socket non bloccante	*/
					ris=SetNoBlocking(sockets[i]);
					if (!ris)  return(0);

					/*	Setto le opzioni TCP del socket in modo da disabilitare l'algoritmo di NAGLE (TCPNODELAY) */
					ris=SetsockoptTCPNODELAY(sockets[i],1);
					if (!ris)  return(0);

					/* 	Setto il buffer di ricezione e di invio del socket con l'opzione TCP*/ 
					ris=SetsockoptSndBuf(sockets[i],SIZEBUF);
					if (!ris)  return(0);

					ris=SetsockoptRcvBuf(sockets[i],SIZEBUF);
					if (!ris)  return(0);
					
					i++;			
					/*	chiudo canale di ascolto	*/		
					close(client);		
				}
				
			} while ((errno==EINTR)&&(i<NCONNECTION)); 
			//in caso di problemi con la connessione al ritardatore controllare questa riga

			/*	setto le variabili di default per la connessione con il Reciver	*/		
			strncpy(reciver_ip, argv[4], 99);			
			reciver_port_number=atoi(argv[5]);		 

			/*	creo socket per connettermi al Reciver	*/
			ris = TCP_setup_connection(socketSender, &reciver_ip, reciver_port_number,SIZEBUF,SIZEBUF,1);

			/*	controllo che non ci siano errori	*/
			if (!ris)
			{	
				printf ("Non è possibile connettersi al Ricevitore\n");
				return(0);
			}		
		
	}	//end else	
	
	/*	Connessioni effetuate con successo	*/
	return(1);
	
}	// end istauraConnessioniReciver


