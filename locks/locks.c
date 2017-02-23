/*
 * cyclecount.c
 *
 *  Created on: Jan 25, 2017
 *      Author: Ioan Raicu
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

typedef unsigned long long ticks;
#define NUM_SAMPLES 1000001
#define TOTAL_THREADS 4
static int NUM_THREADS = 1;


static ticks dequeue_ticks = 0;


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

#define rdtscll(val)                    \
do {                        \
uint64_t tsc;                   \
uint32_t a, d;                  \
asm volatile("rdtsc" : "=a" (a), "=d" (d)); \
*(uint32_t *)&(tsc) = a;            \
*(uint32_t *)(((unsigned char *)&tsc) + 4) = d;   \
val = tsc;                  \
} while (0)

static int counter = 0;

static inline void spinlock(volatile int *lock)
{
    while(!__sync_bool_compare_and_swap(lock, 0, 1))
    {
        sched_yield();
    }
}

static inline void spinunlock(volatile int *lock)
{
    *lock = 0;
}

void *worker_handler(int run)
{
	int localcounter = 0;
    if (NUM_THREADS == 0)
        printf("need %fGB of memory\n", sizeof(ticks)*NUM_SAMPLES*2.0/(1024*1024*1024));
    //dynamically allocate memory to hold data samples
    ticks *timestamp;
    timestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
    ticks *numTicks;
    numTicks = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES-1);
    
    ticks start_tick,end_tick,diff_tick;
    
    //measure the cost of the empty loop and write of a ticks variable
    //also initialize timestamp[] to 0
    start_tick = getticks();
    
    for (int i=0;i<NUM_SAMPLES;i++)
    {
        timestamp[i] = (ticks)0;
    }
    end_tick = getticks();
    diff_tick=end_tick-start_tick;
    //double diff_tick_per_iter=diff_tick/((NUM_SAMPLES-1)*1.0);
    double diff_tick_per_iter = 0;
    int lock = 0;
    sem_t semvar;
    //sem_init(&semvar, 0, 0);
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    timestamp[0] = getticks();
    
    for (int i=0;i<NUM_SAMPLES;i++)
    {
        //atomic (about 40 cycles, of which 18 cycles is the timing and loop ==> ~22 cycles, single threaded)
        if (run == 0) __sync_fetch_and_add(&counter,1);
        //semaphor (about 782 cycles, of which 18 cycles is the timing and loop ==> ~764 cycles, single threaded)
        else if (run == 1)
        {
        	sem_post(&semvar);
        	counter++;
        	sem_wait(&semvar);
        }
            //mutex (about 70 cycles, of which 18 cycles is the timing and loop ==> ~52 cycles, single threaded)
        else if (run == 2)
        {
            pthread_mutex_lock(&mutex);
            counter++;
            pthread_mutex_unlock(&mutex);
        }
        else if (run == 3)
        {
            //spin lock (about 40 cycles, of which 18 cycles is the timing and loop ==> ~22 cycles, single threaded)
            spinlock(&lock);
            counter++;
            spinunlock(&lock);
            
        }
        //no lock (no effect on cost asside from getticks() cost ~ 18 cycles)
        else if(run == 4) counter++;
        else
        {
        	localcounter++;
        }
        
        timestamp[i] = getticks();
    }
    
    //write all data samples to file
    //FILE *fp;
    //fp=fopen("data.txt", "w");
    //if(fp == NULL)
    //    exit(-1);
    
    ticks totalTicks = 0;
    ticks tickMin = (timestamp[1]-timestamp[0]-diff_tick_per_iter);
    ticks tickMax = (timestamp[1]-timestamp[0]-diff_tick_per_iter);
    
    //compute the elapsed time per invocation, and find min and max
    for (int i=0;i<NUM_SAMPLES-1;i++)
    {
        //compute the elapsed time per invocation, and subtract the cost of the emtpy loop cost per iteration
        numTicks[i]=timestamp[i+1]-timestamp[i]-diff_tick_per_iter;
        //fprintf(fp, "%llu\n", numTicks[i]);
        totalTicks += numTicks[i];
        if (numTicks[i]>tickMax) tickMax = numTicks[i];
        if (numTicks[i]<tickMin) tickMin = numTicks[i];
    }
    //fclose(fp);
    //compute average
    double tickAverage = (totalTicks*1.0/(NUM_SAMPLES-1));
    double tickStdDev = 0;
    
    //static ticks dequeue_ticks = 0;
    __sync_add_and_fetch(&dequeue_ticks,totalTicks/(NUM_SAMPLES-1));
    
    //compute standard deviation
    for (int i=0;i<NUM_SAMPLES-1;i++)
        tickStdDev += pow(numTicks[i] - tickAverage, 2);
    
    tickStdDev = sqrt(tickStdDev/(NUM_SAMPLES-1));
    
    //print summary statistics
    //printf("Loop-cycles: %llu\n",end_tick-start_tick);
    if (NUM_THREADS == 0)
    {
    printf("Loop-cycles-per-iter: %f\n",diff_tick_per_iter);
    printf("Samples: %i\n", NUM_SAMPLES-1);
    //printf("TotalTicks: %llu\n", totalTicks);
    printf("Min: %llu\n", tickMin);
    
    
    printf("Average(%d): %f cycles with %d threads\n", run, tickAverage, NUM_THREADS);
    
        printf("Max: %llu\n", tickMax);
    printf("StdDev: %f\n", tickStdDev);
    }
    
    /*
    ticks start_tick,end_tick,diff_tick;
    
    atom value;
    for(;;)
    {
        start_tick = getticks();
        value = Dequeue();
        end_tick = getticks();
        diff_tick = end_tick - start_tick;
        
        __sync_fetch_and_add(&numDequeue,1);
        
    }*/

    free(timestamp);
    free(numTicks);
}




int main(int argc, char *argv[])
{

    printf("Operation,NumSamples,CycleCount,NumThreads\n");

    
    char *runName[] = {"atomic","semaphor","mutex","spinlock","vanila shared", "vanilla unique"};
    
    for (int k=1;k<=TOTAL_THREADS;k=k*2)
    {
        NUM_THREADS = k;
        
    for (int run=0;run<6;run++)
    {
        dequeue_ticks=0;

        //printf("run: %d with %d threads\n", run, NUM_THREADS);
        pthread_t *worker_threads;
        worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&worker_threads[i], NULL, worker_handler, run);

    
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(worker_threads[i], NULL);

        //printf("dequeue,%d,%llu,%d\n", NUM_SAMPLES, dequeue_ticks/NUM_SAMPLES, k);
        printf("%s,%d,%llu,%d\n", runName[run], NUM_SAMPLES-1, dequeue_ticks/NUM_THREADS, NUM_THREADS);
    
    //for (int i = 0; i < NUM_THREADS; i++)
    //    pthread_exit(worker_threads[i]);

    }
    }
    
    
    printf("\nFinished!\n");
    
	return 0;
}
