
all: myftp myftpd

myftp: myftp.o Protocol.o
	gcc myftp.o Protocol.o -o myftp

myftpd: myftpd.o Protocol.o
	gcc myftpd.o Protocol.o -o myftpd

myftp.o: myftp.c Protocol.c
	gcc -c myftp.c

Protocol.o: Protocol.c Protocol.h
	gcc -c Protocol.c

myftpd.o: Protocol.c myftpd.c
	gcc -c myftpd.c


clean: 
	rem*.o
