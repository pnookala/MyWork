CC = gcc
CFLAGS = -std=c99 -O2 -Wall -Wpedantic -Werror -DWAITFORENQUEUE #-DVERBOSE
OBJECTS = squeue.o basicqueue.o sqmain.o
LDFLAGS = -lrt -lpthread

all: sqmain

sqmain: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o sqmain

sqmain.o: sqmain.c
	$(CC) $(CFLAGS) -c sqmain.c

squeue.o: squeue.c
	$(CC) $(CFLAGS) -c squeue.c
	
basicqueue.o: basicqueue.c
	$(CC) $(CFLAGS) -c basicqueue.c

clean:
	rm -f *.o sqmain
