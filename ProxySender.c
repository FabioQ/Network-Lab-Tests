/*
	This file ProxySender.c is part of NetworkLabTest.

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


#define PARAMETRIDEFAULT "./ProxySender.exe 6001 127.0.0.1 7001 127.0.0.1 7002 127.0.0.1 7003 127.0.0.1"


/* Stampa come utilizzare il ProxySender */
void usage(void) 
{  
	printf ("usage: ./ProxySender.exe LOCAL_PORT_NUMBER "
	"FIRST_CONNECTION_PORT FIRST_CONNECTION_IP SECOND_CONNECTION_PORT SECOND_CONNECTION_IP"
	"THIRD_CONNECTION_PORT THIRD_CONNECTION_IP\nesempio: " PARAMETRIDEFAULT "\n" );
}



int main(int argc, char* argv[])
{
	/*	variaibli per la gestione e controllo del protocollo	*/
	int ris;							/*	risultato funzioni	*/
	ACK ack[NCONNECTION]; 				/*	Array degli ack per ogni canale	*/
	char *pAck;							/*	Puntatore per la lettura degli ack	*/
	int bleftAck[NCONNECTION];			/*	Byte rimanenti da leggere per ogni canale	*/
	char *msg;							/*	Puntatore per il messaggio	*/
	int bleftMsg;						/*	Byte rimanenti da leggere per il messaggio	*/
	
	DATAGRAM dtg;						/*	Pacchetto da inviare al ProxyReciver	*/
	
	PCHECK	pCheck[NUMMSGS];			/*	Array di controllo per l'invio dei Pacchetti	*/
	
	CQUALITY quality[NCONNECTION];		/*	Array per il controllo della qualità del canale	*/
	
	/*	variabili per la select	*/
	fd_set rAllSet,fdSet;
	int maxfd, risSelect;
	struct timeval wait;
	
	/*	variabili per i socket	*/
	int socketSend[NCONNECTION], senderClient;
	
	/*	Variabili per la stampa dell'output su file	*/
	FILE *fs;
	
	/*	varie ed eventuali	*/
	int k,i,j,first,controlAck,checkAck,last;	
	
	/*inizializzo Variabili*/
	ris=0;bleftMsg=0;maxfd=0;risSelect=0;senderClient=0;k=0;i=0;j=0,first=0,controlAck=0;last=0;
	pAck=NULL;msg=NULL;
	memset(&ack,0,sizeof(ack));
	memset(&bleftAck,0,sizeof(bleftAck));
	memset(&dtg,0,sizeof(dtg));
	memset(&pCheck,0,sizeof(pCheck));
	memset(&quality,0,sizeof(quality));
	memset(&socketSend,0,sizeof(socketSend));
	memset(&wait,0,sizeof(wait));
	FD_ZERO(&rAllSet);
	FD_ZERO(&fdSet);
	fs=fopen("Reg_Sender.txt","wt");
	
	/*	ISTAURO CONNESSIONE CON IL SENDER E CON IL RITARDATORE	*/
	
	ris=istauraConnessioni(&senderClient,socketSend,argv,argc);
	if(!ris){		
		fprintf(fs,"\nErrore durante il tentativo di connessione");				
		printf("Errore durante il tentativo di connessione");
		 
		fclose(fs);		
		return(0);		
	}else if(ris<0){
		usage();
		return(0);
	}
		
	fprintf(fs,"\nConnessioni Istaurate correttamente!");				
	printf("\nConnessioni Istaurate correttamente!");
	
	
	/*	setto parametri per la Select per i cannale con il ritardatore	*/
	for (i=0;i<NCONNECTION;i++){
		if (socketSend[i]>maxfd)
			maxfd=socketSend[i];	
		FD_SET(socketSend[i], &rAllSet);		
	}
	
	
	/*	setto parametri per la Select per il canale con il Sender	*/
	if(senderClient>maxfd)
		maxfd=senderClient;
	FD_SET(senderClient, &rAllSet);  
	
	/*-------------------------------------------------------------------------------------------*/
	i=0;
	controlAck=-1;
	/*	ciclo finché tutti i messaggi ricevuti dal Sender sono stati inviati correttamente al Reciver	*/
	while((i<NUMMSGS)||(first<NUMMSGS))
	{	
		
		checkAck=0;
		fdSet=rAllSet;
		if(timeZero(wait)&&(controlAck<0)){
			risSelect= select(maxfd+1, &fdSet, NULL, NULL, NULL);
		}
		else
		{		
			risSelect= select(maxfd+1, &fdSet, NULL, NULL, &wait); 				
			if (risSelect==0){checkAck=1;}
		} 
		
		if (FD_ISSET(senderClient,&fdSet)){ /*pacchetti ricevuti dal Ritardatore */	
			/*	Stampo Informazioni nel registro	*/
			fprintf(fs,"\nInizio Ricezione pacchetto %d dal Sender",i);							
			
			
			if(msg==NULL){			
				bleftMsg=LENMSG;				
				if((msg=malloc(LENMSG))==NULL) return(0); //	Se non riesco ad allocare memoria termino
			
			}
			/*	Stampo Informazioni nel registro	*/
			fprintf(fs,"\nAttendo Ricezione di %d byte",bleftMsg);							
						
			
			/* lettura del byte dal buffer in modalità non bloccante */
			ris = ReadNB(senderClient, msg, bleftMsg );
			if(ris<0){
				printf("Errore\n");
				fflush(stdout);
				if(ris==-1)
					return(0);
				continue;	
			}else if (ris != bleftMsg)
			{
				/*	Se i byte letti	sono minori dei necessari aggiorno le variabili	*/
				msg+=ris;			/*	incremento la posizione del puntatore di quanti byte ho riceuto	*/
				bleftMsg-=ris;		/*	decremento il numero di byte che mi rimangono da leggere	*/
				
				/*	Stampo Informazioni nel registro	*/
				fprintf(fs,"\nMessaggio arrivato incompleto attendo nuova disponibilita' dati");						
				
				
				continue;			/*	attendo il resto dei dati	*/
			}
			else
			{
				msg-=LENMSG-ris;	/*	resetto il puntatore del messaggio alla posizione iniziale	*/
			}
			
			/*	Stampo Informazioni nel registro	*/
			fprintf(fs,"\nMessaggio %d Ricevuto Correttamente, Costruisco Pacchetto da inoltrare al PR",i);				
			
												
			/*creo datagram applicazione*/
			dtg.seqNum = htonl(i);
			memcpy(&dtg.msg, msg, LENMSG); 
			
			/*	Seleziono il canale migliore per spedire il pacchetto	*/			
			k=selectChannel(quality);
			
			/*	Memorizzo le informazioni riguardanti l'invio del pacchetto	*/
			pCheck[i].canale=k;							/*	cananle di invio	*/
			memcpy(&pCheck[i].dtg,&dtg,LENDATAGRAM);	/*	memorizzo il datagram	*/
			gettimeofday(&pCheck[i].tv,NULL);			/*	memorizzo il momento di invio	*/

			/*	Stampo Informazioni nel registro	*/
			fprintf(fs,"\nPacchetto costruito correttamente Inoltro al PR sul cananle %d alle %d.%d ",k,pCheck[i].tv.tv_sec,pCheck[i].tv.tv_usec);			

			/*Spedisco Pacchetto al ritardatore*/											
			ris = Writen(socketSend[k], &dtg, LENDATAGRAM);
			if (ris != LENDATAGRAM)
			{
				printf ("Errore nell'invio del pacchetto al ritardatore");
				return(0);
			}		
			
			quality[k].buffer++;					/*	aggiorno il buffer del canale	*/			
			keyquality(&quality[k]);	/*	aggiorniamo la qualità del canale	*/
			
			/*	Stampo Informazioni nel registro	*/
			fprintf(fs,"\nPacchetto Spedito correttamente la qualità del cannale è: %d",quality[k].key);							
			
						
			i++;
			
			/*	Calcola Attesa	*/
			
			wait=attesa(pCheck,first,i,&controlAck,last);
			
			if (timeZero(wait)){
				checkAck=1;
			}else checkAck=0;
			/*	resetto variabili	*/
			free(msg);
			msg=NULL;
			k=0;
			
			/*	Stampo Informazioni nel registro	*/
			fprintf(fs,"\nattesa prima di inviare il pacchetto n.%d è di %d sec %d us",controlAck,wait.tv_sec,wait.tv_usec);							
			
		
			/*	Verifico che non ci sia altro da fare	*/
			if((--risSelect==0)&&(checkAck==0))	continue;						
		}							
		/*	Verifico l'arrivo di ack sui canali su cui sono disponibili i dati 	*/
		if(risSelect>0)
		for(j=0; j < NCONNECTION; j++){
			int socket;
			struct timeval now;
			if((socket=socketSend[j])<0)			
				continue;								
			if (FD_ISSET(socket,&fdSet)){
				int *bleft;
				int seqNum;
				
				/*	Stampo Informazioni nel registro	*/
				fprintf(fs,"\nc'e' un ACK da leggere sul canale %d",j);							
								
				
				/* attendo l'ack del pacchetto*/
				if (pAck==NULL){
					if(bleftAck[j]==0){
						bleftAck[j]=LENACK;
					}
					pAck=&(ack[j]);
					pAck+=(LENACK-bleftAck[j]);					
					bleft=&bleftAck[j];
				}
				fprintf(fs,"\nLeggo ACK dal buffer %d byte",*bleft);							
				
				
				ris=ReadNB(socket,pAck,(*bleft));
				if (ris!=(*bleft)){
					(*bleft)=(*bleft)-ris;
					pAck=NULL;
					
					/*	Stampo Informazioni nel registro	*/
					fprintf(fs,"\nL'ack non e' arrivato completo attendo nuovo disponibilita'");							
									
					
					if(--risSelect==0)	break;
					continue;
				}
				else
				{ 
					pAck-=(LENACK-ris);
				}				
			seqNum=ntohl(ack[j].seqNum);
			/*	Stampo Informazioni nel registro	*/
			fprintf(fs,"\nRicevuto ACK per il pacchetto %d",seqNum);							
							
			k=pCheck[seqNum].canale;
			pCheck[seqNum].check=1;

			while ((first<NUMMSGS)&&(pCheck[first].check==1))
				first++;

			/* Aggiorno la qualita' del canale */
			quality[k].buffer--;
			quality[k].pCount++;					/*	incremento il contatore inviati dal canale	*/						
			gettimeofday(&now,NULL);
			now=differenza(now,pCheck[seqNum].tv);
			quality[k].delay+=(now.tv_sec*1000+now.tv_usec/1000);
			keyquality(&quality[k]);
			
			/*	Stampo Informazioni nel registro	*/
			fprintf(fs,"\nAgiorno la qualita' del canale %d a %d",k,quality[k].key);							
																	
			
			if(quality[k].timeout>0) {			
				/*	Stampo Informazioni nel registro	*/
				fprintf(fs,"\nIL CANANLE %d è stato messo in timeout ritardi troppo alti",k);							
			}
						
			/*	resetto variabili	*/
			bleftAck[j]=0;
			pAck=NULL;
			
			/* Ricalcolo l'attesa per l'ack più vecchio non ancora verificato  */						
			wait=attesa(pCheck,first,i,&controlAck,last);
			
			/*	in caso il wait sia zero c'è un ack da controllare	*/			
			if (timeZero(wait)){
				checkAck=1;
			}else checkAck=0;
			/*	Stampo Informazioni nel registro	*/
			fprintf(fs,"\nattesa prima di reinviare il pacchetto n.%d è di %d sec %d us",controlAck,wait.tv_sec,wait.tv_usec);							
			
				
			if(--risSelect==0)	break;
			
			}	//end if
			
		}	//end for
		
		/*	quando la select esce perche' e' finita l'attesa	*/
		while ((risSelect==0) && (checkAck==1)&&(controlAck>=0)){
			
			/*	Nel momento in cui rimane solo un canale funzionante	*/
			if((last==0)&&(isLast(quality))){
				last=1;
			}
			
			struct timeval max_wait;
			
			/*	resetto variabile di controllo per verificare l'ack	*/
			checkAck=0;			
			
			k=pCheck[controlAck].canale;
			
			/*	Stamfspo Informazioni nel registro	*/
			fprintf(fs,"\nL'ACK %d non è arrivato rispedisco pacchetto",controlAck);							
			
						
			
			if ((last==0)&&(quality[k].timeout<=5)){
				quality[k].timeout=10;			
				quality[k].broke++;
			}
			quality[k].buffer--;
			
			/*	Stampo Informazioni nel registro	vp*/
			fprintf(fs,"\nLe penalita' del canale %d e' di %d ed e' stato messo in timeout",k,quality[k].broke);							
			
			
			k=selectChannel(quality);
			
			/*	Memorizzo le informazioni riguardanti l'invio del pacchetto	*/
			pCheck[controlAck].canale=k;							/*	cananle di invio	*/
			gettimeofday(&pCheck[controlAck].tv,NULL);			/*	memorizzo il momento di invio	*/
			
			/*	Stampo Informazioni nel registro	*/
			fprintf(fs,"\nInoltro Pacchetto al PR sul cananle %d alle %d.%d ",k,pCheck[controlAck].tv.tv_sec,pCheck[controlAck].tv.tv_usec);				
			
			
			/*Spedisco Pacchetto al ritardatore*/											
			ris = Writen(socketSend[k], &pCheck[controlAck].dtg, LENDATAGRAM);
			if (ris != LENDATAGRAM)
			{
				printf ("Errore nell'invio del pacchetto al ritardatore");
				return(0);
			}		
								
			quality[k].buffer++;							/*	aggiorno il buffer del canale	*/						
			keyquality(&quality[k]);	/*	aggiorniamo la qualità del canale	*/
			
			/*	Stampo Informazioni nel registro	*/
			fprintf(fs,"\nPacchetto Spedito correttamente la qualità del cannale è: %d\nil buffer contiene %d pacchetti",quality[k].key,quality[k].buffer);										
			
			wait=attesa(pCheck,first,i,&controlAck,last);
			
			/*	Stampo Informazioni nel registro	*/
			fprintf(fs,"\nattesa prima di inviare il pacchetto n.%d è di %d sec %d us",controlAck,wait.tv_sec,wait.tv_usec);							
			
						
			k=0;
		}// end while		 
		
	}  //end while
	
	fclose(fs);
	close(senderClient);
	return (1);
} /* chiudo il main */
