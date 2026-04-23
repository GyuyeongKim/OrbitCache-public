CC = gcc
#-lhiredis -lmemcached
LDFLAGS= -pthread  -lpthread

all: client server preload
client : client.o
	$(CC) -o client.out client.o -lpthread -lm -D_GNU_SOURCE -lgsl -lgslcblas
client.o: client.c
	$(CC) -c client.c

server: server.o
	$(CC) -o server.out server.o -lpthread -lm -D_GNU_SOURCE -lgsl -lgslcblas

server.o: server.c
	$(CC) -c server.c

preload: preload.o
	$(CC) -o preload.out preload.o -lpthread -lm -D_GNU_SOURCE -lgsl -lgslcblas

preload.o: preload.c
	$(CC) -c preload.c

clean:
	rm -f  *.out *.o
