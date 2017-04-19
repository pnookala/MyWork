/*
 * main.c
 *
 *  Created on: Apr 18, 2017
 *      Author: pnookala
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_SAMPLES 10

typedef long unsigned int ticks;

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

int main(int argc, char** argv) {
  // Initialize the MPI environment
  MPI_Init(NULL, NULL);
  // Find out rank, size
  int world_rank, i;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  double start, end;

  starttimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
  endtimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);

  MPI_Barrier(MPI_COMM_WORLD);

  //printf("MPI_WTIME_IS_GLOBAL : %d\n", MPI_WTIME_IS_GLOBAL);

  int token = 0;
  // Receive from the lower process and send to the higher process. Take care
  // of the special case when you are the first process to prevent deadlock.
#ifdef THROUGHPUT
  start = MPI_Wtime();
#endif
	  while(token < NUM_SAMPLES)
	  {
#ifdef LATENCY
		  starttimestamp[token] = getticks();
#endif
		  MPI_Send(&token, 1, MPI_INT, (world_rank + 1) % world_size, 0, MPI_COMM_WORLD);
		  MPI_Recv(&token, 1, MPI_INT, world_rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#ifdef LATENCY
		  endtimestamp[token] = getticks();
#endif
		  token++;
	  }

#ifdef THROUGHPUT
	  end = MPI_Wtime();
#endif

  MPI_Barrier(MPI_COMM_WORLD);
#ifdef LATENCY
//  for(i=0; i<NUM_SAMPLES;i++)
//  {
//	  printf("Latency %ld\n", (endtimestamp[i]-starttimestamp[i]));
//  }
  ComputeSummary();
#endif
#ifdef THROUGHPUT
  printf("Throughput %f\n", ((end-start)/NUM_SAMPLES));
#endif

  MPI_Finalize();
}
