CC = gcc -g -DVERBOSE
#CC = icc -mmic -DPHI 	#use this for to make an executable for Phi
CFLAGS = -std=c99 -O2 -fopenmp -Wall -Wpedantic -DLATENCY -DCALIBRATE #-DTHROUGHPUT #-DVERBOSE #-DCALIBRATE
RAWFLAGS = -std=c99 -O2 -fopenmp -Wall -Wpedantic -DLATENCY -DRAW -DCALIBRATE #-DTHROUGHPUT #-DVERBOSE #-DCALIBRATE
THFLAGS = -std=c99 -O2 -fopenmp -Wall -Wpedantic -DTHROUGHPUT -DCALIBRATE #-DVERBOSE #-DCALIBRATE
OBJECTS = squeue.o squeuemultiple.o basicqueue.o xtaskqueue.o sqmain.o
LDFLAGS = -L/usr/local/lib -lrt -lpthread -lurcu-cds -lurcu -llfds711 -lhwloc

all:
	$(CC) $(CFLAGS) -c squeue.c
	$(CC) $(CFLAGS) -c squeuemultiple.c
	$(CC) $(CFLAGS) -c basicqueue.c
	$(CC) $(CFLAGS) -c xtaskqueue.c
	$(CC) $(CFLAGS) -c sqmain.c
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o sqmain
	
allwithtitle:
	$(CC) $(CFLAGS) -DTITLE -c squeue.c
	$(CC) $(CFLAGS) -DTITLE -c squeuemultiple.c
	$(CC) $(CFLAGS) -DTITLE -c basicqueue.c
	$(CC) $(CFLAGS) -c xtaskqueue.c
	$(CC) $(CFLAGS) -DTITLE -c sqmain.c
	$(CC) $(CFLAGS) -DTITLE $(OBJECTS) $(LDFLAGS) -o sqmain

raw: 
	$(CC) $(RAWFLAGS) -c squeue.c
	$(CC) $(RAWFLAGS) -c squeuemultiple.c
	$(CC) $(RAWFLAGS) -c basicqueue.c
	$(CC) $(RAWFLAGS) -c xtaskqueue.c
	$(CC) $(RAWFLAGS) -c sqmain.c
	$(CC) $(RAWFLAGS) $(OBJECTS) $(LDFLAGS) -o sqmain
	
rawwithtitle: 
	$(CC) $(RAWFLAGS) -DTITLE -c squeue.c
	$(CC) $(RAWFLAGS) -DTITLE -c squeuemultiple.c
	$(CC) $(RAWFLAGS) -DTITLE -c basicqueue.c
	$(CC) $(RAWFLAGS) -c xtaskqueue.c
	$(CC) $(RAWFLAGS) -DTITLE -c sqmain.c
	$(CC) $(RAWFLAGS) -DTITLE $(OBJECTS) $(LDFLAGS) -o sqmain

th: 	
	$(CC) $(THFLAGS) -c squeue.c
	$(CC) $(THFLAGS) -c squeuemultiple.c
	$(CC) $(THFLAGS) -c basicqueue.c
	$(CC) $(THFLAGS) -c xtaskqueue.c
	$(CC) $(THFLAGS) -c sqmain.c
	$(CC) $(THFLAGS) $(OBJECTS) $(LDFLAGS) -o sqmain
	
thwithtitle: 	
	$(CC) $(THFLAGS) -DTITLE -c squeue.c
	$(CC) $(THFLAGS) -DTITLE -c squeuemultiple.c
	$(CC) $(THFLAGS) -DTITLE -c basicqueue.c
	$(CC) $(THFLAGS) -c xtaskqueue.c
	$(CC) $(THFLAGS) -DTITLE -c sqmain.c
	$(CC) $(THFLAGS) -DTITLE $(OBJECTS) $(LDFLAGS) -o sqmain

clean:
	rm -f *.o sqmain
