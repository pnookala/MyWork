/*
 ============================================================================
 Name        : SimpleQueue.c
 Author      : Poornima Nookala
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include "queue.h"
#include "semiblockingqueue.h"
#include "lockfreequeue.h"
#include "queue_atomic.h"
#include "blockingqueue.h"
#include "basicqueue.h"

#define NUM_TASKS 8//10000000
#define TOTAL_THREADS 1
static int NUM_THREADS = TOTAL_THREADS;

int dequeuedItemCount = 0;
ticks dequeue_ticks;

volatile int cond = 0;

ticks *enqueuetimestamp, *dequeuetimestamp;

static __inline__ ticks getticks(void)
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
static __inline__ ticks getticks_old(void)
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

static void *malloc_aligned(unsigned int size)
{
	void *ptr;
	int r = posix_memalign(&ptr, 256, size);
	if (r != 0) {
		perror("malloc error");
		abort();
	}
	memset(ptr, 0, size);
	return ptr;
}

void testblockingqueue_singlethread(ticks *enqueuetimestamp, ticks *dequeuetimestamp)
{
	int i;
	ticks start_tick,end_tick;
	struct queue *q;

	q = create_queue(QUEUE_SIZE);

	for(i=0;i<NUM_TASKS;i++)
	{
		struct task_desc task;
		task.task_id = i;
		task.task_type = 1;
		task.num_threads = 1;
		task.params = (void*)(5);

		start_tick = getticks();
		enqueue(task, q);
		end_tick = getticks();

		enqueuetimestamp[i] = end_tick - start_tick;
	}

	for(i=0;i<NUM_TASKS;i++)
	{
		struct task_desc task;

		start_tick = getticks();
		task = dequeue(q);
		end_tick = getticks();

		dequeuetimestamp[i] = end_tick - start_tick;
	}
}

void testnonblocking_atomicqueue_singlethread(ticks *enqueuetimestamp, ticks *dequeuetimestamp)
{
	int i;
	ticks start_tick,end_tick;
	struct queue *q;

	q = create_queue_atomic(QUEUE_SIZE);

	for(i=1;i<=NUM_TASKS;i++)
	{
		struct task_desc task;
		task.task_id = i;
		task.task_type = 1;
		task.num_threads = 1;
		task.params = (void*)(5);

		start_tick = getticks();
		enqueue_atomic(task, q);
		end_tick = getticks();

		enqueuetimestamp[i] = end_tick - start_tick;
	}

	for(i=1;i<=NUM_TASKS;i++)
	{
		struct task_desc task;

		start_tick = getticks();
		task = dequeue_atomic(q);
		end_tick = getticks();

		dequeuetimestamp[i] = end_tick - start_tick;
	}
}

void testblocking_queue_singlethread(ticks *enqueuetimestamp, ticks *dequeuetimestamp)
{
	int i;
	ticks start_tick,end_tick;
	struct queue *q;

	q = create_queue_blocking(QUEUE_SIZE);

	for(i=0;i<NUM_TASKS;i++)
	{
		struct task_desc task;
		task.task_id = i;
		task.task_type = 1;
		task.num_threads = 1;
		task.params = (void*)(5);

		start_tick = getticks();
		enqueue_blocking(task, q);
		end_tick = getticks();

		enqueuetimestamp[i] = end_tick - start_tick;
	}

	for(i=0;i<NUM_TASKS;i++)
	{
		struct task_desc task;

		start_tick = getticks();
		task = dequeue_blocking(q);
		end_tick = getticks();

		dequeuetimestamp[i] = end_tick - start_tick;
	}
}

void *worker_handler(void *_queue)
{
	struct queue *queue = (struct queue *)_queue;
	ticks start_tick,end_tick;
	int i=100;
	struct task_desc value;

	int NUM_SAMPLES_PER_THREAD = NUM_TASKS/NUM_THREADS;
	for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
	{
		start_tick = getticks();
		value = dequeue_blocking(queue);
		end_tick = getticks();
		//s__asm__ __volatile__("" : : : "memory");
		dequeuetimestamp[value.task_id] = end_tick - start_tick;

		//sched_yield();
		//CHECK_COND(cond);
	}
}

void testblockingqueue_multiplethreads(ticks *enqueuetimestamp, ticks *dequeuetimestamp)
{
	int i = 0;
	ticks start_tick,end_tick;
	struct queue *q = create_queue(QUEUE_SIZE);
	pthread_t *worker_threads;

	worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * NUM_THREADS);

	for(i=0;i<NUM_TASKS;i++)
	{
		struct task_desc task;
		task.task_id = i;
		task.task_type = 1;
		task.num_threads = 1;
		task.params = (void*)(5);

		start_tick = getticks();
		enqueue_blocking(task, q);
		end_tick = getticks();

		enqueuetimestamp[i] = end_tick - start_tick;
	}

	for (i = 0; i < NUM_THREADS; i++)
		pthread_create(&worker_threads[i], NULL, worker_handler, q);

	for (i = 0; i < NUM_THREADS; i++)
		pthread_join(worker_threads[i], NULL);
}

/********************************************************************************************************************************/

void *atomicworker_handler(void *_queue)
{
	struct queue *queue = (struct queue *)_queue;
	ticks start_tick,end_tick;
	struct task_desc value;
	int NUM_SAMPLES_PER_THREAD = NUM_TASKS/NUM_THREADS;

	for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
	{
		start_tick = getticks();
		value = dequeue_atomic(queue);
		end_tick = getticks();
		dequeuetimestamp[value.task_id-1] = end_tick - start_tick;
		//sched_yield();
	}
}

void *enqueue_handler(void *_queue)
{
	struct queue *q = (struct queue *)_queue;

	ticks start_tick,end_tick;
	int i;

	for(i=1;i<=NUM_TASKS;i++)
	{
		struct task_desc task;
		task.task_id = i;
		task.task_type = 1;
		task.num_threads = 1;
		task.params = (void*)(5);

		start_tick = getticks();
		enqueue_atomic(task, q);
		end_tick = getticks();

		enqueuetimestamp[i-1] = end_tick - start_tick;
	}
}

void testatomicqueue_multiplethreads(ticks *enqueuetimestamp, ticks *dequeuetimestamp)
{
	int i = 0;
	ticks start_tick,end_tick;
	struct queue *q = create_queue_atomic(QUEUE_SIZE);
	pthread_t *worker_threads;

	worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * NUM_THREADS);

	for (i = 0; i < NUM_THREADS; i++)
		pthread_create(&worker_threads[i], NULL, atomicworker_handler, q);

	for(i=1;i<=NUM_TASKS;i++)
	{
		struct task_desc task;
		task.task_id = i;
		task.task_type = 1;
		task.num_threads = 1;
		task.params = (void*)(5);

		start_tick = getticks();
		enqueue_atomic(task, q);
		end_tick = getticks();

		enqueuetimestamp[i-1] = end_tick - start_tick;
	}

	for (int i = 0; i < NUM_THREADS; i++)
		pthread_join(worker_threads[i], NULL);
}

/*********************************************************************************************************************************/


void testsemiblockingqueue_singlethread(ticks *enqueuetimestamp, ticks *dequeuetimestamp)
{
	struct queue_root *queue = ALLOC_QUEUE_ROOT();
	int i;
	ticks start_tick,end_tick;
	struct queue_head *item = malloc_aligned(sizeof(struct queue_head));
	INIT_QUEUE_HEAD(item);

	for(i=0;i<NUM_TASKS;i++)
	{
		struct queue_head *item = malloc_aligned(sizeof(struct queue_head));
		start_tick = getticks();
		queue_put(item, queue);
		end_tick = getticks();

		enqueuetimestamp[i] = end_tick - start_tick;
	}

	for(i=0;i<NUM_TASKS;i++)
	{
		start_tick = getticks();
		item = queue_get(queue);
		end_tick = getticks();

		dequeuetimestamp[i] = end_tick - start_tick;
	}
}

void testlockfreequeue_singlethread(ticks *enqueuetimestamp, ticks *dequeuetimestamp)
{
	int i = 0;
	ticks start_tick,end_tick;
	Queue *queue = q_initialize();
	struct task_desc *value = malloc(NUM_TASKS* sizeof(struct task_desc));

	for(i=0;i<NUM_TASKS;i++)
	{
		value[i].task_id = i;
		value[i].task_type = 1;
		value[i].num_threads = 1;
		value[i].params = (void*)(5);
		start_tick = getticks();
		qpush(queue,&value[i]);
		end_tick = getticks();

		enqueuetimestamp[i] = end_tick - start_tick;
	}

	for(i=0;i<NUM_TASKS;i++)
	{
		struct task_desc *value = NULL;
		start_tick = getticks();
		value = qpop(queue,(unsigned int)pthread_self());
		//printf("%d \n", value->task_id);
		end_tick = getticks();

		dequeuetimestamp[i] = end_tick - start_tick;
	}

	free(value);
	queue_free(queue);
}

void *consumer(void *_queue)
{
	Queue *queue = (Queue *)_queue;
	ticks start_tick,end_tick;
	int i=100;
	struct task_desc *value = NULL;

	for(;;)
	{
		start_tick = getticks();
		value = qpop(queue,(unsigned int)pthread_self());
		if(value != NULL)
		{
			end_tick = getticks();
			//__asm__ __volatile__("" : : : "memory");
			dequeuetimestamp[value->task_id] = end_tick - start_tick;
		}

		sched_yield();
		value = NULL;
		CHECK_COND(cond);
	}
}

void testlockfreequeue_multiplethreads(ticks *enqueuetimestamp, ticks *dequeuetimestamp)
{
	int i = 0;
	ticks start_tick,end_tick;
	Queue *queue = q_initialize();
	struct task_desc *value = malloc(NUM_TASKS* sizeof(struct task_desc));
	pthread_t *worker_threads;

	worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * NUM_THREADS);

	for (i = 0; i < NUM_THREADS; i++)
		pthread_create(&worker_threads[i], NULL, consumer, queue);

	for(i=0;i<NUM_TASKS;i++)
	{
		value[i].task_id = i;
		value[i].task_type = 1;
		value[i].num_threads = 1;
		value[i].params = (void*)(5);
		start_tick = getticks();
		qpush(queue,&value[i]);
		end_tick = getticks();

		enqueuetimestamp[value[i].task_id] = end_tick - start_tick;
	}
}

void *basicworker_handler(void *_queue)
{
	ticks start_tick,end_tick;
	int value = 0;

	int NUM_SAMPLES_PER_THREAD = NUM_TASKS/NUM_THREADS;
	for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
	{
		start_tick = getticks();
		value = BasicDequeue();
		end_tick = getticks();
		__sync_add_and_fetch(&dequeue_ticks,(end_tick-start_tick));
	}
}

int main(void)
{
	int i,k;
	FILE *fp;
	enqueuetimestamp = (ticks *)calloc(NUM_TASKS, sizeof(ticks));
	dequeuetimestamp = (ticks *)calloc(NUM_TASKS, sizeof(ticks));

	ticks totalEnqueueTicks = 0;
	ticks totalDequeueTicks = 0;
	ticks tickEnMin = 0;
	ticks tickEnMax = 0;
	ticks tickEnAverage = 0;
	ticks tickEnStdDev = 0;
	ticks tickDeMin = 0;
	ticks tickDeMax = 0;
	ticks tickDeAverage = 0;
	ticks tickDeStdDev = 0;

	/*for (int k=1;k<=TOTAL_THREADS;k=k*2)
	{
		NUM_THREADS = k;
		testblockingqueue_multiplethreads(enqueuetimestamp, dequeuetimestamp);

		/* char* filename = "blocking_queue_data.txt";
		if( access( filename, F_OK ) != -1 ) {
			// file exists
			fp=fopen(filename, "a");
		} else {
			// file doesn't exist
			fp=fopen(filename, "w");
		}
	if(fp == NULL)
		exit(-1);

	fprintf(fp, "Tasks,Threads,TaskID,EnqueueCycleCount,DequeueCycleCount\n");

		printf("Queue,Operation,NumSamples,CycleCount,NumThreads\n");

		tickEnMin = enqueuetimestamp[0];
		tickDeMin = dequeuetimestamp[0];

		for(i=0;i<NUM_TASKS;i++)
		{
			totalEnqueueTicks += enqueuetimestamp[i];
			totalDequeueTicks += dequeuetimestamp[i];

			if(tickEnMin > enqueuetimestamp[i])
				tickEnMin = enqueuetimestamp[i];

			if(tickEnMax < enqueuetimestamp[i])
				tickEnMax = enqueuetimestamp[i];

			//Calculate time using simple formula ns = CPU cycles * (ns_per_sec / CPU freq)
			//double enqueuens = enqueuetimestamp[i] * (1000000000/2.4);

			//fprintf(fp, "%d,%d,%d,%llu,%llu\n", NUM_TASKS, NUM_THREADS, (i+1), enqueuetimestamp[i], dequeuetimestamp[i]);

			if(tickDeMin > dequeuetimestamp[i])
				tickDeMin = dequeuetimestamp[i];

			if(tickDeMax < dequeuetimestamp[i])
				tickDeMax = dequeuetimestamp[i];
		}

		tickEnAverage = (totalEnqueueTicks*1.0)/NUM_TASKS;
		tickDeAverage = (totalDequeueTicks*1.0)/NUM_TASKS;

		//compute standard deviation
		for (i=0;i<NUM_TASKS;i++)
			tickEnStdDev += pow(enqueuetimestamp[i] - tickEnAverage, 2);

		//tickEnStdDev = sqrt(tickEnStdDev/(NUM_TASKS));

		for (i=0;i<NUM_TASKS;i++)
			tickDeStdDev += pow(dequeuetimestamp[i] - tickDeAverage, 2);

		//tickDeStdDev = sqrt(tickDeStdDev/(NUM_TASKS));
		/*printf("Blocking Queue Results for %d threads\n", NUM_THREADS);
	printf("Samples: %i\n", NUM_TASKS);
	printf("Enqueue Min: %llu\n", tickEnMin);
	printf("Enqueue Average: %u\n", tickEnAverage);
	printf("Enqueue Max: %llu\n", tickEnMax);
	printf("Enqueue StdDev: %u\n", tickEnStdDev);

	printf("Samples: %i\n", NUM_TASKS);
	printf("Dequeue Min: %llu\n", tickDeMin);
	printf("Dequeue Average: %u\n", tickDeAverage);
	printf("Dequeue Max: %llu\n", tickDeMax);
	printf("Dequeue StdDev: %u\n", tickDeStdDev);
	fclose(fp);

		printf("blocking,enqueue,%d,%llu,%d\n", NUM_TASKS, tickEnAverage, NUM_THREADS);
		printf("blocking,dequeue,%d,%llu,%d\n", NUM_TASKS, tickDeAverage, NUM_THREADS);
	}
	/****************************************************/
	//Change method to test different types of queues
	/*for (int k=1;k<=TOTAL_THREADS;k=k*2)
	{
		NUM_THREADS = k;
		testatomicqueue_multiplethreads(enqueuetimestamp, dequeuetimestamp);

		/*char* filename = "atomic_queue_data.txt";
				if( access( filename, F_OK ) != -1 ) {
					// file exists
					fp=fopen(filename, "a");
				} else {
					// file doesn't exist
					fp=fopen(filename, "w");
				}
		if(fp == NULL)
			exit(-1);
		fprintf(fp, "Tasks,Threads,TaskID,EnqueueCycleCount,DequeueCycleCount\n");
		printf("Queue,Operation,NumSamples,CycleCount,NumThreads\n");
		tickEnMin = enqueuetimestamp[0];
		tickDeMin = dequeuetimestamp[0];

		for(i=0;i<NUM_TASKS;i++)
		{
			totalEnqueueTicks += enqueuetimestamp[i];
			totalDequeueTicks += dequeuetimestamp[i];

			if(tickEnMin > enqueuetimestamp[i])
				tickEnMin = enqueuetimestamp[i];

			if(tickEnMax < enqueuetimestamp[i])
				tickEnMax = enqueuetimestamp[i];

			//Calculate time using simple formula ns = CPU cycles * (ns_per_sec / CPU freq)
			//double enqueuens = enqueuetimestamp[i] * (1000000000/2.4);

			//fprintf(fp, "%d,%d,%d,%llu,%llu\n", NUM_TASKS, NUM_THREADS, (i+1), enqueuetimestamp[i], dequeuetimestamp[i]);

			if(tickDeMin > dequeuetimestamp[i])
				tickDeMin = dequeuetimestamp[i];

			if(tickDeMax < dequeuetimestamp[i])
				tickDeMax = dequeuetimestamp[i];
		}

		tickEnAverage = (totalEnqueueTicks*1.0)/NUM_TASKS;
		tickDeAverage = (totalDequeueTicks*1.0)/NUM_TASKS;

		//compute standard deviation
		for (i=0;i<NUM_TASKS;i++)
			tickEnStdDev += pow(enqueuetimestamp[i] - tickEnAverage, 2);

		//tickEnStdDev = sqrt(tickEnStdDev/(NUM_TASKS));

		for (i=0;i<NUM_TASKS;i++)
			tickDeStdDev += pow(dequeuetimestamp[i] - tickDeAverage, 2);

		printf("atomic,enqueue,%d,%llu,%d\n", NUM_TASKS, tickEnAverage, NUM_THREADS);
		printf("atomic,dequeue,%d,%llu,%d\n", NUM_TASKS, tickDeAverage, NUM_THREADS);
	}*/
	for (int k=1;k<=TOTAL_THREADS;k=k*2)
	{
		InitBasicQueue();
		NUM_THREADS = k;
		ticks start_tick,end_tick,diff_tick;
		pthread_t *worker_threads;

		worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * NUM_THREADS);

		start_tick = getticks();
		for (i=0;i<NUM_TASKS;i++)
		{

			BasicEnqueue((i+1));

			//__sync_fetch_and_add(&numEnqueue,1);

		}
		end_tick = getticks();

		for (i = 0; i < NUM_THREADS; i++)
			pthread_create(&worker_threads[i], NULL, basicworker_handler, NULL);

		diff_tick = end_tick - start_tick;
		printf("Queue,Operation,NumSamples,CycleCount,NumThreads\n");
		printf("basic,enqueue,%d,%llu,%d\n", NUM_TASKS, diff_tick/NUM_TASKS, NUM_THREADS);

		for (int i = 0; i < NUM_THREADS; i++)
			pthread_join(worker_threads[i], NULL);

		printf("basic,dequeue,%d,%llu,%d\n", NUM_TASKS, dequeue_ticks/NUM_TASKS, NUM_THREADS);
	}


	free(enqueuetimestamp);
	free(dequeuetimestamp);

	return EXIT_SUCCESS;
}
