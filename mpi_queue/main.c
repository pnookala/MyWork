/*
 * main.c
 *
 *  Created on: Apr 18, 2017
 *      Author: pnookala
 */

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <mpi.h>
#include "xtaskqueue.h"

typedef long unsigned int ticks;
struct queue incoming;
struct queue results;

ticks *starttimestamp, *endtimestamp;

static __inline__ ticks getticks(void) {
	ticks tsc;
	__asm__ __volatile__(
			"rdtscp;"
			"shl $32, %%rdx;"
			"or %%rdx, %%rax"
			: "=a"(tsc)
			  :
			  : "%rcx", "%rdx");

	return tsc;
}

int cmpfunc (const void * a, const void * b)
{
	return ( *(int*)a - *(int*)b );
}


void SortTicks(ticks* numTicks, int total)
{
	qsort(numTicks, total, sizeof(*numTicks), cmpfunc);
}

void ComputeSummary()
{
#ifdef LATENCY
	int i;
	ticks totalTicks = 0;
	ticks tickMin = endtimestamp[0]-starttimestamp[0];
	ticks tickMax = endtimestamp[0]-starttimestamp[0];
	ticks *numTicks;
	numTicks = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);

	//compute the elapsed time per invocation, and find min and max
	for (i=0;i<NUM_SAMPLES;i++)
	{
		//compute the elapsed time per invocation, and subtract the cost of the emtpy loop cost per iteration
		numTicks[i] = (endtimestamp[i]-starttimestamp[i]);
		totalTicks += numTicks[i];
	}

	SortTicks(numTicks, NUM_SAMPLES);

	tickMin = numTicks[0];
	tickMax = numTicks[NUM_SAMPLES-1];
	//compute average
	double tickAverage = (totalTicks/(NUM_SAMPLES));
	printf("AverageLatency %f\n", tickAverage);

	free(numTicks);
#endif
}

void server(int world_size)
{
	int count = 0;
	int counter = 0;
	while(counter < NUM_SAMPLES * (world_size - 1))
	{
		MPI_Status status;
		MPI_Recv(&count, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		//printf("server recv: %d\n", count);
		int ret = DequeueFromQ(&incoming);
		printf("Dequeued %d\n", ret);
		MPI_Send(&count, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
		counter++;

	}
}

void worker()
{
	int count = 0;
	while(count < NUM_SAMPLES)
	{
		MPI_Send(&count, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
		//printf("worker send: %d\n", count);
		MPI_Recv(&count, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		//printf("worker recv: %d\n", count);
		sleep(0);
		count++;
		EnqueueToQ(count, &results);
	}
}

int main(int argc, char** argv) {
	// Initialize the MPI environment
	MPI_Init(NULL, NULL);
	// Find out rank, size
	int world_rank, i, count;
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	int world_size;
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);

	InitXTaskQueue(&incoming);
	InitXTaskQueue(&results);

	//Enqueue all items and then start server and workers
	for(int i=0; i<(NUM_SAMPLES * (world_size - 1)); i++)
	{
		EnqueueToQ((i+1), &incoming);
	}

	double start, end;
	struct timespec tstart;
	struct timespec tend;

	starttimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
	endtimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);

	MPI_Barrier(MPI_COMM_WORLD);

	printf("Results\n");

	//printf("MPI_WTIME_IS_GLOBAL : %d\n", MPI_WTIME_IS_GLOBAL);

	int token = 0;
	// Receive from the lower process and send to the higher process. Take care
	// of the special case when you are the first process to prevent deadlock.
#ifdef THROUGHPUT
	//start = MPI_Wtime();
	clock_gettime(CLOCK_MONOTONIC, &tstart);
#endif

	if(world_rank != 0)
	{
#ifdef LATENCY
		starttimestamp[token] = getticks();
#endif
		worker();
#ifdef LATENCY
		endtimestamp[token] = getticks();
#endif
	}
	else
	{
		server(world_size);
	}
	//token++;

#ifdef THROUGHPUT
	clock_gettime(CLOCK_MONOTONIC, &tend);
	//end = MPI_Wtime();
#endif

	MPI_Barrier(MPI_COMM_WORLD);
#ifdef LATENCY
	ComputeSummary();
#endif
#ifdef THROUGHPUT
	double elapsed = ( tend.tv_sec - tstart.tv_sec ) + (( tend.tv_nsec - tstart.tv_nsec )/ 1E9);
	//printf("elapsed time: %lf\n", elapsed);

	//printf("end: %f, start: %f, samples: %d\n", end, start, NUM_SAMPLES);
	printf("%d %f\n", world_rank, ((NUM_SAMPLES*1.0)/elapsed));
	//printf("MPI Throughput %f\n",  ((NUM_SAMPLES*1.0)/(end-start)));
#endif

	MPI_Finalize();
}
