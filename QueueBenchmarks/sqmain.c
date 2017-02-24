//---------------------------------------------------------------
// File: sqmain.c
// Author: Poornima Nookala
// Date: February 24, 2017
//---------------------------------------------------------------
#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>
#include <sys/types.h>
#include <sched.h>
#include <ck_ring.h>
#include <inttypes.h>
#include "squeue.h"
#include "basicqueue.h"

struct entry {
	int tid;
	int value;
};

struct arg_struct {
	ck_ring_buffer_t *buf;
	ck_ring_t ring;
};

int r, size;
uint64_t s, e, e_a, d_a = 0;

typedef long unsigned int ticks;
#define NUM_THREADS 1
#define NUM_CPUS 1

#ifdef VERBOSE
static int numEnqueue = 0;
static int numDequeue = 0;
#endif

static ticks dequeue_ticks = 0, enqueue_ticks = 0;
static int CUR_NUM_THREADS = NUM_THREADS;

//An alternative way is to use rdtscp which will wait until all previous instructions have been executed before reading the counter; might be problematic on multi-core machines
static __inline__ ticks getticks_serial(void) {
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
static __inline__ ticks getticks(void) {
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

void *worker_handler(void * in) {
	int my_cpu = (int) (long) in;

	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(my_cpu % NUM_CPUS, &set);

	pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

	ticks start_tick, end_tick, diff_tick;

	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
	start_tick = getticks();
	for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
		Dequeue();
#ifdef VERBOSE
		__sync_fetch_and_add(&numDequeue,1);
#endif
	}
	end_tick = getticks();
	diff_tick = end_tick - start_tick;
#ifdef VERBOSE
	printf("Dequeue time: %llu\n", diff_tick/NUM_SAMPLES_PER_THREAD);
#endif
	__sync_add_and_fetch(&dequeue_ticks, diff_tick);

	return 0;

}

void *enqueue_handler(void * in) {
	int my_cpu = (int) (long) in;

	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(my_cpu % NUM_CPUS, &set);

	pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

	ticks start_tick, end_tick, diff_tick;

	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
	start_tick = getticks();
	for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
		Enqueue((atom) (i + 1));
#ifdef VERBOSE
		__sync_fetch_and_add(&numEnqueue,1);
#endif
	}
	end_tick = getticks();
	diff_tick = end_tick - start_tick;
#ifdef VERBOSE
	printf("Dequeue time: %llu\n", diff_tick/NUM_SAMPLES_PER_THREAD);
#endif
	__sync_add_and_fetch(&enqueue_ticks, diff_tick);

	return 0;

}

void *ck_worker_handler(void *arguments) {
	struct arg_struct *args = (struct arg_struct *) arguments;
	struct entry entry;
	ck_ring_buffer_t *buf = args->buf;
	ck_ring_t ring = args->ring;
	s = getticks();
	ck_ring_dequeue_spmc(&ring, buf, &entry);
	e = getticks();
	d_a += (e - s);
	__sync_add_and_fetch(&dequeue_ticks, d_a);

	return 0;
}

void *basicworker_handler(void *_queue)
{
	ticks start_tick,end_tick;

	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES/NUM_THREADS;
	for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
	{
		start_tick = getticks();
		BasicDequeue();
		end_tick = getticks();
		__sync_add_and_fetch(&dequeue_ticks,(end_tick-start_tick));
	}

	return 0;
}

void ResetCounters() {
	dequeue_ticks = 0;
	enqueue_ticks = 0;
#ifdef VERBOSE
	numEnqueue = 0;
	numDequeue = 0;
#endif
}

int main(int argc, char **argv) {
	int k, i;
	///////////////////////////// SQueue - Lock free queue - SECD /////////////////////////////////
	printf("Num threads - %d, Num Samples - %d, Num CPUs - %d \n", NUM_THREADS,
			NUM_SAMPLES, NUM_CPUS);
	printf("SQueue - Single Enqueue Concurrent Dequeue\n");
	printf("Operation,NumSamples,CycleCount,NumThreads\n");
	for (k = 1; k <= NUM_THREADS; k = k * 2) {
		InitQueue();
		ResetCounters();
		CUR_NUM_THREADS = k;
		ticks start_tick, end_tick, diff_tick;
		pthread_t *worker_threads;
		worker_threads = (pthread_t *) malloc(
				sizeof(pthread_t) * CUR_NUM_THREADS);

		cpu_set_t set;

		CPU_ZERO(&set);
		CPU_SET(0, &set);

		pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

#ifndef WAITFORENQUEUE
		for (i = 0; i < CUR_NUM_THREADS; i++)
			pthread_create(&worker_threads[i], NULL, worker_handler,
					(void*) (unsigned long) (i + 1));
#endif

		start_tick = getticks();
		for (i = 0; i < NUM_SAMPLES; i++) {
			Enqueue((atom) (i + 1));
#ifdef VERBOSE
			__sync_fetch_and_add(&numEnqueue,1);
#endif
		}
		end_tick = getticks();

#ifdef WAITFORENQUEUE
		for (i = 0; i < CUR_NUM_THREADS; i++)
			pthread_create(&worker_threads[i], NULL, worker_handler, (void*)(unsigned long)(i+1));
#endif
		diff_tick = end_tick - start_tick;

		for (i = 0; i < CUR_NUM_THREADS; i++)
					pthread_join(worker_threads[i], NULL);

#ifdef VERBOSE
		printf("Enqueue time: %ld\n", diff_tick/NUM_SAMPLES);
#endif

		printf("squeue_secd,enqueue,%d,%ld,%d\n", NUM_SAMPLES, diff_tick / NUM_SAMPLES,
				CUR_NUM_THREADS);

		printf("squeue_secd,dequeue,%d,%ld,%d\n", NUM_SAMPLES,
				dequeue_ticks / NUM_SAMPLES, CUR_NUM_THREADS);

#ifdef VERBOSE
		printf("Enqueue ops: %d\n", numEnqueue);
		printf("Dequeue ops: %d\n", numDequeue);
#endif
	}
	///////////////////////////// SQueue - Lock free queue - CECD /////////////////////////////////
	printf("SQueue - Concurrent Enqueue Concurrent Dequeue\n");
	printf("Operation,NumSamples,CycleCount,NumThreads\n");
	for (k = 1; k <= NUM_THREADS; k = k * 2) {
		InitQueue();
		ResetCounters();
		CUR_NUM_THREADS = k;

		pthread_t *worker_threads;
		pthread_t *enqueue_threads;

		worker_threads = (pthread_t *) malloc(
				sizeof(pthread_t) * CUR_NUM_THREADS);
		enqueue_threads = (pthread_t *) malloc(
				sizeof(pthread_t) * CUR_NUM_THREADS);

		cpu_set_t set;

		CPU_ZERO(&set);
		CPU_SET(0, &set);

		pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

#ifndef WAITFORENQUEUE
		for (i = 0; i < CUR_NUM_THREADS; i++)
			pthread_create(&worker_threads[i], NULL, worker_handler,
					(void*) (unsigned long) (i + 1));
#endif

		for (i = 0; i < CUR_NUM_THREADS; i++)
			pthread_create(&enqueue_threads[i], NULL, enqueue_handler,
					(void*) (unsigned long) (i + 1));

		for (i = 0; i < CUR_NUM_THREADS; i++)
			pthread_join(enqueue_threads[i], NULL);

#ifdef WAITFORENQUEUE
		for (i = 0; i < CUR_NUM_THREADS; i++)
			pthread_create(&worker_threads[i], NULL, worker_handler, (void*)(unsigned long)(i+1));
#endif
		for (i = 0; i < CUR_NUM_THREADS; i++)
			pthread_join(worker_threads[i], NULL);

		printf("squeue_cecd,enqueue,%d,%ld,%d\n", NUM_SAMPLES,
				enqueue_ticks / NUM_SAMPLES, CUR_NUM_THREADS);
		printf("squeue_cecd,dequeue,%d,%ld,%d\n", NUM_SAMPLES,
				dequeue_ticks / NUM_SAMPLES, CUR_NUM_THREADS);
	}
	///////////////////////////// CK - Lock free queue - SPMC /////////////////////////////////
	printf("CK - Single Producer Multiple Consumer \n");
	printf("Type,Operation,NumSamples,CycleCount,NumThreads\n");
	struct entry entry = { 0, 0 };
	ck_ring_buffer_t *buf;
	ck_ring_t ring;

	size = 8; //Hardcoded for benchmarking purposes

	buf = malloc(sizeof(ck_ring_buffer_t) * size);

	ck_ring_init(&ring, size);

	for (int k = 1; k <= NUM_THREADS; k = k * 2) {
		ResetCounters();
		CUR_NUM_THREADS = k;
		pthread_t *worker_threads;

		worker_threads = (pthread_t *) malloc(
				sizeof(pthread_t) * CUR_NUM_THREADS);

		struct arg_struct args;
		args.ring = ring;
		args.buf = buf;
#ifndef WAITFORENQUEUE
		for (int i = 0; i < CUR_NUM_THREADS; i++)
			pthread_create(&worker_threads[i], NULL, ck_worker_handler,(void *) &args);
#endif
		e_a = d_a = s = e = 0;
		for (r = 0; r < NUM_SAMPLES; r++)
		{
			s = getticks();
			ck_ring_enqueue_spmc(&ring, buf, &entry);
			e = getticks();
			e_a += (e - s);
		}
#ifdef WAITFORENQUEUE
		for (int i = 0; i < CUR_NUM_THREADS; i++)
			pthread_create(&worker_threads[i], NULL, ck_worker_handler,(void *) &args);
#endif

		for (i = 0; i < CUR_NUM_THREADS; i++)
			pthread_join(worker_threads[i], NULL);

		printf("ck_spmc,enqueue,%d,%" PRIu64 ",%d\n", NUM_SAMPLES, e_a/NUM_SAMPLES, CUR_NUM_THREADS);
		printf("ck_spmc,dequeue,%d,%" PRIu64 ",%d\n", NUM_SAMPLES, dequeue_ticks/CUR_NUM_THREADS, CUR_NUM_THREADS);
	}

	for (k=1;k<=NUM_THREADS;k=k*2)
		{
		ResetCounters();
			InitBasicQueue();
			CUR_NUM_THREADS = k;
			ticks start_tick,end_tick,diff_tick;
			pthread_t *worker_threads;

			worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * NUM_THREADS);
#ifndef WAITFORENQUEUE
			for (i = 0; i < NUM_THREADS; i++)
							pthread_create(&worker_threads[i], NULL, basicworker_handler, NULL);
#endif
			start_tick = getticks();
			for (i=0;i<NUM_SAMPLES;i++)
			{
				BasicEnqueue((i+1));
			}
			end_tick = getticks();
#ifdef WAITFORENQUEUE
			for (i = 0; i < NUM_THREADS; i++)
				pthread_create(&worker_threads[i], NULL, basicworker_handler, NULL);
#endif
			diff_tick = end_tick - start_tick;

			for (int i = 0; i < NUM_THREADS; i++)
				pthread_join(worker_threads[i], NULL);

			printf("Type,Operation,NumSamples,CycleCount,NumThreads\n");
			printf("basic,enqueue,%d,%ld,%d\n", NUM_SAMPLES, diff_tick/NUM_SAMPLES, NUM_THREADS);

			printf("basic,dequeue,%d,%ld,%d\n", NUM_SAMPLES, dequeue_ticks/NUM_SAMPLES, NUM_THREADS);
		}
	return 0;
}
