CC = gcc
CFLAGS = -std=c99 -O2 -Wall -Wpedantic -Werror
#CFLAGS = -O2 -pthread -std=c99
OBJECTS = squeue.o sqmain.o

hack: sqmain.c
	$(CC) -O2 -pthread -std=c99 squeue.c sqmain.c -lpthread -lrt -o sqmain

all: sqmain

sqmain: $(OBJECTS)
	$(CC) $(OBJECTS) -lrt -lpthread -o sqmain

sqmain.o: sqmain.c
	$(CC) $(CFLAGS) -c sqmain.c

squeue.o: squeue.c
	$(CC) $(CFLAGS) -c squeue.c

clean:
	rm -f *.o sqmain
