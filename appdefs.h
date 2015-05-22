/* appdefs.h */

#ifndef __APPDEFS_H__
#define __APPDEFS_H__

#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DELAYMSEC 50
#define LENBUFMSG 2048

typedef struct vaf {
		struct timeval tv;
		unsigned char buf[LENBUFMSG];
} MSG;

#define LENMSG sizeof(MSG)


/*	numero di pacchetti spediti/ricevuti */
/* #define NUMMSGS 3600L */
#define NUMMSGS 1200L


#define MAXNUMCONNECTIONS 3



#endif   /*  __APPDEFS_H__  */ 


