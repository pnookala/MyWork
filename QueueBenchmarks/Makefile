CC = gcc
CFLAGS = -std=c99 -O2 -Wall -Wpedantic -Werror -DLATENCY #-DRAW #-DTHROUGHPUT #-DVERBOSE #-DCALIBRATE
RAWFLAGS = -std=c99 -O2 -Wall -Wpedantic -Werror -DLATENCY -DRAW #-DTHROUGHPUT #-DVERBOSE #-DCALIBRATE
THFLAGS = -std=c99 -O2 -Wall -Wpedantic -Werror -DTHROUGHPUT #-DVERBOSE #-DCALIBRATE
OBJECTS = squeue.o squeuemultiple.o basicqueue.o sqmain.o
LDFLAGS = -lrt -lpthread

all: sqmain

raw: 
	$(CC) $(RAWFLAGS) -c squeue.c
	$(CC) $(RAWFLAGS) -c squeuemultiple.c
	$(CC) $(RAWFLAGS) -c basicqueue.c
	$(CC) $(RAWFLAGS) -c sqmain.c
	$(CC) $(RAWFLAGS) $(OBJECTS) $(LDFLAGS) -o sqmain

th: 	
	$(CC) $(THFLAGS) -c squeue.c
	$(CC) $(THFLAGS) -c squeuemultiple.c
	$(CC) $(THFLAGS) -c basicqueue.c
	$(CC) $(THFLAGS) -c sqmain.c
	$(CC) $(THFLAGS) $(OBJECTS) $(LDFLAGS) -o sqmain

sqmain: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o sqmain

sqmain.o: sqmain.c
	$(CC) $(CFLAGS) -c sqmain.c

squeue.o: squeue.c
	$(CC) $(CFLAGS) -c squeue.c
	
squeuemultiple.o: squeuemultiple.c
	$(CC) $(CFLAGS) -c squeuemultiple.c
	
basicqueue.o: basicqueue.c
	$(CC) $(CFLAGS) -c basicqueue.c

clean:
	rm -f *.o sqmain
