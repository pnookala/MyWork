CC = gcc
CFLAGS = -std=c99 -pedantic -Wall
#CFLAGS = -O2 -pthread
LIBS = -lrt -lm -latomic
OBJECTS = queue.o queue_atomic.o blockingqueue.o semiblockingqueue.o lockfreequeue.o basicqueue.o main.o 

all: main

queue.o: queue.c
	$(CC) $(CFLAGS) -c queue.c
	
queue_atomic.o: queue_atomic.c
	$(CC) $(CFLAGS) -c queue_atomic.c
	
blockingqueue.o: blockingqueue.c
	$(CC) $(CFLAGS) -c blockingqueue.c
	
lockfreequeue.o: lockfreequeue.c
	$(CC) $(CFLAGS) -c lockfreequeue.c

semiblockingqueue.o: semiblockingqueue.c
	$(CC) $(CFLAGS) -c semiblockingqueue.c
	
basicqueue.o: basicqueue.c
	$(CC) $(CFLAGS) -c basicqueue.c
	
main.o: main.c
	$(CC) $(CFLAGS) -c main.c

main: $(OBJECTS)
	$(CC) $(OBJECTS) -pthread -o main $(LIBS)

clean:
	rm -f *.o main