CC = gcc -g
CFLAGS = -std=c99 -pedantic -Wall -O2 -fopenmp
#CFLAGS = -O2
OBJECTS = locks.o

all: locks

locks.o: locks.c
	$(CC) $(CFLAGS) -c locks.c

locks: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o locks -lpthread -lm -lrt

clean:
	rm -f *.o locks
