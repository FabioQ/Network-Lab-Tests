GCCFLAGS= -Wall -pedantic -ggdb
LINKERFLAGS=-lpthread

all:  Apps Ritardatore.exe

Apps:  Sender.exe Receiver.exe ProxySender.exe ProxyReceiver.exe


Sender.exe: Sender.o Util.o
	gcc -o Sender.exe ${GCCFLAGS} ${LINKERFLAGS} Sender.o Util.o

Sender.o: Sender.c appdefs.h Util.h
	gcc -c ${GCCFLAGS} Sender.c

Receiver.exe: Receiver.o Util.o
	gcc -o Receiver.exe ${GCCFLAGS} ${LINKERFLAGS} Receiver.o Util.o

Receiver.o: Receiver.c appdefs.h Util.h
	gcc -c ${GCCFLAGS} Receiver.c

Ritardatore.o:	Ritardatore.c appdefs.h Util.h
	gcc -c ${GCCFLAGS} Ritardatore.c

Ritardatore.exe:	Ritardatore.o Util.o
	gcc -o Ritardatore.exe ${GCCFLAGS} ${LINKERFLAGS} Util.o Ritardatore.o 

ProxySender.o: ProxySender.o appdefs.h Util.h
	gcc -c ${GCCFLAGS} ProxySender.c

ProxySender.exe:  ProxySender.o Util.o
	gcc -o ProxySender.exe ${GCCFLAGS} ${LINKERFLAGS} Util.o ProxySender.o

ProxyReceiver.o: ProxyReceiver.o appdefs.h Util.h
	gcc -c ${GCCFLAGS} ProxyReceiver.c

ProxyReceiver.exe:  ProxyReceiver.o Util.o
	gcc -o ProxyReceiver.exe ${GCCFLAGS} ${LINKERFLAGS} Util.o ProxyReceiver.o



Util.o: Util.c 
	gcc -c ${GCCFLAGS}  Util.c

clean:	
	rm -f core* 
	rm -f *.exe
	rm -f Sender.o Sender.exe
	rm -f Receiver.o Receiver.exe
	rm -f Ritardatore.o Ritardatore.exe
	rm -f Util.o 
	rm -f ProxySender.o ProxySender.exe
	rm -f ProxyReceiver.o ProxyReceiver.exe

