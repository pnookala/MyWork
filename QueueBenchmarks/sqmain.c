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
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sched.h>
#include <omp.h>
#include <time.h>
#ifndef PHI
#include <ck_ring.h>
#endif
#include <inttypes.h>
#include "squeue.h"
#include "basicqueue.h"
#include <time.h>
#include "squeuemultiple.h"
#include <sys/time.h>
#ifndef PHI
#include <urcu.h>		/* RCU flavor */
#include <urcu/rculfqueue.h>	/* RCU Lock-free queue */
#include <urcu/compiler.h>	/* For CAA_ARRAY_SIZE */

/*
 * Nodes populated into the queue.
 */
struct mynode {
	int value;			/* Node content */
	struct cds_lfq_node_rcu node;	/* Chaining in queue */
	struct rcu_head rcu_head;	/* For call_rcu() */
};
#endif

struct entry {
	int tid;
	int value;
};

#ifndef PHI
struct arg_struct {
	ck_ring_buffer_t *buf;
	ck_ring_t *ring;
};

int r, size;
uint64_t s, e, e_a, d_a = 0;
#endif
struct timeval sTime, eTime;
float clockFreq;

typedef long unsigned int ticks;
#define NUM_THREADS 1
#define NUM_CPUS 1

ticks *enqueuetimestamp, *dequeuetimestamp;

static int numEnqueue = 0;
static int numDequeue = 0;
static int CUR_NUM_THREADS = 0;
static int NUM_QUEUES = 1;

volatile int numEnqueueThreadsCreated = 0, numDequeueThreadsCreated = 0;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;
pthread_mutex_t cond_var_lock =  PTHREAD_MUTEX_INITIALIZER;

double enqueuethroughput, dequeuethroughput = 0;
static pthread_barrier_t barrier;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#ifndef PHI
struct cds_lfq_queue_rcu myqueue;	/* Queue */
static int failed_ck_dequeues = 0;

static
void free_node(struct rcu_head *head)
{
	struct mynode *node = caa_container_of(head, struct mynode, rcu_head);

	free(node);
}

//An alternative way is to use rdtscp which will wait until all previous instructions have been executed before reading the counter; might be problematic on multi-core machines
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
#endif

#ifdef PHI

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
#endif

static __inline__ void cyclesleep(ticks numTicks)
{
	if(numTicks <= 0)
		{
			sleep(0);
			return;
		}
	ticks st;
	st = getticks();
	while(getticks() < (st + numTicks));
	return;
}
static inline unsigned long getticks_phi()
{
	unsigned int hi, lo;

	__asm volatile (
			"xorl %%eax, %%eax nt"
			"cpuid             nt"
			"rdtsc             nt"
			:"=a"(lo), "=d"(hi)
			 :
			 :"%ebx", "%ecx"
	);
	return ((unsigned long)hi << 32) | lo;
}

void *worker_handler(void * in) {
	int my_cpu = (int) (long) in;

	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(my_cpu % NUM_CPUS, &set);

	pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
#ifdef LATENCY
	ticks start_tick, end_tick;
#endif
#ifdef THROUGHPUT
	//ticks st, et;
	struct timespec tstart, tend;
#endif

	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
	//Wait until all threads call the barrier_wait. This is used for getting highest contention with threads
	pthread_barrier_wait(&barrier);

#ifdef VERBOSE
	printf("Dequeue thread woke up\n");
#endif

#ifdef THROUGHPUT
	//st = getticks();
	clock_gettime(CLOCK_MONOTONIC, &tstart);
#endif
	for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++)
	{
#ifdef LATENCY
		start_tick = getticks();
#endif
		Dequeue();
#ifdef LATENCY
		end_tick = getticks();
		pthread_mutex_lock(&lock);
		dequeuetimestamp[numDequeue] = (end_tick-start_tick);
		//printf("%d\n", numDequeue);
		__sync_fetch_and_add(&numDequeue,1);
		pthread_mutex_unlock(&lock);
#endif
	}
#ifdef THROUGHPUT
	//et = getticks();
	clock_gettime(CLOCK_MONOTONIC, &tend);
	pthread_mutex_lock(&lock);
	//ticks diff_tick = et - st;
	//double elapsed = (diff_tick/clockFreq);
	double elapsed = ( tend.tv_sec - tstart.tv_sec ) + (( tend.tv_nsec - tstart.tv_nsec )/ 1E9);
	printf("elapsed time: %lf\n", elapsed);
	//dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1000000000.0)/elapsed);
	dequeuethroughput += ((NUM_SAMPLES_PER_THREAD*1.0)/elapsed);
	pthread_mutex_unlock(&lock);
#endif

	return 0;

}

void *enqueue_handler(void * in)
{
	int my_cpu = (int) (long) in;

	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(my_cpu % NUM_CPUS, &set);

	pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

#ifdef LATENCY
	ticks start_tick, end_tick;
#endif
#ifdef THROUGHPUT
	//ticks st, et;
	struct timespec tstart, tend;
#endif

	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
	pthread_barrier_wait(&barrier);
#ifdef VERBOSE
	printf("Enqueue thread woke up\n");
#endif

#ifdef THROUGHPUT
	//st = getticks();
	clock_gettime(CLOCK_MONOTONIC, &tstart);
#endif
	for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++)
	{
#ifdef LATENCY
		start_tick = getticks();
#endif
		Enqueue((atom) (i+1));
#ifdef LATENCY
		end_tick = getticks();
		pthread_mutex_lock(&lock);
		enqueuetimestamp[numEnqueue] = (end_tick-start_tick);

		__sync_fetch_and_add(&numEnqueue,1);
		pthread_mutex_unlock(&lock);
#endif

	}
	//#ifdef THROUGHPUT
	//	et = getticks();
	//	pthread_mutex_lock(&lock);
	//	ticks diff_tick = et - st;
	//	double elapsed = (diff_tick/clockFreq);
	//	enqueuethroughput += ((NUM_SAMPLES_PER_THREAD * 1000000000.0)/elapsed);
	//	pthread_mutex_unlock(&lock);
	//#endif
#ifdef THROUGHPUT
	clock_gettime(CLOCK_MONOTONIC, &tend);
	pthread_mutex_lock(&lock);
	double elapsed = ( tend.tv_sec - tstart.tv_sec ) + (( tend.tv_nsec - tstart.tv_nsec )/ 1E9);
	printf("elapsed time: %lf\n", elapsed);
	enqueuethroughput += ((NUM_SAMPLES_PER_THREAD*1.0)/elapsed);
	pthread_mutex_unlock(&lock);
#endif

	return 0;
}

void *workermultiple_handler(void * in) {
	int my_cpu = (int) (long) in;

	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(my_cpu % NUM_CPUS, &set);

	pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

#ifdef LATENCY
	ticks start_tick, end_tick;
#endif
#ifdef THROUGHPUT
	ticks st, et;
#endif

	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
	pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
	st = getticks();
#endif
	for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++)
	{
#ifdef LATENCY
		start_tick = getticks();
#endif
		DequeueMultiple(queues[my_cpu], my_cpu);
#ifdef LATENCY
		end_tick = getticks();
		pthread_mutex_lock(&lock);
		dequeuetimestamp[numDequeue++] = (end_tick-start_tick);

		//__sync_fetch_and_add(&numDequeue,1);
		pthread_mutex_unlock(&lock);
#endif
	}
#ifdef THROUGHPUT
	et = getticks();
	pthread_mutex_lock(&lock);
	ticks diff_tick = et - st;
	double elapsed = (diff_tick/clockFreq);
	dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1000000000.0)/elapsed);
	pthread_mutex_unlock(&lock);
#endif

	return 0;

}

void *enqueuemultiple_handler(void * in)
{
	int my_cpu = (int) (long) in;

	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(my_cpu % NUM_CPUS, &set);

	pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

#ifdef LATENCY
	ticks start_tick, end_tick;
#endif
#ifdef THROUGHPUT
	ticks st, et;
#endif

	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
	pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
	st = getticks();
#endif
	for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++)
	{
#ifdef LATENCY
		start_tick = getticks();
#endif
		EnqueueMultiple((atom) (i+1), queues[my_cpu], my_cpu);
#ifdef LATENCY
		end_tick = getticks();
		pthread_mutex_lock(&lock);
		enqueuetimestamp[numEnqueue++] = (end_tick-start_tick);

		//__sync_fetch_and_add(&numEnqueue,1);
		pthread_mutex_unlock(&lock);
#endif

	}
#ifdef THROUGHPUT
	et = getticks();
	pthread_mutex_lock(&lock);
	ticks diff_tick = et - st;
	double elapsed = (diff_tick/clockFreq);
	enqueuethroughput += ((NUM_SAMPLES_PER_THREAD * 1000000000.0)/elapsed);
	pthread_mutex_unlock(&lock);
#endif

	return 0;
}

#ifndef PHI
void *ck_worker_handler(void *arguments) {
	struct arg_struct *args = (struct arg_struct *) arguments;
	struct entry entry;
	ck_ring_buffer_t *buf = args->buf;
	ck_ring_t *ring = args->ring;
	int success = 1;
#ifdef LATENCY
	ticks start_tick, end_tick;
#endif
#ifdef THROUGHPUT
	ticks st, et;
#endif

	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
	pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
	st = getticks();
#endif
	for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++)
	{
#ifdef LATENCY
		start_tick = getticks();
#endif
		if(ck_ring_dequeue_mpmc(ring, buf, &entry) == false)
			success = ck_ring_trydequeue_mpmc(ring, buf, &entry);

#ifdef LATENCY
		end_tick = getticks();
		pthread_mutex_lock(&lock);
		if(success == 0)
		{
			__sync_fetch_and_add(&failed_ck_dequeues,1);
			success = 1;
		}

		dequeuetimestamp[numDequeue++] = (end_tick-start_tick);
		//__sync_fetch_and_add(&numDequeue,1);
		pthread_mutex_unlock(&lock);
#endif

	}
#ifdef THROUGHPUT
	et = getticks();
	pthread_mutex_lock(&lock);
	ticks diff_tick = et - st;
	double elapsed = (diff_tick/clockFreq);
	dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1000000000.0)/elapsed);
	pthread_mutex_unlock(&lock);
#endif
	return 0;
}

void *ck_enqueue_handler(void *arguments) {
	struct arg_struct *args = (struct arg_struct *) arguments;
	ck_ring_buffer_t *buf = args->buf;
	ck_ring_t *ring = args->ring;
#ifdef LATENCY
	ticks start_tick, end_tick;
#endif
#ifdef THROUGHPUT
	ticks st, et;
#endif
	struct entry entry = { 0, 0 };
	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
	pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
	st = getticks();
#endif
	for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++)
	{
#ifdef LATENCY
		start_tick = getticks();
#endif
		ck_ring_enqueue_mpmc(ring, buf, &entry);
#ifdef LATENCY
		end_tick = getticks();
		pthread_mutex_lock(&lock);
		enqueuetimestamp[numEnqueue++] = (end_tick-start_tick);

		//__sync_fetch_and_add(&numEnqueue,1);
		pthread_mutex_unlock(&lock);
#endif
	}
#ifdef THROUGHPUT
	et = getticks();
	pthread_mutex_lock(&lock);
	ticks diff_tick = et - st;
	double elapsed = (diff_tick/clockFreq);
	enqueuethroughput += ((NUM_SAMPLES_PER_THREAD * 1000000000.0)/elapsed);
	pthread_mutex_unlock(&lock);
#endif
	return 0;
}
#endif

void *basicenqueue_handler(void *_queue)
{
#ifdef LATENCY
	ticks start_tick, end_tick;
#endif
#ifdef THROUGHPUT
	ticks st, et;
#endif

	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES/CUR_NUM_THREADS;
	pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
	st  = getticks();
#endif
	for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
	{
#ifdef LATENCY
		start_tick = getticks();
#endif
		BasicEnqueue(i);
#ifdef LATENCY
		end_tick = getticks();
		pthread_mutex_lock(&lock);
		enqueuetimestamp[numEnqueue++] = (end_tick-start_tick);
		//__sync_fetch_and_add(&numEnqueue,1);
		pthread_mutex_unlock(&lock);
#endif
	}
#ifdef THROUGHPUT
	et = getticks();
	pthread_mutex_lock(&lock);
	ticks diff_tick = et - st;
	double elapsed = (diff_tick/clockFreq);
	enqueuethroughput += ((NUM_SAMPLES_PER_THREAD * 1000000000.0)/elapsed);
	pthread_mutex_unlock(&lock);
#endif

	return 0;
}

void *basicworker_handler(void *_queue)
{
#ifdef LATENCY
	ticks start_tick, end_tick;
#endif
#ifdef THROUGHPUT
	ticks st, et;
#endif

	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES/CUR_NUM_THREADS;
	pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
	st = getticks();
#endif
	for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
	{
#ifdef LATENCY
		start_tick = getticks();
#endif
		BasicDequeue();
#ifdef LATENCY
		end_tick = getticks();
		pthread_mutex_lock(&lock);
		dequeuetimestamp[numDequeue++] = (end_tick-start_tick);
		//__sync_fetch_and_add(&numDequeue,1);
		pthread_mutex_unlock(&lock);
#endif
	}
#ifdef THROUGHPUT
	et = getticks();
	pthread_mutex_lock(&lock);
	ticks diff_tick = et - st;
	double elapsed = (diff_tick/clockFreq);
	dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1000000000.0)/elapsed);
	pthread_mutex_unlock(&lock);
#endif

	return 0;
}
#ifndef PHI
void *rculfenqueue_handler()
{
#ifdef LATENCY
	ticks start_tick, end_tick;
#endif
#ifdef THROUGHPUT
	ticks st, et;
#endif

	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES/CUR_NUM_THREADS;
	pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
	st = getticks();
#endif
	for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
	{
		struct mynode *node;
#ifdef LATENCY
		start_tick = getticks();
#endif
		node = malloc(sizeof(*node));

		cds_lfq_node_init_rcu(&node->node);
		node->value = i;
		/*
		 * Both enqueue and dequeue need to be called within RCU
		 * read-side critical section.
		 */
		rcu_read_lock();
		cds_lfq_enqueue_rcu(&myqueue, &node->node);
		rcu_read_unlock();
#ifdef LATENCY
		end_tick = getticks();
		pthread_mutex_lock(&lock);
		enqueuetimestamp[numEnqueue] = (end_tick-start_tick);
		__sync_fetch_and_add(&numEnqueue,1);
		pthread_mutex_unlock(&lock);
#endif
	}
#ifdef THROUGHPUT
	et = getticks();
	pthread_mutex_lock(&lock);
	ticks diff_tick = et - st;
	double elapsed = (diff_tick/clockFreq);
	enqueuethroughput += ((NUM_SAMPLES_PER_THREAD * 1000000000.0)/elapsed);
	pthread_mutex_unlock(&lock);
#endif

	return 0;
}

void* rculfdequeue_handler()
{
#ifdef LATENCY
	ticks start_tick, end_tick;
#endif
#ifdef THROUGHPUT
	ticks st, et;
#endif

	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES/CUR_NUM_THREADS;
	pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
	st = getticks();
#endif
	for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
	{
		/*
		 * Dequeue each node from the queue. Those will be dequeued from
		 * the oldest (first enqueued) to the newest (last enqueued).
		 */
		struct cds_lfq_node_rcu *qnode;
		struct mynode *node;

#ifdef LATENCY
		start_tick = getticks();
#endif
		/*
		 * Both enqueue and dequeue need to be called within RCU
		 * read-side critical section.
		 */
		rcu_read_lock();
		qnode = cds_lfq_dequeue_rcu(&myqueue);
		rcu_read_unlock();

#ifdef LATENCY
		end_tick = getticks();
		pthread_mutex_lock(&lock);
		dequeuetimestamp[numDequeue++] = (end_tick-start_tick);
		//__sync_fetch_and_add(&numDequeue,1);
		pthread_mutex_unlock(&lock);
#endif
		//			if (!qnode) {
		//				break;	/* Queue is empty. */
		//			}

		/* Getting the container structure from the node */
		node = caa_container_of(qnode, struct mynode, node);
#ifdef VERBOSE
		printf(" %d", node->value);
#endif
		call_rcu(&node->rcu_head, free_node);
	}
#ifdef THROUGHPUT
	et = getticks();
	pthread_mutex_lock(&lock);
	ticks diff_tick = et - st;
	double elapsed = (diff_tick/clockFreq);
	dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1000000000.0)/elapsed);
	pthread_mutex_unlock(&lock);
#endif

	return 0;

}
#endif

int cmpfunc (const void * a, const void * b)
{
	return ( *(int*)a - *(int*)b );
}

void SortTicks(ticks* numTicks)
{
	//	ticks a;
	//	for (int i = 0; i < NUM_SAMPLES; i++)
	//	    {
	//	        for (int j = i + 1; j < NUM_SAMPLES; j++)
	//	        {
	//	            if (numTicks[i] > numTicks[j])
	//	            {
	//	                a =  numTicks[i];
	//	                numTicks[i] = numTicks[j];
	//	                numTicks[j] = a;
	//	            }
	//	        }
	//	    }

	//printf("Size:%d, Num size:%ld\n", NUM_SAMPLES, sizeof(numTicks));
	qsort(numTicks, NUM_SAMPLES, sizeof(*numTicks), cmpfunc);
}

void ResetCounters() {
	numEnqueue = 0;
	numDequeue = 0;
	numEnqueueThreadsCreated = 0;
	numDequeueThreadsCreated = 0;
	dequeuethroughput = 0;
	enqueuethroughput = 0;
#ifndef PHI
	failed_ck_dequeues = 0;
#endif
}

void ComputeSummary(int type, int numThreads, FILE* afp, FILE* rfp, int rdtsc_overhead)
{
#ifdef LATENCY
	ticks totalEnqueueTicks = 0,  totalDequeueTicks = 0;
	ticks enqueuetickMin = enqueuetimestamp[0]-rdtsc_overhead;
	ticks enqueuetickMax = enqueuetimestamp[0]-rdtsc_overhead;
	ticks dequeuetickMin = dequeuetimestamp[0]-rdtsc_overhead;
	ticks dequeuetickMax = dequeuetimestamp[0]-rdtsc_overhead;
	ticks *numEnqueueTicks, *numDequeueTicks;
	numEnqueueTicks = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
	numDequeueTicks = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);

	//compute the elapsed time per invocation, and find min and max
	for (int i=0;i<NUM_SAMPLES;i++)
	{
		//compute the elapsed time per invocation, and subtract the cost of the emtpy loop cost per iteration
		numEnqueueTicks[i]=enqueuetimestamp[i]-rdtsc_overhead;
		totalEnqueueTicks += numEnqueueTicks[i];

		numDequeueTicks[i]= dequeuetimestamp[i]-rdtsc_overhead;
		totalDequeueTicks += numDequeueTicks[i];
	}

	SortTicks(numEnqueueTicks);
	SortTicks(numDequeueTicks);

	for(int i=0;i<NUM_SAMPLES;i++)
	{
#ifdef RAW
		double enqueueTime = (numEnqueueTicks[i]/clockFreq);
		double dequeueTime = (numDequeueTicks[i]/clockFreq);

		fprintf(rfp, "%d %d %ld %ld %d %lf %lf\n", type, NUM_SAMPLES, (numEnqueueTicks[i]), (numDequeueTicks[i]), CUR_NUM_THREADS, enqueueTime, dequeueTime);
#ifdef VERBOSE
		printf("%d %d %ld %ld %d %lf %lf\n", type, NUM_SAMPLES, (numEnqueueTicks[i]), (numDequeueTicks[i]), CUR_NUM_THREADS, enqueueTime, dequeueTime);
#endif
#endif

	}

	enqueuetickMin = numEnqueueTicks[0];
	enqueuetickMax = numEnqueueTicks[NUM_SAMPLES-1];

	dequeuetickMin = numDequeueTicks[0];
	dequeuetickMax = numDequeueTicks[NUM_SAMPLES-1];

	//compute average
	double tickEnqueueAverage = (totalEnqueueTicks/(NUM_SAMPLES));
	double tickDequeueAverage = (totalDequeueTicks/(NUM_SAMPLES));

	printf("Num threads: %d, Num samples: %d\n", numThreads, NUM_SAMPLES);
	printf("Enqueue Min: %ld\n", enqueuetickMin);
	printf("Dequeue Min: %ld\n", dequeuetickMin);

	printf("Enqueue Max: %ld\n", enqueuetickMax);
	printf("Dequeue Max: %ld\n", dequeuetickMax);

	printf("Average Enqueue : %lf\n", tickEnqueueAverage);
	printf("Average Dequeue : %lf\n", tickDequeueAverage);

	ticks enqueuetickmedian = 0, dequeuetickmedian = 0;

	if(NUM_SAMPLES % 2==0) {
		// if there is an even number of elements, return mean of the two elements in the middle
		enqueuetickmedian = ((numEnqueueTicks[(NUM_SAMPLES/2)] + numEnqueueTicks[(NUM_SAMPLES/2) - 1]) / 2.0);
		dequeuetickmedian = ((numDequeueTicks[(NUM_SAMPLES/2)] + numDequeueTicks[(NUM_SAMPLES/2) - 1]) / 2.0);
	} else {
		// else return the element in the middle
		enqueuetickmedian = numEnqueueTicks[(NUM_SAMPLES/2)];
		dequeuetickmedian = numDequeueTicks[(NUM_SAMPLES/2)];
	}

	printf("Median Enqueue : %ld\n", enqueuetickmedian);
	printf("Median Dequeue : %ld\n", dequeuetickmedian);

	double enqueueMinTime = ((enqueuetickMin)/clockFreq);
	double dequeueMinTime = ((dequeuetickMin)/clockFreq);
	double enqueueMaxTime = ((enqueuetickMax)/clockFreq);
	double dequeueMaxTime = ((dequeuetickMax)/clockFreq);
	double enqueueAvgTime = ((tickEnqueueAverage)/clockFreq);
	double dequeueAvgTime = ((tickDequeueAverage)/clockFreq);

	printf("Enqueue Min Time (ns): %lf\n", enqueueMinTime);
	printf("Dequeue Min Time (ns): %lf\n", dequeueMinTime);

	printf("Enqueue Max Time (ns): %lf\n", enqueueMaxTime);
	printf("Dequeue Max Time (ns): %lf\n", dequeueMaxTime);

	printf("Average Enqueue Time (ns): %lf\n", enqueueAvgTime);
	printf("Average Dequeue Time (ns): %lf\n", dequeueAvgTime);

	fprintf(afp, "%d %d %d %ld %ld %ld %ld %lf %lf %ld %ld %lf %lf %lf %lf %lf %lf\n",type, numThreads, NUM_SAMPLES, enqueuetickMin, dequeuetickMin, enqueuetickMax, dequeuetickMax, tickEnqueueAverage, tickDequeueAverage, enqueuetickmedian, dequeuetickmedian, enqueueMinTime, dequeueMinTime, enqueueMaxTime, dequeueMaxTime, enqueueAvgTime, dequeueAvgTime);
#endif
#ifdef THROUGHPUT
	printf("NumSamples:%d NumThreads:%d EnqueueThroughput:%f DequeueThroughput:%f\n", NUM_SAMPLES, numThreads, enqueuethroughput, dequeuethroughput);
#ifdef TITLE
	fprintf(afp, "NumSamples NumThreads EnqueueThroughput DequeueThroughput\n");
#endif
	fprintf(afp, "%d %d %f %f\n", NUM_SAMPLES, numThreads, enqueuethroughput, dequeuethroughput);
#endif

#ifdef LATENCY
	free(numEnqueueTicks);
	free(numDequeueTicks);
#endif
}

int main(int argc, char **argv) {
	int threadCount=0;
	int queueType;
	int *threads = malloc(sizeof(int*));
	char* fileName1, *fileName2;
	//Inputs are type of queue, thread list,
	if(argc != 5)
	{
		printf("Usage: <QueueType 1-SQueue, 2-CK, 3-Basic Queue, 4-Multiple Incoming Queues>, \nThreads-1,2,4,6,8,12,16,24,32,48,57,96,114,192,228,384,456,768,912,1024, \nRaw data file name: <name>,  \nSummary file name: <name>, \nClock Frequency in GHz: <3.4>\n");
		exit(-1);
	}
	else
	{
		char* arg = argv[1];
		queueType = atoi(arg);

		switch(queueType)
		{
		case 1:
			printf("Queue type: SQueue\n");
			break;
		case 2:
			printf("Queue type: CK\n");
			break;
		case 3:
			printf("Queue type: Basic Linux Queue\n");
			break;
		case 4:
			printf("Queue type: Multiple Incoming Queues (NumQueues=NumThreads/2) \n");
			break;
		case 5:
			printf("Queue type: RCU LF Queue\n");
			break;
		case 6:
			printf("Queue type: SQueue using OpenMP\n");
			break;
		default:
			printf("Usage: <QueueType 1-SQueue, 2-CK, 3-Basic Queue, 4-Multiple Incoming Queues, 5-RCU LF Queue>, \nThreads-1,2,4,6,8,12,16,24,32,48,57,96,114,192,228,384,456,768,912,1024, \nRaw data file name: <name>,  \nSummary file name: <name>\n");
			exit(-1);
			break;
		}

		char* str = argv[2];
		char *thread;
		thread = strtok (str,",");
		printf("Thread list: ");
		while (thread != NULL)
		{
			threads[threadCount] = atoi(thread);
			threadCount++;
			printf("%s ", thread);
			thread = strtok (NULL, ",");
		}

		printf("\n");

		fileName1 = argv[3];
		fileName2 = argv[4];

		printf("Num of samples: %d\n", NUM_SAMPLES);
		printf("Thread list count: %d\n", threadCount);
		printf("Output files: %s, %s\n", fileName1, fileName2);
	}
	int rdtsc_overhead_ticks = 0;

	//Open file for storing data

	FILE *rfp=fopen(fileName1, "a");
	FILE *afp=fopen(fileName2, "a");

	struct timezone tz;
	struct timeval tvstart, tvstop;
	unsigned long long int cycles[2];
	unsigned long microseconds;

	memset(&tz, 0, sizeof(tz));

	gettimeofday(&tvstart, &tz);
	cycles[0] = getticks();
	gettimeofday(&tvstart, &tz);

	usleep(250000);

	gettimeofday(&tvstop, &tz);
	cycles[1] = getticks();
	gettimeofday(&tvstop, &tz);

	microseconds = ((tvstop.tv_sec-tvstart.tv_sec)*1000000) + (tvstop.tv_usec-tvstart.tv_usec);

	clockFreq = ((cycles[1]-cycles[0])*1.0) / (microseconds * 1000);

	printf("Clock Freq Obtained: %f\n", clockFreq);

#ifdef CALIBRATE
	//Calibrate RDTSC
	ticks start_tick = (ticks)0;
	ticks end_tick = (ticks)0;
	ticks totalTicks = (ticks)0;
	ticks diff_tick = (ticks)0;

	ticks minRdtscTicks = 0;
	for (int i = 0; i < NUM_SAMPLES; i++)
	{
		start_tick = getticks();
		end_tick = getticks();

		diff_tick = (end_tick - start_tick);
		totalTicks += diff_tick;
		if(i == 0)
			minRdtscTicks = diff_tick;
		else
		{
			if(minRdtscTicks > diff_tick)
				minRdtscTicks = diff_tick;
		}
		//printf("min rdtsc: %ld\n", minRdtscTicks);
	}

	//rdtsc_overhead_ticks = (totalTicks/NUM_SAMPLES);
	rdtsc_overhead_ticks = minRdtscTicks;
	printf("RDTSC time: %d\n", rdtsc_overhead_ticks);
#ifdef TITLE
#ifdef RAW
	fprintf(rfp, "RDTSC time: %d\n", rdtsc_overhead_ticks);
#endif
	fprintf(afp, "RDTSC time: %d\n", rdtsc_overhead_ticks);

#ifdef RAW
	fprintf(rfp, "Clock Freq: %f\n", clockFreq);
#endif
	fprintf(afp, "Clock Freq: %f\n", clockFreq);

#endif
#endif
	//Initialization
	enqueuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
	dequeuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);

	for (int i=0;i<NUM_SAMPLES;i++)
	{
		enqueuetimestamp[i] = (ticks)0;
		dequeuetimestamp[i] = (ticks)0;
	}

	//	if(rfp == NULL || afp == NULL)
	//		exit(-1);
#ifdef RAW
#ifdef TITLE
	fprintf(rfp, "QueueType NumSamples EnqueueCycles DequeueCycles NumThreads EnqueueTime(ns) DequeueTime(ns)\n");
#endif
#endif
#ifdef VERBOSE
	printf("QueueType NumSamples EnqueueCycles DequeueCycles NumThreads EnqueueTime(ns) DequeueTime(ns)\n");
#endif
#ifdef LATENCY
#ifdef TITLE
	fprintf(afp, "QueueType NumThreads NumSamples EnqueueMin DequeueMin EnqueueMax DequeueMax EnqueueAverage DequeueAverage EnqueueMedian DequeueMedian EnqueueMinTime DequeueMinTime EnqueueMaxTime DequeueMaxTime EnqueueAverageTime DequeueAverageTime\n");
#endif
#endif

	//Execute benchmarks for various types of queues
	switch(queueType)
	{
	case 1: //SQueue
		for (int k = 0; k < threadCount; k++)
		{
			InitQueue();
			ResetCounters();
			enqueuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
			dequeuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
			CUR_NUM_THREADS = (threads[k])/2;

			pthread_t *worker_threads;
			pthread_t *enqueue_threads;

			worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);
			enqueue_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);

			cpu_set_t set;

			CPU_ZERO(&set);
			CPU_SET(0, &set);

			pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

			//Set number of threads that will call the barrier_wait to total of enqueue and dequeue threads
			pthread_barrier_init(&barrier, NULL, threads[k]);

			for (int i = 0; i < CUR_NUM_THREADS; i++)
			{
				pthread_create(&enqueue_threads[i], NULL, enqueue_handler,(void*) (unsigned long) (i));
				pthread_create(&worker_threads[i], NULL, worker_handler,(void*) (unsigned long) (i));
			}

			for (int i = 0; i < CUR_NUM_THREADS; i++)
			{
				pthread_join(enqueue_threads[i], NULL);
				pthread_join(worker_threads[i], NULL);
			}

			ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp, rdtsc_overhead_ticks);

			free(enqueuetimestamp);
			free(dequeuetimestamp);
		}
		break;
#ifndef PHI
	case 2: //Concurrency Kit
		for (int k = 0; k < threadCount; k++)
		{
			ResetCounters();
			ck_ring_buffer_t *buf;
			ck_ring_t *ring;

			size = NUM_SAMPLES; //Hardcoded for benchmarking purposes

			buf = malloc(sizeof(ck_ring_buffer_t) * size);
			ring = malloc(sizeof(ck_ring_t) * size);

			ck_ring_init(ring, size);

			struct arg_struct args;
			args.ring = ring;
			args.buf = buf;

			enqueuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
			dequeuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
			CUR_NUM_THREADS = (threads[k])/2;

			pthread_t *worker_threads;
			pthread_t *enqueue_threads;

			worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);
			enqueue_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);

			//Set number of threads that will call the barrier_wait to total of enqueue and dequeue threads
			pthread_barrier_init(&barrier, NULL, threads[k]);

			for (int i = 0; i < CUR_NUM_THREADS; i++)
			{
				pthread_create(&enqueue_threads[i], NULL, ck_enqueue_handler,(void *) &args);
				pthread_create(&worker_threads[i], NULL, ck_worker_handler,(void *) &args);
			}

			for (int i = 0; i < CUR_NUM_THREADS; i++)
			{
				pthread_join(enqueue_threads[i], NULL);
				pthread_join(worker_threads[i], NULL);
			}

			printf("Failed Dequeues: %d\n", failed_ck_dequeues);
			fprintf(afp, "Failed Dequeues: %d\n", failed_ck_dequeues);
			ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp, rdtsc_overhead_ticks);

			free(enqueuetimestamp);
			free(dequeuetimestamp);
		}
		break;
#endif
	case 3: //Basic linux queue
		for (int k = 0; k < threadCount; k++)
		{
			InitBasicQueue();
			ResetCounters();

			enqueuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
			dequeuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
			CUR_NUM_THREADS = (threads[k])/2;

			pthread_t *worker_threads;
			pthread_t *enqueue_threads;

			worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);
			enqueue_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);

			//Set number of threads that will call the barrier_wait to total of enqueue and dequeue threads
			pthread_barrier_init(&barrier, NULL, threads[k]);

			for (int i = 0; i < CUR_NUM_THREADS; i++)
			{
				pthread_create(&enqueue_threads[i], NULL, basicenqueue_handler,NULL);
				pthread_create(&worker_threads[i], NULL, basicworker_handler,NULL);
			}

			for (int i = 0; i < CUR_NUM_THREADS; i++)
			{
				pthread_join(enqueue_threads[i], NULL);
				pthread_join(worker_threads[i], NULL);
			}

			ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp, rdtsc_overhead_ticks);

			free(enqueuetimestamp);
			free(dequeuetimestamp);
		}
		break;
	case 4://Multiple Incoming Queues
		for (int k = 0; k < threadCount; k++)
		{
			CUR_NUM_THREADS = (threads[k])/2;
			NUM_QUEUES = CUR_NUM_THREADS;

			printf("Number of queues: %d\n", NUM_QUEUES);

			InitQueues(NUM_QUEUES);
			ResetCounters();
			enqueuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
			dequeuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);

			pthread_t *worker_threads;
			pthread_t *enqueue_threads;

			worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);
			enqueue_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);

			cpu_set_t set;

			CPU_ZERO(&set);
			CPU_SET(0, &set);

			pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

			//Set number of threads that will call the barrier_wait to total of enqueue and dequeue threads
			pthread_barrier_init(&barrier, NULL, threads[k]);

			for (int i = 0; i < CUR_NUM_THREADS; i++)
			{
				pthread_create(&enqueue_threads[i], NULL, enqueuemultiple_handler,(void*) (unsigned long) (i));
				pthread_create(&worker_threads[i], NULL, workermultiple_handler,(void*) (unsigned long) (i));
			}

			for (int i = 0; i < CUR_NUM_THREADS; i++)
			{
				pthread_join(enqueue_threads[i], NULL);
				pthread_join(worker_threads[i], NULL);
			}

			ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp, rdtsc_overhead_ticks);

			free(enqueuetimestamp);
			free(dequeuetimestamp);
		}
		break;
#ifndef PHI
	case 5: //RCU LF Queue

		for (int k = 0; k < threadCount; k++)
		{
			CUR_NUM_THREADS = (threads[k])/2;
			int ret = 0;

			ResetCounters();
			enqueuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
			dequeuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);

			pthread_t *worker_threads;
			pthread_t *enqueue_threads;

			worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);
			enqueue_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);

			/*
			 * Each thread need using RCU read-side need to be explicitly
			 * registered.
			 */
			rcu_register_thread();

			cds_lfq_init_rcu(&myqueue, call_rcu);

			//Set number of threads that will call the barrier_wait to total of enqueue and dequeue threads
			pthread_barrier_init(&barrier, NULL, threads[k]);

			for (int i = 0; i < CUR_NUM_THREADS; i++)
			{
				pthread_create(&enqueue_threads[i], NULL, rculfenqueue_handler,NULL);
				pthread_create(&worker_threads[i], NULL, rculfdequeue_handler,NULL);
			}

			for (int i = 0; i < CUR_NUM_THREADS; i++)
			{
				pthread_join(enqueue_threads[i], NULL);
				pthread_join(worker_threads[i], NULL);
			}

			ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp, rdtsc_overhead_ticks);

			free(enqueuetimestamp);
			free(dequeuetimestamp);

			/*
			 * Release memory used by the queue.
			 */
			ret = cds_lfq_destroy_rcu(&myqueue);
			if (ret) {
				printf("Error destroying queue (non-empty)\n");
			}

			rcu_unregister_thread();
			return ret;
		}
		break;
#endif
	case 6://SQueue with OpenMP threads
		for (int k = 0; k < threadCount; k++)
		{
			InitQueue();
			ResetCounters();
			enqueuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
			dequeuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
			CUR_NUM_THREADS = threads[k];

			cpu_set_t set;

			CPU_ZERO(&set);
			CPU_SET(0, &set);

			pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

#ifdef LATENCY
			ticks start_tick, end_tick;
#endif
#ifdef THROUGHPUT
			struct timespec tstart, tend;
#endif
			omp_set_num_threads(CUR_NUM_THREADS);
//			struct timespec tim, tim2;
//			tim.tv_sec = 0;
//			 tim.tv_nsec = 0;

#ifdef THROUGHPUT
			clock_gettime(CLOCK_MONOTONIC, &tstart);
#endif
#pragma omp parallel for
			for (int i = 0; i < NUM_SAMPLES; i++)
			{
#ifdef LATENCY
				start_tick = getticks();
#endif
				//Enqueue((atom) (i+1));
//				int tid=omp_get_thread_num();
//				printf("thread id:%d\n", tid);
				//nanosleep(&tim, &tim2);
				//cyclesleep(100);
				if(i < (NUM_SAMPLES/2))
					sleep(0);
				else
					sleep(1);

				//usleep(1);
#ifdef LATENCY
				end_tick = getticks();
				pthread_mutex_lock(&lock);
				enqueuetimestamp[numEnqueue] = (end_tick-start_tick);
				__sync_fetch_and_add(&numEnqueue,1);
				pthread_mutex_unlock(&lock);
#endif

			}
#ifdef THROUGHPUT
			clock_gettime(CLOCK_MONOTONIC, &tend);
			pthread_mutex_lock(&lock);
			double elapsed = ( tend.tv_sec - tstart.tv_sec ) + (( tend.tv_nsec - tstart.tv_nsec )/ 1E9);
			printf("elapsed time: %lf\n", elapsed);
			enqueuethroughput += ((NUM_SAMPLES*1.0)/elapsed);
			pthread_mutex_unlock(&lock);
#endif

//#ifdef LATENCY
//			ticks deq_start_tick, deq_end_tick;
//#endif
//#ifdef THROUGHPUT
//			//ticks st, et;
//			struct timespec deq_tstart, deq_tend;
//#endif
//
//#ifdef THROUGHPUT
//			//st = getticks();
//			clock_gettime(CLOCK_MONOTONIC, &deq_tstart);
//#endif
//#pragma omp parallel for
//			for (int i = 0; i < NUM_SAMPLES; i++)
//			{
//#ifdef LATENCY
//				deq_start_tick = getticks();
//#endif
//				atom ele = Dequeue();
//				printf("Dequeued %d\n", ele);
//#ifdef LATENCY
//				deq_end_tick = getticks();
//				pthread_mutex_lock(&lock);
//				dequeuetimestamp[numDequeue] = (deq_end_tick-deq_start_tick);
//				//printf("%d\n", numDequeue);
//				__sync_fetch_and_add(&numDequeue,1);
//				pthread_mutex_unlock(&lock);
//#endif
//			}
//#ifdef THROUGHPUT
//			//et = getticks();
//			clock_gettime(CLOCK_MONOTONIC, &deq_tend);
//			pthread_mutex_lock(&lock);
//			//ticks diff_tick = et - st;
//			//double elapsed = (diff_tick/clockFreq);
//			elapsed = ( deq_tend.tv_sec - deq_tstart.tv_sec ) + (( deq_tend.tv_nsec - deq_tstart.tv_nsec )/ 1E9);
//			printf("elapsed time: %lf\n", elapsed);
//			//dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1000000000.0)/elapsed);
//			dequeuethroughput += ((NUM_SAMPLES*1.0)/elapsed);
//			pthread_mutex_unlock(&lock);
//#endif
#ifdef LATENCY
			ticks deq_start_tick, deq_end_tick;
#endif
#ifdef THROUGHPUT
			//ticks st, et;
			struct timespec deq_tstart, deq_tend;
#endif

#ifdef THROUGHPUT
			//st = getticks();
			clock_gettime(CLOCK_MONOTONIC, &deq_tstart);
#endif
#pragma omp parallel for
			for (int i = 0; i < NUM_SAMPLES; i++)
			{
#ifdef LATENCY
				deq_start_tick = getticks();
#endif
				Dequeue();
#ifdef LATENCY
				deq_end_tick = getticks();
				pthread_mutex_lock(&lock);
				dequeuetimestamp[numDequeue] = (deq_end_tick-deq_start_tick);
				//printf("%d\n", numDequeue);
				__sync_fetch_and_add(&numDequeue,1);
				pthread_mutex_unlock(&lock);
#endif
			}
#ifdef THROUGHPUT
			//et = getticks();
			clock_gettime(CLOCK_MONOTONIC, &deq_tend);
			pthread_mutex_lock(&lock);
			//ticks diff_tick = et - st;
			//double elapsed = (diff_tick/clockFreq);
			elapsed = ( deq_tend.tv_sec - deq_tstart.tv_sec ) + (( deq_tend.tv_nsec - deq_tstart.tv_nsec )/ 1E9);
			printf("elapsed time: %lf\n", elapsed);
			//dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1000000000.0)/elapsed);
			dequeuethroughput += ((NUM_SAMPLES*1.0)/elapsed);
			pthread_mutex_unlock(&lock);
#endif

			ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp, rdtsc_overhead_ticks);

			free(enqueuetimestamp);
			free(dequeuetimestamp);
		}
		break;
	default:
		break;
	}

	//#ifdef RAW
	fclose(rfp);
	//#endif

	fclose(afp);
	printf("Done!!\n");

	return 0;
}
