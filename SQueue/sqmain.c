//---------------------------------------------------------------
// File: QueueMain.c
// Purpose: Main file with tests for a demonstration of a queue
//		as an array.
// Programming Language: C
// Author: Dr. Rick Coleman
// Date: February 11, 2002
//---------------------------------------------------------------
#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <sys/types.h>
#include <sched.h>
#include "squeue.h"

typedef unsigned long long ticks;
#define NUM_THREADS 8
#define NUM_SAMPLES 128000000
#define NUM_CPUS 1

//static int numEnqueue = 0;
//static int numDequeue = 0;
static ticks dequeue_ticks = 0, enqueue_ticks = 0;
static int CUR_NUM_THREADS = NUM_THREADS;


//An alternative way is to use rdtscp which will wait until all previous instructions have been executed before reading the counter; might be problematic on multi-core machines
static __inline__ ticks getticks_serial(void)
{
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

//get number of ticks, could be problematic on modern CPUs with out of order execution
static __inline__ ticks getticks(void)
{
	ticks tsc;
	__asm__ __volatile__(
			"rdtsc;"
			"shl $32, %%rdx;"
			"or %%rdx, %%rax"
			: "=a"(tsc)
			  :
			  : "%rcx", "%rdx");

	return tsc;
}



void *worker_handler( void * in)
{
	int my_cpu = (int) (long) in;

	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(my_cpu%NUM_CPUS,&set);

	pthread_setaffinity_np(pthread_self(),sizeof(set),&set);


	ticks start_tick,end_tick,diff_tick;

	//volatile atom value;
	//for(;;)
	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES/CUR_NUM_THREADS;
	start_tick = getticks();
	for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
	{
		Dequeue();

		//__sync_fetch_and_add(&numDequeue,1);

	}
	end_tick = getticks();
	diff_tick = end_tick - start_tick;
	//    printf("Dequeue time: %llu\n", diff_tick/NUM_SAMPLES_PER_THREAD);
	__sync_add_and_fetch(&dequeue_ticks,diff_tick);

	return 0;

}

void *enqueue_handler( void * in)
{
	int my_cpu = (int) (long) in;

	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(my_cpu%NUM_CPUS,&set);

	pthread_setaffinity_np(pthread_self(),sizeof(set),&set);

	ticks start_tick,end_tick,diff_tick;

	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES/CUR_NUM_THREADS;
	start_tick = getticks();
	for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
	{
		Enqueue((atom)(i+1));

		//__sync_fetch_and_add(&numEnqueue,1);

	}
	end_tick = getticks();
	diff_tick = end_tick - start_tick;
	//    printf("Dequeue time: %llu\n", diff_tick/NUM_SAMPLES_PER_THREAD);
	__sync_add_and_fetch(&enqueue_ticks,diff_tick);

	return 0;

}


int main(int argc, char **argv)
{
	//printf("NUM_SAMPLES: %d\n", NUM_SAMPLES);

	InitQueue();
	printf("Single Enqueue Concurrent Dequeue\n");
	printf("Operation,NumSamples,CycleCount,NumThreads\n");
	for (int k=1;k<=NUM_THREADS;k=k*2)
	{
		CUR_NUM_THREADS = k;

		ticks start_tick,end_tick,diff_tick;
		pthread_t *worker_threads;

		worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);

		start_tick = getticks();
		for (int i=0;i<NUM_SAMPLES;i++)
		{

			Enqueue((atom)(i+1));

			//__sync_fetch_and_add(&numEnqueue,1);

		}
		end_tick = getticks();

		cpu_set_t set;

		CPU_ZERO(&set);
		CPU_SET(0,&set);

		pthread_setaffinity_np(pthread_self(),sizeof(set),&set);

		for (int i = 0; i < CUR_NUM_THREADS; i++)
			pthread_create(&worker_threads[i], NULL, worker_handler, (void*)(unsigned long)(i+1));

		diff_tick = end_tick - start_tick;
		//    printf("Enqueue time: %llu\n", diff_tick/NUM_SAMPLES);
		printf("enqueue,%d,%llu,%d\n", NUM_SAMPLES, diff_tick/NUM_SAMPLES, CUR_NUM_THREADS);

		for (int i = 0; i < CUR_NUM_THREADS; i++)
			pthread_join(worker_threads[i], NULL);

		printf("dequeue,%d,%llu,%d\n", NUM_SAMPLES, dequeue_ticks/NUM_SAMPLES, CUR_NUM_THREADS);

		//    usleep(1000000);
		//printf("NUM_SAMPLES: %d\n", NUM_SAMPLES);
		//printf("Enqueue ops: %d\n", numEnqueue);
		//printf("Dequeue ops: %d\n", numDequeue);
		//    printf("Enqueue time: %llu\n", diff_tick/NUM_SAMPLES);


	}

	printf("Concurrent Enqueue Concurrent Dequeue\n");
	printf("Operation,NumSamples,CycleCount,NumThreads\n");
	for (int k=1;k<=NUM_THREADS;k=k*2)
		{
			CUR_NUM_THREADS = k;

			pthread_t *worker_threads;
			pthread_t *enqueue_threads;

			worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);
			enqueue_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);

			cpu_set_t set;

			CPU_ZERO(&set);
			CPU_SET(0,&set);

			pthread_setaffinity_np(pthread_self(),sizeof(set),&set);

			for (int i = 0; i < CUR_NUM_THREADS; i++)
				pthread_create(&enqueue_threads[i], NULL, enqueue_handler, (void*)(unsigned long)(i+1));

			for (int i = 0; i < CUR_NUM_THREADS; i++)
				pthread_join(enqueue_threads[i], NULL);

			for (int i = 0; i < CUR_NUM_THREADS; i++)
				pthread_create(&worker_threads[i], NULL, worker_handler, (void*)(unsigned long)(i+1));

			for (int i = 0; i < CUR_NUM_THREADS; i++)
				pthread_join(worker_threads[i], NULL);

			printf("enqueue,%d,%llu,%d\n", NUM_SAMPLES, enqueue_ticks/NUM_SAMPLES, CUR_NUM_THREADS);
			printf("dequeue,%d,%llu,%d\n", NUM_SAMPLES, dequeue_ticks/NUM_SAMPLES, CUR_NUM_THREADS);
		}
	return 0;
}
