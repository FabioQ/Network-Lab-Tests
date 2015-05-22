/*
	This file ProxyReceiver.c is part of NetworkLabTest.

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

#include "ProxyUtil.c"

#include "appdefs.h"
#include "Util.h"

#define PARAMETRIDEFAULT "./ProxySender.exe 8001 8002 8003 127.0.0.1 9001"


/* Stampa come utilizzare il ProxySender */
void usage(void) 
{  printf ("usage: ./ProxySender.exe LOCAL_PORT_NUMBER_FIRST LOCAL_PORT_NUMBER_SECOND"
 	"LOCAL_PORT_NUMBER_THIRD REMOTE_IP_ADDRESS REMOTE_PORT\n"
	"esempio: " PARAMETRIDEFAULT "\n" );
}

int main(int argc, char* argv[]){
	
	/*	variaibli per la gestione e controllo del protocollo	*/
	int ris;								/*	risultato funzioni	*/
	DATAGRAM dtg[NCONNECTION]; 				/*	Array dei pacchetti per ogni canale	*/
	char *pDtg;								/*	Puntatore per la lettura dei pacchetti	*/
	int bleftDtg[NCONNECTION];				/*	Byte rimanenti da leggere per ogni canale	*/
	MSG msg;								/*	messaggio da inviare al Reciver	*/
	
	ACK ack;								/*	ACK di risposta per il ProxySender	*/
	
	SEQPAC	check[NUMMSGS];				/*	Array di controllo per l'invio dei Pacchetti al Reciver	*/
		
	/*	variabili per la select	*/
	fd_set rAllSet,fdSet;
	int maxfd, risSelect;
	
	/*	variabili per i socket	*/
	int socketReciver[NCONNECTION], senderReciver;
	
	/*	Variabili file	*/
	FILE *fr;
	
	/*	varie ed eventuali	*/
	int i,j,first;
	
	/*	Inizializzo Variabili	*/
	ris=0;maxfd=0;risSelect=0;senderReciver=0;i=0;j=0;first=0;
	pDtg=NULL;
	memset(&dtg,0,sizeof(dtg));
	memset(&bleftDtg,0,sizeof(bleftDtg));
	memset(&msg,0,sizeof(msg));
	memset(&ack,0,sizeof(ack));
	memset(&check,0,sizeof(check));
	memset(&socketReciver,0,sizeof(socketReciver));
	FD_ZERO(&rAllSet);
	FD_ZERO(&fdSet);
	fr=fopen("Reg_Reciver.txt","wt");
	
	/*	ISTAURO CONNESSIONE CON IL RICEVITORE E CON IL RITARDAOTRE	*/
	
	ris=istauraConnessioniReciver(&senderReciver,socketReciver,argv,argc);
	if(!ris){
		fprintf(fr,"Errore durante il tentativo di connessione");
		fclose(fr);
		return(0);		
	}else if(ris<0){
		usage();
		fclose(fr);
		return(0);
	}
	fprintf(fr,"Connessioni Istaurate correttamente!");
	fflush(stdout);
	
	/*	setto parametri per la Select per i cannali con il ritardatore	*/
	for (i=0;i<NCONNECTION;i++){
		if (socketReciver[i]>maxfd)
			maxfd=socketReciver[i];	
		FD_SET(socketReciver[i], &rAllSet);		
	}
	
	/*	setto parametri per la Select per il canale con il Reciver	*/
	if(senderReciver>maxfd)
		maxfd=senderReciver;
	FD_SET(senderReciver, &rAllSet);  
	
	/*-------------------------------------------------------------------------------------------*/
	i=0;
	/*	ciclo finché tutti i messaggi ricevuti dal ProxySender sono stati ricevuti correttamente e istradati al Reciver	*/
	while(first<NUMMSGS)
	{		
		fdSet=rAllSet;
		int socket;		
		risSelect= select(maxfd+1, &fdSet, NULL, NULL, NULL);
		if (risSelect<0){
			return(0);
		}
		/*	Controllo ogni cananle e se i ci sono dati disponibili li leggo	*/
		for(j = 0; j < NCONNECTION; j++){
							
			if((socket=socketReciver[j])<0)			
				continue;													
			if (FD_ISSET(socket,&fdSet)){
				int *bleft;
				int seqNum;
				if (pDtg==NULL){
					if(bleftDtg[j]==0){						
						bleftDtg[j]=LENDATAGRAM;
					}					
					pDtg=&dtg[j];
					pDtg+=(LENDATAGRAM-bleftDtg[j]);
					bleft=&bleftDtg[j];
				}	
				/* attendo pacchetto*/
				ris=ReadNB(socket,pDtg,(*bleft));
				if(ris<0){
					return(0);
				}
				else if (ris!=(*bleft)){
					(*bleft)=(*bleft)-ris;
					pDtg=NULL;					
					if(--risSelect==0)	break;
					continue;
				}
				else
				{ 					
					pDtg-=(LENDATAGRAM-ris);
				}	/*	se Arrivo quà ho ricevuto il pacchetto correttamente e invio l'ack e instrado il messaggio	*/
					
				seqNum=ntohl(dtg[j].seqNum);				
				fprintf(fr,"\nRicevuto pacchetto %d dal canale %d e archivio il messaggio",seqNum,j);
				/* Se il pacchetto che ricevo non è stato ancora archiviato lo archivio */				
				if(check[seqNum].check==0){			
					check[seqNum].check=1;
					memcpy(&check[seqNum].msg,&dtg[j].msg,LENMSG);															
				}
				
				fprintf(fr,"\nRispondo con ACK su canale %d",j);
				/*	Invio Ack di risposta al ProxySender	*/
				ack.seqNum =htonl(seqNum);
				ris = Writen (socketReciver[j], &(ack), LENACK);
				if (ris != LENACK)
				{
					fprintf(fr,"Problemi nell'inviare ACK");							
					fclose(fr);
					return (0);
				}
				fprintf(fr,"\n ACK spedito corretamente");
				/* spedisco il carico utile via TCP */ 							
				while ((first<NUMMSGS)&&(check[first].check==1)){				
					fprintf(fr,"\n Inoltro messaggio %d al Reciver",first);
					ris = Writen (senderReciver, &check[first].msg, LENMSG);
					if (ris != LENMSG)
					{
						fprintf(fr,"Problemi nel'inviare il messaggio al Reciver");						
						fclose(fr);
						return (0);
					}
					fprintf(fr,"\nMessaggio %d inoltrato correttamente",first);
					first++;					
					/*	incremento contatore pacchetti inviati	*/					
					i++;												
				}
				
				/*	azzero variabili */
				pDtg=NULL;
				bleftDtg[j]=0;
				memset(&dtg[j],0,LENDATAGRAM);
				
				
				if(--risSelect==0)	break;						
			} // END IF
		}	// END FOR
	}	//END WHILE
	
	fprintf(fr,"\n Tutti i %d pacchetti sono stati invati correttamente",i);
	/* chiudo i socket dopo aver instradato tutti i pacchetti*/
	close(socketReciver[0]);
	close(socketReciver[1]);
	close(socketReciver[2]);
	fclose(fr);
	return (1);
} /* chiudo il main */
