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
#include "xtaskqueue.h"
#include <sys/time.h>
#ifndef PHI
#include <urcu.h>  /* RCU flavor */
#include <urcu/rculfqueue.h> /* RCU Lock-free queue */
#include <urcu/compiler.h> /* For CAA_ARRAY_SIZE */
#include "liblfds711.h"
#ifdef HWLOC
#include <hwloc.h>
#include <hwloc/cpuset.h>
#endif

/*
 * Nodes populated into the queue.
 */
#ifdef HWLOC
static hwloc_topology_t topology = NULL; /* Topology object */
#endif

struct mynode {
    int value; /* Node content */
    struct cds_lfq_node_rcu node; /* Chaining in queue */
    struct rcu_head rcu_head; /* For call_rcu() */
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
#define NUM_CPUS 24
#define ENQUEUE_SECONDS 3.0
#define DEQUEUE_SECONDS 3.0

ticks *enqueuetimestamp, *dequeuetimestamp;
static ticks dequeue_ticks = 0, enqueue_ticks = 0;

static int numEnqueue = 0;
static int numDequeue = 0;
static int CUR_NUM_THREADS = 0;
static int NUM_QUEUES = 1;
static long long int ENQUEUE_SAMPLES = 0;
static long long int DEQUEUE_SAMPLES = 0;

volatile int numEnqueueThreadsCreated = 0, numDequeueThreadsCreated = 0;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;
pthread_mutex_t cond_var_lock = PTHREAD_MUTEX_INITIALIZER;

double enqueuethroughput, dequeuethroughput = 0;
static pthread_barrier_t barrier;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#ifndef PHI
struct cds_lfq_queue_rcu myqueue; /* Queue */
static int failed_ck_dequeues = 0;

int *pick; //For picking the queue for load balancing

static
void free_node(struct rcu_head *head) {
    struct mynode *node = caa_container_of(head, struct mynode, rcu_head);

    free(node);
}

//An alternative way is to use rdtscp which will wait until all previous instructions have been executed before reading the counter; might be problematic on multi-core machines

#ifndef ARM

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

#ifdef ARM

static void enable_ccnt_read(void* data) {
    // WRITE PMUSERENR = 1
    //__asm__ __volatile__ ("mcr p15, 0, %0, c9, c14, 0\n\t" : : "r" (1));

    uint32_t pmccntr;
    uint32_t pmuseren;
    uint32_t pmcntenset;
    // Read the user mode perf monitor counter access permissions.
    __asm__ __volatile__ ("mrc p15, 0, %0, c9, c14, 0" : "=r" (pmuseren));
    printf("pmuseren : %d\n", pmuseren);
    if (pmuseren & 1) { // Allows reading perfmon counters for user mode code.
        {
            __asm__ __volatile__ ("mrc p15, 0, %0, c9, c12, 1" : "=r" (pmcntenset));
            printf("pmcntenset : %d\n", pmcntenset);
        }
        if (pmcntenset & 0x80000000ul) { // Is it counting?
            __asm__ __volatile__ ("mrc p15, 0, %0, c9, c13, 0" : "=r" (pmccntr));
            // The counter is set up to count every 64th cycle
            return pmccntr << 6;
        }
    }
}

static __inline__ ticks getticks() {
    volatile unsigned cc;
    __asm__ __volatile__ ("mrc p15, 0, %0, c9, c13, 0" : "=r" (cc));
    return cc;
}

#endif

static __inline__ ticks getticks_cpuid(void) {
    register unsigned a, d;
    __asm__ __volatile__ (
            "cpuid\n\t"
            "rdtsc\n\t"
            : "=a" (a),
            "=d" (d)
            :
            : "ebx", "ecx"
            );
    return (((uint64_t) d << 32) | a);
}

static __inline__ void cyclesleep(ticks numTicks) {
    if (numTicks <= 0) {
        sleep(0);
        return;
    }
    ticks st;
    st = getticks();
    while (getticks() < (st + numTicks));
    return;
}

static inline unsigned long getticks_phi() {
    unsigned int hi, lo;

    __asm volatile (
            "xorl %%eax, %%eax nt"
            "cpuid             nt"
            "rdtsc             nt"
            : "=a"(lo), "=d"(hi)
            :
            : "%ebx", "%ecx"
            );
    return ((unsigned long) hi << 32) | lo;
}

void *worker_handler(void * in) {
    int my_cpu = (int) (long) in;
#ifdef HWLOC
    hwloc_obj_t obj; /* Hwloc object    */
    hwloc_cpuset_t cpuset; /* HwLoc cpuset    */
    obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, my_cpu % NUM_CPUS);
    cpuset = hwloc_cpuset_dup(obj->cpuset);
    hwloc_cpuset_singlify(cpuset);

    hwloc_set_cpubind(topology, cpuset, HWLOC_CPUBIND_THREAD);

    char *str = NULL;
    hwloc_cpuset_asprintf(&str, obj->cpuset);
    printf("Thread bound to cpuset %s\n", str);
#else

    cpu_set_t set;

    CPU_ZERO(&set);
    CPU_SET(my_cpu % NUM_CPUS, &set);

    pthread_setaffinity_np(pthread_self(), sizeof (set), &set);
#ifdef ARM
    enable_ccnt_read(NULL);
#endif
    printf("Tried to pin to %d, Worker Thread on CPU %d\n", my_cpu, sched_getcpu());
#endif

#ifdef LATENCY
    ticks start_tick, end_tick;
    int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
    ticks *timestamp;
    timestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES_PER_THREAD);
    for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
        timestamp[i] = (ticks) 0;
    }
#endif

#ifdef THROUGHPUT
    struct timespec looptime, loopend;
    struct timespec tstart, tend;
#endif

    pthread_barrier_wait(&barrier);
#ifdef VERBOSE
    printf("Dequeue thread woke up\n");
#endif
        
#ifdef THROUGHPUT
    int ret;
    long int NUM_SAMPLES_PER_THREAD = 0;
    int count = 1;
    double diff = 0.0;
    clock_gettime(CLOCK_MONOTONIC, &looptime);
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    while (diff <= DEQUEUE_SECONDS && ret != -1) {
#endif
#ifdef LATENCY
        for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
            start_tick = getticks();
#endif
#ifdef THROUGHPUT
            ret = Dequeue();
#else
            Dequeue();
#endif
#ifdef LATENCY
            end_tick = getticks();
            /*pthread_mutex_lock(&lock);
            dequeuetimestamp[numDequeue] = (end_tick-start_tick);
            __sync_fetch_and_add(&numDequeue, 1);
            pthread_mutex_unlock(&lock);*/
            timestamp[i] = end_tick - start_tick;
#endif
#ifdef THROUGHPUT
            count++;
            if (count % 10000 == 0 || ret == -1) {
                clock_gettime(CLOCK_MONOTONIC, &loopend);
                NUM_SAMPLES_PER_THREAD += count;
                count = 1;
                diff = (loopend.tv_sec - looptime.tv_sec);
            }
#endif
        }
#ifdef LATENCY
        //		ticks totalTicks = 0;
        //		for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
        //			{
        //				totalTicks += timestamp[i];
        //			}

        //__sync_add_and_fetch(&dequeue_ticks,totalTicks);
        //Merge local array to global array.
        pthread_mutex_lock(&lock);
        memcpy(dequeuetimestamp + numDequeue, timestamp, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
        numDequeue += NUM_SAMPLES_PER_THREAD;
        pthread_mutex_unlock(&lock);
#endif

#ifdef THROUGHPUT
        clock_gettime(CLOCK_MONOTONIC, &tend);
        pthread_mutex_lock(&lock);
        double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
        printf("elapsed time: %lf\n", elapsed);
        printf("Num dequeue tasks run: %d\n", NUM_SAMPLES_PER_THREAD);
        dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
        DEQUEUE_SAMPLES += NUM_SAMPLES_PER_THREAD;
        pthread_mutex_unlock(&lock);
#endif

        return 0;

    }

    void *enqueue_handler(void * in) {
        int my_cpu = (int) (long) in;

#ifdef HWLOC
        hwloc_obj_t obj; /* Hwloc object    */
        hwloc_cpuset_t cpuset; /* HwLoc cpuset    */
        obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, my_cpu % NUM_CPUS);
        cpuset = hwloc_cpuset_dup(obj->cpuset);
        hwloc_cpuset_singlify(cpuset);

        hwloc_set_cpubind(topology, cpuset, HWLOC_CPUBIND_THREAD);

        char *str = NULL;
        hwloc_cpuset_asprintf(&str, obj->cpuset);
        printf("Thread bound to cpuset %s\n", str);
#else

        cpu_set_t set;

        CPU_ZERO(&set);
        CPU_SET(my_cpu % NUM_CPUS, &set);

        pthread_setaffinity_np(pthread_self(), sizeof (set), &set);

#ifdef ARM
        enable_ccnt_read(NULL);
#endif
        printf("Tried to pin to %d, Worker Thread on CPU %d\n", my_cpu, sched_getcpu());
#endif

#ifdef LATENCY
        ticks start_tick, end_tick;
        int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
        ticks *timestamp;
        timestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES_PER_THREAD);
        for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
            timestamp[i] = (ticks) 0;
        }
#endif

#ifdef THROUGHPUT
        struct timespec tstart, tend, looptime, loopend;
        int i = 1;
        long int NUM_SAMPLES_PER_THREAD = 0;
        double diff = 0.0;
#endif

        pthread_barrier_wait(&barrier);

#ifdef THROUGHPUT
        clock_gettime(CLOCK_MONOTONIC, &looptime);
        clock_gettime(CLOCK_MONOTONIC, &tstart);
        while (diff <= ENQUEUE_SECONDS) {
#endif
#ifdef LATENCY
            for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                start_tick = getticks();
#endif
                Enqueue((atom) i + 1);
#ifdef LATENCY
                end_tick = getticks();
                /*pthread_mutex_lock(&lock);
                enqueuetimestamp[numEnqueue] = end_tick - start_tick;
                __sync_fetch_and_add(&numEnqueue, 1);
                pthread_mutex_unlock(&lock);*/
                timestamp[i] = end_tick - start_tick;
#endif
#ifdef THROUGHPUT
                i++;
                if (i % 10000 == 0) {
                    clock_gettime(CLOCK_MONOTONIC, &loopend);
                    NUM_SAMPLES_PER_THREAD += i;
                    i = 1;
                    diff = (loopend.tv_sec - looptime.tv_sec);
                }
#endif
            }
#ifdef LATENCY
            //			ticks totalTicks = 0;
            //		for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
            //			{
            //				totalTicks += timestamp[i];
            //			}
            //
            //			__sync_add_and_fetch(&enqueue_ticks,totalTicks);
            //printf("e, %llu, %llu\n", enqueue_ticks, totalTicks);
            pthread_mutex_lock(&lock);
            memcpy(enqueuetimestamp + numEnqueue, timestamp, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
            numEnqueue += NUM_SAMPLES_PER_THREAD;
            pthread_mutex_unlock(&lock);
#endif

#ifdef THROUGHPUT
            clock_gettime(CLOCK_MONOTONIC, &tend);
            pthread_mutex_lock(&lock);
            double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
            printf("elapsed time: %lf\n", elapsed);
            printf("Num enqueue tasks run: %d\n", NUM_SAMPLES_PER_THREAD);
            enqueuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
            ENQUEUE_SAMPLES += NUM_SAMPLES_PER_THREAD;
            pthread_mutex_unlock(&lock);
#endif

            return 0;
        }

        void *workermultiple_handler(void * in) {
            int my_cpu = (int) (long) in;
#ifdef HWLOC
            hwloc_obj_t obj; /* Hwloc object    */
            hwloc_cpuset_t cpuset; /* HwLoc cpuset    */
            obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, my_cpu % NUM_CPUS);
            cpuset = hwloc_cpuset_dup(obj->cpuset);
            hwloc_cpuset_singlify(cpuset);

            hwloc_set_cpubind(topology, cpuset, HWLOC_CPUBIND_THREAD);

            char *str = NULL;
            hwloc_cpuset_asprintf(&str, obj->cpuset);
            printf("Thread bound to cpuset %s\n", str);
#else

            cpu_set_t set;

            CPU_ZERO(&set);
            CPU_SET(my_cpu % NUM_CPUS, &set);

            pthread_setaffinity_np(pthread_self(), sizeof (set), &set);
#ifdef ARM
            enable_ccnt_read(NULL);
#endif
            printf("Tried to pin to %d, Worker Thread on CPU %d\n", my_cpu, sched_getcpu());
#endif

#ifdef LATENCY
            ticks start_tick, end_tick;
            int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
            ticks *timestamp;
            timestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES_PER_THREAD);
            for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                timestamp[i] = (ticks) 0;
            }
#endif

#ifdef THROUGHPUT
            struct timespec looptime, loopend;
            struct timespec tstart, tend;
#endif

            pthread_barrier_wait(&barrier);
#ifdef VERBOSE
            printf("Dequeue thread woke up\n");
#endif
            
#ifdef THROUGHPUT
            int ret;
            long int NUM_SAMPLES_PER_THREAD = 0;
            int count = 1;
            double diff = 0.0;
            clock_gettime(CLOCK_MONOTONIC, &looptime);
            clock_gettime(CLOCK_MONOTONIC, &tstart);
            while (diff <= DEQUEUE_SECONDS && ret != -1) {
#endif
#ifdef LATENCY
                for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                    start_tick = getticks();
#endif
#ifdef THROUGHPUT
                    ret = DequeueMultiple(queues[my_cpu], my_cpu);
#else
                    DequeueMultiple(queues[my_cpu], my_cpu);
#endif

#ifdef LATENCY
                    end_tick = getticks();
                    timestamp[i] = end_tick - start_tick;
                    //			pthread_mutex_lock(&lock);
                    //			dequeuetimestamp[numDequeue] = (end_tick-start_tick);
                    //			__sync_fetch_and_add(&numDequeue, 1);
                    //			pthread_mutex_unlock(&lock);
#endif
#ifdef THROUGHPUT
                    count++;
                    if (count % 10000 == 0 || ret == -1) {
                        clock_gettime(CLOCK_MONOTONIC, &loopend);
                        NUM_SAMPLES_PER_THREAD += count;
                        count = 1;
                        diff = (loopend.tv_sec - looptime.tv_sec);
                    }
#endif
                }
#ifdef LATENCY
                //		ticks totalTicks = 0;
                //		for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
                //			{
                //				totalTicks += timestamp[i];
                //			}
                //
                //			__sync_add_and_fetch(&dequeue_ticks,totalTicks);
                pthread_mutex_lock(&lock);
                memcpy(dequeuetimestamp + numDequeue, timestamp, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
                numDequeue += NUM_SAMPLES_PER_THREAD;
                pthread_mutex_unlock(&lock);
#endif

#ifdef THROUGHPUT
                clock_gettime(CLOCK_MONOTONIC, &tend);
                pthread_mutex_lock(&lock);
                double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
                printf("elapsed time: %lf\n", elapsed);
                printf("Num dequeue tasks run: %d\n", NUM_SAMPLES_PER_THREAD);
                dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
                DEQUEUE_SAMPLES += NUM_SAMPLES_PER_THREAD;
                pthread_mutex_unlock(&lock);
#endif

                return 0;

            }

            void *enqueuemultiple_handler(void * in) {
                int my_cpu = (int) (long) in;

#ifdef HWLOC
                hwloc_obj_t obj; /* Hwloc object    */
                hwloc_cpuset_t cpuset; /* HwLoc cpuset    */
                obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, my_cpu % NUM_CPUS);
                cpuset = hwloc_cpuset_dup(obj->cpuset);
                hwloc_cpuset_singlify(cpuset);

                hwloc_set_cpubind(topology, cpuset, HWLOC_CPUBIND_THREAD);

                char *str = NULL;
                hwloc_cpuset_asprintf(&str, obj->cpuset);
                printf("Thread bound to cpuset %s\n", str);
#else

                cpu_set_t set;

                CPU_ZERO(&set);
                CPU_SET(my_cpu % NUM_CPUS, &set);

                pthread_setaffinity_np(pthread_self(), sizeof (set), &set);
#ifdef ARM
                enable_ccnt_read(NULL);
#endif
                printf("Tried to pin to %d, Worker Thread on CPU %d\n", my_cpu, sched_getcpu());
#endif

#ifdef LATENCY
                ticks start_tick, end_tick;
                int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
                ticks *timestamp;
                timestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES_PER_THREAD);
                for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                    timestamp[i] = (ticks) 0;
                }
#endif

#ifdef THROUGHPUT
                struct timespec tstart, tend, looptime, loopend;
                int i = 1;
                long int NUM_SAMPLES_PER_THREAD = 0;
                double diff = 0.0;
#endif

                pthread_barrier_wait(&barrier);

#ifdef THROUGHPUT
                clock_gettime(CLOCK_MONOTONIC, &looptime);
                clock_gettime(CLOCK_MONOTONIC, &tstart);
                while (diff <= ENQUEUE_SECONDS) {
#endif
#ifdef LATENCY
                    for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                        start_tick = getticks();
#endif
                        EnqueueMultiple((atom) (i + 1), queues[my_cpu], my_cpu);
#ifdef LATENCY
                        end_tick = getticks();
                        timestamp[i] = end_tick - start_tick;
                        //				pthread_mutex_lock(&lock);
                        //				enqueuetimestamp[numEnqueue] = end_tick - start_tick;
                        //				__sync_fetch_and_add(&numEnqueue, 1);
                        //				pthread_mutex_unlock(&lock);
#endif
#ifdef THROUGHPUT
                        i++;
                        if (i % 10000 == 0) {
                            clock_gettime(CLOCK_MONOTONIC, &loopend);
                            NUM_SAMPLES_PER_THREAD += i;
                            i = 1;
                            diff = (loopend.tv_sec - looptime.tv_sec);
                        }
#endif
                    }
#ifdef LATENCY
                    //			ticks totalTicks = 0;
                    //		for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
                    //			{
                    //				totalTicks += timestamp[i];
                    //			}
                    //
                    //			__sync_add_and_fetch(&enqueue_ticks,totalTicks);
                    //printf("e, %llu, %llu\n", enqueue_ticks, totalTicks);
                    pthread_mutex_lock(&lock);
                    memcpy(enqueuetimestamp + numEnqueue, timestamp, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
                    numEnqueue += NUM_SAMPLES_PER_THREAD;
                    pthread_mutex_unlock(&lock);
#endif

#ifdef THROUGHPUT
                    clock_gettime(CLOCK_MONOTONIC, &tend);
                    pthread_mutex_lock(&lock);
                    double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
                    printf("elapsed time: %lf\n", elapsed);
                    printf("Num enqueue tasks run: %d\n", NUM_SAMPLES_PER_THREAD);
                    enqueuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
                    ENQUEUE_SAMPLES += NUM_SAMPLES_PER_THREAD;
                    pthread_mutex_unlock(&lock);
#endif

                    return 0;
                }

                void *enqueuespmc_handler(void * in) {
                    int my_cpu = (int) (long) in;

#ifdef HWLOC
                    hwloc_obj_t obj; /* Hwloc object    */
                    hwloc_cpuset_t cpuset; /* HwLoc cpuset    */
                    obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, my_cpu % NUM_CPUS);
                    cpuset = hwloc_cpuset_dup(obj->cpuset);
                    hwloc_cpuset_singlify(cpuset);

                    hwloc_set_cpubind(topology, cpuset, HWLOC_CPUBIND_THREAD);

                    char *str = NULL;
                    hwloc_cpuset_asprintf(&str, obj->cpuset);
                    printf("Thread bound to cpuset %s\n", str);
#else

                    cpu_set_t set;

                    CPU_ZERO(&set);
                    CPU_SET(my_cpu % NUM_CPUS, &set);

                    pthread_setaffinity_np(pthread_self(), sizeof (set), &set);
#ifdef ARM
                    enable_ccnt_read(NULL);
#endif
                    printf("Tried to pin to %d, Enqueue Thread on CPU %d\n", my_cpu, sched_getcpu());
#endif

#ifdef LATENCY
                    ticks start_tick, end_tick;
                    int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES;
                    ticks *timestamp;
                    timestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES_PER_THREAD);
                    for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                        timestamp[i] = (ticks) 0;
                    }
#endif

#ifdef THROUGHPUT
                    struct timespec tstart, tend, looptime, loopend;
                    int i = 1;
                    long int NUM_SAMPLES_PER_THREAD = 0;
                    double diff = 0.0;
#endif

                    pthread_barrier_wait(&barrier);

#ifdef THROUGHPUT
                    clock_gettime(CLOCK_MONOTONIC, &looptime);
                    clock_gettime(CLOCK_MONOTONIC, &tstart);
                    while (diff <= ENQUEUE_SECONDS) {
#endif
#ifdef LATENCY
                        for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                            start_tick = getticks();
#endif
                            EnqueueMultiple((atom) (i + 1), queues[i % NUM_QUEUES], (i % NUM_QUEUES));
#ifdef LATENCY
                            end_tick = getticks();
                            timestamp[i] = end_tick - start_tick;
#endif
#ifdef THROUGHPUT
                            i++;
                            if (i % 10000 == 0) {
                                clock_gettime(CLOCK_MONOTONIC, &loopend);
                                NUM_SAMPLES_PER_THREAD += i;
                                i = 1;
                                diff = (loopend.tv_sec - looptime.tv_sec);
                            }
#endif
                        }
#ifdef LATENCY
                        pthread_mutex_lock(&lock);
                        memcpy(enqueuetimestamp + numEnqueue, timestamp, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
                        numEnqueue += NUM_SAMPLES_PER_THREAD;
                        pthread_mutex_unlock(&lock);
#endif

#ifdef THROUGHPUT
                        clock_gettime(CLOCK_MONOTONIC, &tend);
                        pthread_mutex_lock(&lock);
                        double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
                        printf("elapsed time: %lf\n", elapsed);
                        printf("Num enqueue tasks run: %d\n", NUM_SAMPLES_PER_THREAD);
                        enqueuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
                        ENQUEUE_SAMPLES += NUM_SAMPLES_PER_THREAD;
                        pthread_mutex_unlock(&lock);
#endif

                        return 0;
                    }

                    void *workerspmc_handler(void * in) {
                        int my_cpu = (int) (long) in;
#ifdef HWLOC
                        hwloc_obj_t obj; /* Hwloc object    */
                        hwloc_cpuset_t cpuset; /* HwLoc cpuset    */
                        obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, my_cpu % NUM_CPUS);
                        cpuset = hwloc_cpuset_dup(obj->cpuset);
                        hwloc_cpuset_singlify(cpuset);

                        hwloc_set_cpubind(topology, cpuset, HWLOC_CPUBIND_THREAD);

                        char *str = NULL;
                        hwloc_cpuset_asprintf(&str, obj->cpuset);
                        printf("Thread bound to cpuset %s\n", str);
#else
                        cpu_set_t set;
                        CPU_ZERO(&set);
                        CPU_SET(my_cpu % NUM_CPUS, &set);
                        pthread_setaffinity_np(pthread_self(), sizeof (set), &set);
#ifdef ARM
                        enable_ccnt_read(NULL);
#endif
                        printf("Tried to pin to %d, Worker Thread on CPU %d\n", my_cpu, sched_getcpu());
#endif
#ifdef LATENCY
                        ticks start_tick, end_tick;
                        int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
                        ticks *timestamp;
                        timestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES_PER_THREAD);
                        for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                            timestamp[i] = (ticks) 0;
                        }
#endif

#ifdef THROUGHPUT
                        struct timespec looptime, loopend;
                        struct timespec tstart, tend;
#endif

                        pthread_barrier_wait(&barrier);
#ifdef VERBOSE
                        printf("Dequeue thread woke up\n");
#endif

#ifdef THROUGHPUT
                        long int NUM_SAMPLES_PER_THREAD = 0;
                        int count = 1;
                        int ret;
                        double diff = 0.0;
                        clock_gettime(CLOCK_MONOTONIC, &looptime);
                        clock_gettime(CLOCK_MONOTONIC, &tstart);
                        while (diff <= DEQUEUE_SECONDS && ret != -1) {
#endif
#ifdef LATENCY
                            for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                                start_tick = getticks();
#endif
#ifdef THROUGHPUT
                                ret = DequeueMultiple(queues[my_cpu], my_cpu);
#else
                                DequeueMultiple(queues[my_cpu], my_cpu);
#endif

#ifdef LATENCY
                                end_tick = getticks();
                                timestamp[i] = end_tick - start_tick;
#endif
#ifdef THROUGHPUT
                                count++;
                                if (count % 10000 == 0 || ret == -1) {
                                    clock_gettime(CLOCK_MONOTONIC, &loopend);
                                    NUM_SAMPLES_PER_THREAD += count;
                                    count = 1;
                                    diff = (loopend.tv_sec - looptime.tv_sec);
                                }
#endif
                            }
#ifdef LATENCY
                            pthread_mutex_lock(&lock);
                            memcpy(dequeuetimestamp + numDequeue, timestamp, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
                            numDequeue += NUM_SAMPLES_PER_THREAD;
                            pthread_mutex_unlock(&lock);
#endif

#ifdef THROUGHPUT
                            clock_gettime(CLOCK_MONOTONIC, &tend);
                            pthread_mutex_lock(&lock);
                            double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
                            printf("elapsed time: %lf\n", elapsed);
                            printf("Num dequeue tasks run: %d\n", NUM_SAMPLES_PER_THREAD);
                            dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
                            DEQUEUE_SAMPLES += NUM_SAMPLES_PER_THREAD;
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
                            //int success = 0;
#ifdef LATENCY
                            ticks start_tick, end_tick;
#endif

#ifdef THROUGHPUT
                            //struct timespec looptime, loopend;
                            struct timespec tstart, tend;
                            //double NUM_SAMPLES_PER_THREAD = 0.0;
#endif
                            int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
                            pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
                            NUM_SAMPLES_PER_THREAD = NUM_SAMPLES;
                            clock_gettime(CLOCK_MONOTONIC, &tstart);
                            //					int count = 1;
                            //					int counttry = 0;
                            //					double diff = 0.0;
                            //					clock_gettime(CLOCK_MONOTONIC, &looptime);
                            //					while(diff <= DEQUEUE_SECONDS)
                            //					{
#endif
                            for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
#ifdef LATENCY
                                start_tick = getticks();
#endif
                                bool ret = ck_ring_dequeue_mpmc(ring, buf, &entry);
#ifdef LATENCY
                                end_tick = getticks();
#endif

#ifdef LATENCY
                                pthread_mutex_lock(&lock);
                                if (!ret) {
                                    __sync_fetch_and_add(&failed_ck_dequeues, 1);
                                } else {
                                    dequeuetimestamp[numDequeue] = (end_tick - start_tick);
                                    __sync_fetch_and_add(&numDequeue, 1);
                                }
                                pthread_mutex_unlock(&lock);
#endif
                                //#ifdef THROUGHPUT
                                //						if(count % 100000 == 0)
                                //						{
                                //							clock_gettime(CLOCK_MONOTONIC, &loopend);
                                //							NUM_SAMPLES_PER_THREAD += count;
                                //							count = 1;
                                //							diff = ( loopend.tv_sec - looptime.tv_sec );
                                //						}
                                //#endif
                            }
#ifdef LATENCY
                            printf("global failed dequeues: %d\n", failed_ck_dequeues);
                            printf("Num dequeue samples per thread: %d\n", NUM_SAMPLES_PER_THREAD);
#endif

#ifdef THROUGHPUT
                            clock_gettime(CLOCK_MONOTONIC, &tend);
                            pthread_mutex_lock(&lock);
                            double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
                            printf("elapsed time: %lf\n", elapsed);
                            printf("Num tasks run: %d\n", NUM_SAMPLES_PER_THREAD);
                            dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
                            DEQUEUE_SAMPLES += NUM_SAMPLES_PER_THREAD;
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
                            //struct timespec looptime, loopend;
                            struct timespec tstart, tend;
#endif

                            int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;

                            struct entry entry = {0, 0};
                            pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
                            //					double NUM_SAMPLES_PER_THREAD = 0.0;
                            //
                            //					int count = 1;
                            //					double diff = 0.0;
                            //					clock_gettime(CLOCK_MONOTONIC, &looptime);
                            NUM_SAMPLES_PER_THREAD = NUM_SAMPLES;
                            clock_gettime(CLOCK_MONOTONIC, &tstart);
                            //					while(diff <= ENQUEUE_SECONDS)
                            //					{
#endif
                            for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
#ifdef LATENCY
                                start_tick = getticks();
#endif
                                ck_ring_enqueue_mpmc(ring, buf, &entry);
#ifdef LATENCY
                                end_tick = getticks();
                                pthread_mutex_lock(&lock);
                                enqueuetimestamp[numEnqueue] = (end_tick - start_tick);
                                numEnqueue++;
                                pthread_mutex_unlock(&lock);
#endif
                                //#ifdef THROUGHPUT
                                //						if(count % 100000 == 0)
                                //						{
                                //							clock_gettime(CLOCK_MONOTONIC, &loopend);
                                //							NUM_SAMPLES_PER_THREAD += count;
                                //							count = 1;
                                //							diff = ( loopend.tv_sec - looptime.tv_sec );
                                //						}
                                //#endif
                            }

#ifdef THROUGHPUT
                            clock_gettime(CLOCK_MONOTONIC, &tend);
                            pthread_mutex_lock(&lock);
                            double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
                            printf("elapsed time: %lf\n", elapsed);
                            printf("Num tasks run: %d\n", NUM_SAMPLES_PER_THREAD);
                            enqueuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
                            ENQUEUE_SAMPLES += NUM_SAMPLES_PER_THREAD;
                            pthread_mutex_unlock(&lock);
#endif
                            return 0;
                        }

                        void *bmm_worker_handler(void * in) {

                            struct lfds711_queue_bmm_state *qbmms = (struct lfds711_queue_bmm_state *) in;
#ifdef LATENCY
                            ticks start_tick, end_tick;
                            int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
                            ticks *timestamp;
                            timestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES_PER_THREAD);
                            for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                                timestamp[i] = (ticks) 0;
                            }
#endif

#ifdef THROUGHPUT
                            struct timespec looptime, loopend;
                            struct timespec tstart, tend;
#endif

                            pthread_barrier_wait(&barrier);

#ifdef THROUGHPUT
                            long int NUM_SAMPLES_PER_THREAD = 0;
                            int count = 1;
                            double diff = 0.0;
                            clock_gettime(CLOCK_MONOTONIC, &looptime);
                            clock_gettime(CLOCK_MONOTONIC, &tstart);
                            int isempty = 0;
                            while (diff <= DEQUEUE_SECONDS && !isempty) {
#endif
#ifdef LATENCY
                                for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                                    start_tick = getticks();
#endif
                                    int ret;
                                    ret = -1;
                                    while (ret == -1) {
#ifdef THROUGHPUT
                                        //printf("%d %d\n", qbmms->write_index, qbmms->read_index);
                                        isempty = (qbmms->write_index > NUM_SAMPLES) && (qbmms->read_index == qbmms->write_index);
                                        if (isempty) //queue is empty
                                            break;
#endif
                                        lfds711_queue_bmm_dequeue(qbmms, NULL, (void**)&ret);
                                    }
                                    //printf("dequeued %d\n", ret);

#ifdef LATENCY
                                    end_tick = getticks();
                                    timestamp[i] = end_tick - start_tick;
                                    //							pthread_mutex_lock(&lock);
                                    //							dequeuetimestamp[numDequeue] = (end_tick-start_tick);
                                    //							__sync_fetch_and_add(&numDequeue, 1);
                                    //							pthread_mutex_unlock(&lock);
#endif
#ifdef THROUGHPUT
                                    count++;
                                    if (count % 10000 == 0 || isempty) {
                                        clock_gettime(CLOCK_MONOTONIC, &loopend);
                                        NUM_SAMPLES_PER_THREAD += count;
                                        count = 1;
                                        diff = (loopend.tv_sec - looptime.tv_sec);
                                    }
#endif
                                }
#ifdef LATENCY
                                //		ticks totalTicks = 0;
                                //		for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
                                //			{
                                //				totalTicks += timestamp[i];
                                //			}
                                //
                                //			__sync_add_and_fetch(&dequeue_ticks,totalTicks);
                                //printf("d, %llu, %llu\n", dequeue_ticks, totalTicks);
                                pthread_mutex_lock(&lock);
                                memcpy(dequeuetimestamp + numDequeue, timestamp, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
                                numDequeue += NUM_SAMPLES_PER_THREAD;
                                pthread_mutex_unlock(&lock);
#endif

#ifdef THROUGHPUT
                                clock_gettime(CLOCK_MONOTONIC, &tend);
                                pthread_mutex_lock(&lock);
                                double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
                                //printf("elapsed time: %lf\n", elapsed);
                                //printf("Num dequeue tasks run: %d\n", NUM_SAMPLES_PER_THREAD);
                                dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
                                DEQUEUE_SAMPLES += NUM_SAMPLES_PER_THREAD;
                                pthread_mutex_unlock(&lock);
#endif

                                return 0;

                            }

                            void *bmm_enqueue_handler(void * in) {
                                struct lfds711_queue_bmm_state *qbmms = (struct lfds711_queue_bmm_state *) in;

#ifdef LATENCY
                                ticks start_tick, end_tick;
                                int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
                                ticks *timestamp;
                                timestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES_PER_THREAD);
                                for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                                    timestamp[i] = (ticks) 0;
                                }
#endif

#ifdef THROUGHPUT
                                struct timespec tstart, tend, looptime, loopend;
                                int i = 1;
                                long int NUM_SAMPLES_PER_THREAD = 0;
                                double diff = 0.0;
#endif

                                pthread_barrier_wait(&barrier);

#ifdef THROUGHPUT
                                clock_gettime(CLOCK_MONOTONIC, &looptime);
                                clock_gettime(CLOCK_MONOTONIC, &tstart);
                                while (diff <= ENQUEUE_SECONDS) {
#endif
#ifdef LATENCY
                                    for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                                        start_tick = getticks();
#endif
                                        lfds711_queue_bmm_enqueue(qbmms, NULL, &i);
                                        //printf("enqueued %d\n", i);
#ifdef LATENCY
                                        end_tick = getticks();
                                        /*pthread_mutex_lock(&lock);
                                        enqueuetimestamp[numEnqueue] = end_tick - start_tick;
                                        __sync_fetch_and_add(&numEnqueue, 1);
                                        pthread_mutex_unlock(&lock);*/
                                        timestamp[i] = end_tick - start_tick;
#endif
#ifdef THROUGHPUT
                                        i++;
                                        if (i % 10000 == 0) {
                                            clock_gettime(CLOCK_MONOTONIC, &loopend);
                                            NUM_SAMPLES_PER_THREAD += i;
                                            i = 1;
                                            diff = (loopend.tv_sec - looptime.tv_sec);
                                        }
#endif
                                    }
#ifdef LATENCY
                                    //			ticks totalTicks = 0;
                                    //		for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
                                    //			{
                                    //				totalTicks += timestamp[i];
                                    //			}
                                    //
                                    //			__sync_add_and_fetch(&enqueue_ticks,totalTicks);
                                    //printf("e, %llu, %llu\n", enqueue_ticks, totalTicks);
                                    pthread_mutex_lock(&lock);
                                    memcpy(enqueuetimestamp + numEnqueue, timestamp, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
                                    numEnqueue += NUM_SAMPLES_PER_THREAD;
                                    pthread_mutex_unlock(&lock);
#endif

#ifdef THROUGHPUT
                                    clock_gettime(CLOCK_MONOTONIC, &tend);
                                    pthread_mutex_lock(&lock);
                                    double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
                                    //printf("elapsed time: %lf\n", elapsed);
                                    //printf("Num enqueue tasks run: %d\n", NUM_SAMPLES_PER_THREAD);
                                    enqueuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
                                    ENQUEUE_SAMPLES += NUM_SAMPLES_PER_THREAD;
                                    pthread_mutex_unlock(&lock);
#endif

                                    return 0;
                                }
#endif

                                void *basicenqueue_handler(void *_queue) {
#ifdef LATENCY
                                    ticks start_tick, end_tick;

#endif
                                    int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
                                    ticks *timestamp;
                                    timestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES_PER_THREAD);
                                    for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                                        timestamp[i] = (ticks) 0;
                                    }

#ifdef THROUGHPUT
                                    struct timespec tstart, tend;
                                    //double NUM_SAMPLES_PER_THREAD = 0.0;
                                    //struct timespec looptime, loopend;
#endif

                                    pthread_barrier_wait(&barrier);
#ifdef VERBOSE
                                    printf("Enqueue thread woke up\n");
#endif
#ifdef THROUGHPUT
                                    NUM_SAMPLES_PER_THREAD = NUM_SAMPLES;
                                    clock_gettime(CLOCK_MONOTONIC, &tstart);
                                    //					int count = 1;
                                    //					double diff = 0.0;
                                    //					clock_gettime(CLOCK_MONOTONIC, &looptime);
                                    //					while(diff <= ENQUEUE_SECONDS)
                                    //					{
#endif
                                    for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
#ifdef LATENCY
                                        start_tick = getticks();
#endif

                                        BasicEnqueue((atom) i + 1);
#ifdef LATENCY
                                        end_tick = getticks();
                                        timestamp[i] = end_tick - start_tick;
                                        //						pthread_mutex_lock(&lock);
                                        //						enqueuetimestamp[numEnqueue] = (end_tick-start_tick);
                                        //						__sync_fetch_and_add(&numEnqueue, 1);
                                        //						pthread_mutex_unlock(&lock);
#endif
                                        //#ifdef THROUGHPUT
                                        //						count++;
                                        //						if(count % 100000 == 0)
                                        //						{
                                        //							clock_gettime(CLOCK_MONOTONIC, &loopend);
                                        //							NUM_SAMPLES_PER_THREAD += count;
                                        //							count = 1;
                                        //							diff = ( loopend.tv_sec - looptime.tv_sec );
                                        //						}
                                        //#endif
                                    }
#ifdef LATENCY
                                    //			ticks totalTicks = 0;
                                    //		for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
                                    //			{
                                    //				totalTicks += timestamp[i];
                                    //			}
                                    //
                                    //			__sync_add_and_fetch(&enqueue_ticks,totalTicks);
                                    //printf("e, %llu, %llu\n", enqueue_ticks, totalTicks);
                                    pthread_mutex_lock(&lock);
                                    memcpy(enqueuetimestamp + numEnqueue, timestamp, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
                                    numEnqueue += NUM_SAMPLES_PER_THREAD;
                                    pthread_mutex_unlock(&lock);
#endif

#ifdef THROUGHPUT
                                    clock_gettime(CLOCK_MONOTONIC, &tend);
                                    pthread_mutex_lock(&lock);
                                    double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
                                    printf("elapsed time: %lf\n", elapsed);
                                    printf("Num tasks run: %d\n", NUM_SAMPLES_PER_THREAD);
                                    enqueuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
                                    ENQUEUE_SAMPLES += NUM_SAMPLES_PER_THREAD;
                                    pthread_mutex_unlock(&lock);
#endif

                                    return 0;
                                }

                                void *basicworker_handler(void *_queue) {
#ifdef LATENCY
                                    ticks start_tick, end_tick;

#endif
                                    int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
                                    ticks *timestamp;
                                    timestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES_PER_THREAD);
                                    for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                                        timestamp[i] = (ticks) 0;
                                    }
#ifdef THROUGHPUT
                                    //struct timespec looptime, loopend;
                                    struct timespec tstart, tend;
                                    //double NUM_SAMPLES_PER_THREAD = 0.0;
#endif
                                    pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
                                    NUM_SAMPLES_PER_THREAD = NUM_SAMPLES;
                                    clock_gettime(CLOCK_MONOTONIC, &tstart);


                                    //					int count = 1;
                                    //
                                    //					double diff = 0.0;
                                    //					clock_gettime(CLOCK_MONOTONIC, &looptime);
                                    //					while(diff <= DEQUEUE_SECONDS)
                                    //					{
#endif

                                    for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
#ifdef LATENCY
                                        start_tick = getticks();
#endif
                                        BasicDequeue();

#ifdef LATENCY
                                        end_tick = getticks();
                                        timestamp[i] = end_tick - start_tick;
                                        //						pthread_mutex_lock(&lock);
                                        //						dequeuetimestamp[numDequeue] = (end_tick-start_tick);
                                        //						__sync_fetch_and_add(&numDequeue, 1);
                                        //						pthread_mutex_unlock(&lock);
#endif
                                        //#ifdef THROUGHPUT
                                        //						count++;
                                        //						if(count % 100000 == 0)
                                        //						{
                                        //							clock_gettime(CLOCK_MONOTONIC, &loopend);
                                        //							NUM_SAMPLES_PER_THREAD += count;
                                        //							count = 1;
                                        //							diff = ( loopend.tv_sec - looptime.tv_sec );
                                        //						}
                                        //#endif
                                    }
#ifdef LATENCY
                                    //		ticks totalTicks = 0;
                                    //		for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
                                    //			{
                                    //				totalTicks += timestamp[i];
                                    //			}
                                    //
                                    //			__sync_add_and_fetch(&dequeue_ticks,totalTicks);
                                    //printf("d, %llu, %llu\n", dequeue_ticks, totalTicks);
                                    pthread_mutex_lock(&lock);
                                    memcpy(dequeuetimestamp + numDequeue, timestamp, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
                                    numDequeue += NUM_SAMPLES_PER_THREAD;
                                    pthread_mutex_unlock(&lock);
#endif

#ifdef THROUGHPUT
                                    clock_gettime(CLOCK_MONOTONIC, &tend);
                                    pthread_mutex_lock(&lock);
                                    double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
                                    printf("elapsed time: %lf\n", elapsed);
                                    printf("Num tasks run: %d\n", NUM_SAMPLES_PER_THREAD);
                                    dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
                                    DEQUEUE_SAMPLES += NUM_SAMPLES_PER_THREAD;
                                    pthread_mutex_unlock(&lock);
#endif

                                    return 0;
                                }
#ifndef PHI

                                void *rculfenqueue_handler() {
#ifdef LATENCY
                                    ticks start_tick, end_tick;

#endif
                                    int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
                                    ticks *timestamp;
                                    timestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES_PER_THREAD);
                                    for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                                        timestamp[i] = (ticks) 0;
                                    }
#ifdef THROUGHPUT
                                    struct timespec tstart, tend;
                                    //struct timespec looptime, loopend;
                                    //double NUM_SAMPLES_PER_THREAD = 0.0;
#endif
                                    pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
                                    NUM_SAMPLES_PER_THREAD = NUM_SAMPLES;
                                    clock_gettime(CLOCK_MONOTONIC, &tstart);
                                    //					int count = 1;
                                    //					double diff = 0.0;
                                    //					clock_gettime(CLOCK_MONOTONIC, &looptime);
                                    //					while(diff <= ENQUEUE_SECONDS)
                                    //					{
#endif

                                    struct mynode *node;

                                    rcu_register_thread();

                                    for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
#ifdef LATENCY
                                        start_tick = getticks();

#endif

                                        node = malloc(sizeof (struct mynode));

                                        cds_lfq_node_init_rcu(&node->node);
                                        node->value = i + 1;
                                        /*
                                         * Both enqueue and dequeue need to be called within RCU
                                         * read-side critical section.
                                         */
                                        rcu_read_lock();
                                        cds_lfq_enqueue_rcu(&myqueue, &node->node);
                                        rcu_read_unlock();
#ifdef LATENCY
                                        end_tick = getticks();
                                        timestamp[i] = end_tick - start_tick;
                                        /*pthread_mutex_lock(&lock);
                        enqueuetimestamp[numEnqueue] = (end_tick-start_tick);
                        __sync_fetch_and_add(&numEnqueue, 1);
                        pthread_mutex_unlock(&lock);*/
#endif
                                        //#ifdef THROUGHPUT
                                        //						count++;
                                        //						if(count % 100000 == 0)
                                        //						{
                                        //							clock_gettime(CLOCK_MONOTONIC, &loopend);
                                        //							NUM_SAMPLES_PER_THREAD += count;
                                        //							count = 1;
                                        //							diff = ( loopend.tv_sec - looptime.tv_sec );
                                        //						}
                                        //#endif
                                    }
#ifdef LATENCY
                                    //			ticks totalTicks = 0;
                                    //		for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
                                    //			{
                                    //				totalTicks += timestamp[i];
                                    //			}
                                    //
                                    //			__sync_add_and_fetch(&enqueue_ticks,totalTicks);
                                    //printf("e, %llu, %llu\n", enqueue_ticks, totalTicks);
                                    pthread_mutex_lock(&lock);
                                    memcpy(enqueuetimestamp + numEnqueue, timestamp, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
                                    numEnqueue += NUM_SAMPLES_PER_THREAD;
                                    pthread_mutex_unlock(&lock);
#endif

#ifdef THROUGHPUT
                                    clock_gettime(CLOCK_MONOTONIC, &tend);
                                    pthread_mutex_lock(&lock);
                                    double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
                                    printf("elapsed time: %lf\n", elapsed);
                                    printf("Num tasks run: %d\n", NUM_SAMPLES_PER_THREAD);
                                    enqueuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
                                    ENQUEUE_SAMPLES += NUM_SAMPLES_PER_THREAD;
                                    pthread_mutex_unlock(&lock);
#endif
                                    rcu_unregister_thread();

                                    return 0;
                                }

                                void* rculfdequeue_handler() {
#ifdef LATENCY
                                    ticks start_tick, end_tick;
#endif
                                    int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / CUR_NUM_THREADS;
                                    ticks *timestamp;
                                    timestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES_PER_THREAD);
                                    for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {
                                        timestamp[i] = (ticks) 0;
                                    }

#ifdef THROUGHPUT

                                    //struct timespec looptime, loopend;
                                    struct timespec tstart, tend;
                                    //double NUM_SAMPLES_PER_THREAD = 0.0;
#endif

                                    pthread_barrier_wait(&barrier);
#ifdef THROUGHPUT
                                    NUM_SAMPLES_PER_THREAD = NUM_SAMPLES;
                                    clock_gettime(CLOCK_MONOTONIC, &tstart);
                                    //					int count = 1;
                                    //					double diff = 0.0;
                                    //					clock_gettime(CLOCK_MONOTONIC, &looptime);
                                    //					while(diff <= DEQUEUE_SECONDS)
                                    //					{
#endif
                                    rcu_register_thread();

                                    for (int i = 0; i < NUM_SAMPLES_PER_THREAD; i++) {

                                        /*
                                         * Dequeue each node from the queue. Those will be dequeued from
                                         * the oldest (first enqueued) to the newest (last enqueued).
                                         */
                                        struct cds_lfq_node_rcu *qnode;
                                        struct mynode *n;

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
                                        /* Getting the container structure from the node */
                                        if (qnode) {
                                            n = caa_container_of(qnode, struct mynode, node);
#ifdef VERBOSE
                                            printf(" %d", node->value);
#endif
                                            call_rcu(&n->rcu_head, free_node);
                                        }
#ifdef LATENCY
                                        end_tick = getticks();
                                        timestamp[i] = end_tick - start_tick;
                                        //						pthread_mutex_lock(&lock);
                                        //						dequeuetimestamp[numDequeue] = (end_tick-start_tick);
                                        //						__sync_fetch_and_add(&numDequeue, 1);
                                        //						pthread_mutex_unlock(&lock);
#endif

                                        //#ifdef THROUGHPUT
                                        //						count++;
                                        //						if(count % 100000 == 0)
                                        //						{
                                        //							clock_gettime(CLOCK_MONOTONIC, &loopend);
                                        //							NUM_SAMPLES_PER_THREAD += count;
                                        //							count = 1;
                                        //							diff = ( loopend.tv_sec - looptime.tv_sec );
                                        //						}
                                        //#endif
                                    }
#ifdef LATENCY
                                    //					ticks totalTicks = 0;
                                    //					for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
                                    //						{
                                    //							totalTicks += timestamp[i];
                                    //						}
                                    //
                                    //						__sync_add_and_fetch(&dequeue_ticks,totalTicks);
                                    //printf("d, %llu, %llu\n", dequeue_ticks, totalTicks);
                                    pthread_mutex_lock(&lock);
                                    memcpy(dequeuetimestamp + numDequeue, timestamp, NUM_SAMPLES_PER_THREAD * sizeof (ticks));
                                    numDequeue += NUM_SAMPLES_PER_THREAD;
                                    pthread_mutex_unlock(&lock);
#endif

#ifdef THROUGHPUT
                                    clock_gettime(CLOCK_MONOTONIC, &tend);
                                    pthread_mutex_lock(&lock);
                                    double elapsed = (tend.tv_sec - tstart.tv_sec) + ((tend.tv_nsec - tstart.tv_nsec) / 1E9);
                                    printf("elapsed time: %lf\n", elapsed);
                                    printf("Num tasks run: %d\n", NUM_SAMPLES_PER_THREAD);
                                    dequeuethroughput += ((NUM_SAMPLES_PER_THREAD * 1.0) / elapsed);
                                    DEQUEUE_SAMPLES += NUM_SAMPLES_PER_THREAD;
                                    pthread_mutex_unlock(&lock);
#endif

                                    rcu_unregister_thread();
                                    return 0;
                                }
#endif

                                int cmpfunc(const void * a, const void * b) {
                                    return ( *(int*) a - *(int*) b);
                                }

                                void* simplesleephandler() {
                                    sleep(0);
                                    return 0;
                                }

                                void SortTicks(ticks* numTicks, int total, int faileddeq) {
                                    qsort(numTicks, total, sizeof (*numTicks), cmpfunc);
                                }

                                void ResetCounters() {
                                    numEnqueue = 0;
                                    numDequeue = 0;
                                    numEnqueueThreadsCreated = 0;
                                    numDequeueThreadsCreated = 0;
                                    dequeuethroughput = 0;
                                    enqueuethroughput = 0;
                                    DEQUEUE_SAMPLES = 0;
                                    ENQUEUE_SAMPLES = 0;
                                    dequeue_ticks = 0;
                                    enqueue_ticks = 0;
#ifndef PHI
                                    failed_ck_dequeues = 0;
#endif
                                }

                                void ComputeSummary(int type, int numThreads, FILE* afp, FILE* rfp, int rdtsc_overhead) {
                                    printf("Printing summary\n");
#ifdef LATENCY
                                    ticks totalEnqueueTicks = 0, totalDequeueTicks = 0;
                                    ticks enqueuetickMin = enqueuetimestamp[0] - rdtsc_overhead;
                                    ticks enqueuetickMax = enqueuetimestamp[0] - rdtsc_overhead;
                                    ticks dequeuetickMin = dequeuetimestamp[0] - rdtsc_overhead;
                                    ticks dequeuetickMax = dequeuetimestamp[0] - rdtsc_overhead;
                                    ticks *numEnqueueTicks, *numDequeueTicks;
                                    numEnqueueTicks = (ticks *) malloc(sizeof (ticks) * numEnqueue);
                                    numDequeueTicks = (ticks *) malloc(sizeof (ticks) * numDequeue);

                                    //compute the elapsed time per invocation, and find min and max
                                    for (int i = 0; i < numEnqueue; i++) {
                                        //compute the elapsed time per invocation, and subtract the cost of the emtpy loop cost per iteration
                                        numEnqueueTicks[i] = enqueuetimestamp[i] - rdtsc_overhead;
                                        totalEnqueueTicks += numEnqueueTicks[i];
                                    }

                                    for (int i = 0; i < (numDequeue); i++) {
                                        numDequeueTicks[i] = dequeuetimestamp[i] - rdtsc_overhead;
                                        totalDequeueTicks += numDequeueTicks[i];
                                    }

                                    SortTicks(numEnqueueTicks, numEnqueue, 0);
                                    SortTicks(numDequeueTicks, numDequeue, 0);
#ifdef RAW
                                    int flag = 0, i = 0;
                                    while (i < numEnqueue && i < numDequeue) {
                                        double dequeueTime = 0.0;
                                        double enqueueTime = (numEnqueueTicks[i] / clockFreq);
                                        if (i < (numDequeue))
                                            dequeueTime = (numDequeueTicks[i] / clockFreq);

                                        fprintf(rfp, "%d %d %d %ld %ld %d %lf %lf\n", type, numEnqueue, numDequeue, (numEnqueueTicks[i]), (numDequeueTicks[i]), CUR_NUM_THREADS, enqueueTime, dequeueTime);
#ifdef VERBOSE
                                        printf("%d %d %d %ld %ld %d %lf %lf\n", type, numEnqueue, numDequeue, (numEnqueueTicks[i]), (numDequeueTicks[i]), CUR_NUM_THREADS, enqueueTime, dequeueTime);
#endif
                                        i++;
                                    }
#endif

                                    enqueuetickMin = numEnqueueTicks[0];
                                    enqueuetickMax = numEnqueueTicks[numEnqueue - 1];

                                    dequeuetickMin = numDequeueTicks[0];
                                    dequeuetickMax = numDequeueTicks[numDequeue - 1];

                                    //compute average
                                    double tickEnqueueAverage = (totalEnqueueTicks / (numEnqueue));
                                    double tickDequeueAverage = (totalDequeueTicks / (numDequeue));

//                                    printf("Num threads: %d, Num enqueue samples: %d, Num dequeue samples: %d\n", numThreads, numEnqueue, numDequeue);
//                                    printf("Enqueue Min: %ld\n", enqueuetickMin);
//                                    printf("Dequeue Min: %ld\n", dequeuetickMin);
//
//                                    printf("Enqueue Max: %ld\n", enqueuetickMax);
//                                    printf("Dequeue Max: %ld\n", dequeuetickMax);
//
//                                    printf("Average Enqueue : %lf\n", tickEnqueueAverage);
//                                    printf("Average Dequeue : %lf\n", tickDequeueAverage);

                                    ticks enqueuetickmedian = 0, dequeuetickmedian = 0;

                                    if (numEnqueue % 2 == 0) {
                                        // if there is an even number of elements, return mean of the two elements in the middle
                                        enqueuetickmedian = ((numEnqueueTicks[(numEnqueue / 2)] + numEnqueueTicks[(numEnqueue / 2) - 1]) / 2.0);
                                    } else {
                                        // else return the element in the middle
                                        enqueuetickmedian = numEnqueueTicks[(numEnqueue / 2)];
                                    }

                                    if (numDequeue % 2 == 0) {
                                        // if there is an even number of elements, return mean of the two elements in the middle

                                        dequeuetickmedian = ((numDequeueTicks[((numDequeue) / 2)] + numDequeueTicks[((numDequeue) / 2) - 1]) / 2.0);
                                    } else {
                                        dequeuetickmedian = numDequeueTicks[((numDequeue) / 2)];
                                    }

//                                    printf("Median Enqueue : %ld\n", enqueuetickmedian);
//                                    printf("Median Dequeue : %ld\n", dequeuetickmedian);

                                    double enqueueMinTime = ((enqueuetickMin) / clockFreq);
                                    double dequeueMinTime = ((dequeuetickMin) / clockFreq);
                                    double enqueueMaxTime = ((enqueuetickMax) / clockFreq);
                                    double dequeueMaxTime = ((dequeuetickMax) / clockFreq);
                                    double enqueueAvgTime = ((tickEnqueueAverage) / clockFreq);
                                    double dequeueAvgTime = ((tickDequeueAverage) / clockFreq);

//                                    printf("Enqueue Min Time (ns): %lf\n", enqueueMinTime);
//                                    printf("Dequeue Min Time (ns): %lf\n", dequeueMinTime);
//
//                                    printf("Enqueue Max Time (ns): %lf\n", enqueueMaxTime);
//                                    printf("Dequeue Max Time (ns): %lf\n", dequeueMaxTime);
//
//                                    printf("Average Enqueue Time (ns): %lf\n", enqueueAvgTime);
//                                    printf("Average Dequeue Time (ns): %lf\n", dequeueAvgTime);
//
//                                    printf("%s,%d,%llu,%llu,%d\n", type, NUM_SAMPLES, enqueue_ticks / NUM_SAMPLES, dequeue_ticks / NUM_SAMPLES, CUR_NUM_THREADS);
                                    printf("%d %d %d %d %ld %ld %ld %ld %lf %lf %ld %ld\n", type, numThreads, numEnqueue, numDequeue, enqueuetickMin, dequeuetickMin, enqueuetickMax, dequeuetickMax, tickEnqueueAverage, tickDequeueAverage, enqueuetickmedian, dequeuetickmedian);
                                    fprintf(afp, "%d %d %d %d %ld %ld %ld %ld %lf %lf %ld %ld %lf %lf %lf %lf %lf %lf\n", type, numThreads, numEnqueue, numDequeue, enqueuetickMin, dequeuetickMin, enqueuetickMax, dequeuetickMax, tickEnqueueAverage, tickDequeueAverage, enqueuetickmedian, dequeuetickmedian, enqueueMinTime, dequeueMinTime, enqueueMaxTime, dequeueMaxTime, enqueueAvgTime, dequeueAvgTime);
#endif
#ifdef THROUGHPUT
                                    printf("NumEnqueueSamples:%d NumDequeueSamples:%d NumThreads:%d EnqueueThroughput:%f DequeueThroughput:%f\n", ENQUEUE_SAMPLES, DEQUEUE_SAMPLES, numThreads, enqueuethroughput, dequeuethroughput);
#ifdef TITLE
                                    fprintf(afp, "EnqueueSamples DequeueSamples NumThreads EnqueueThroughput DequeueThroughput\n");
#endif
                                    fprintf(afp, "%d %d %d %f %f\n", ENQUEUE_SAMPLES, DEQUEUE_SAMPLES, numThreads, enqueuethroughput, dequeuethroughput);
#endif

#ifdef LATENCY
                                    free(numEnqueueTicks);
                                    free(numDequeueTicks);
#endif
                                }

                                unsigned int rand_interval(unsigned int min, unsigned int max) {
                                    int r;
                                    const unsigned int range = 1 + max - min;
                                    const unsigned int buckets = RAND_MAX / range;
                                    const unsigned int limit = buckets * range;

                                    /* Create equal size buckets all in a row, then fire randomly towards
                                     * the buckets until you land in one of them. All buckets are equally
                                     * likely. If you land off the end of the line of buckets, try again. */
                                    do {
                                        r = rand();
                                    } while (r >= limit);

                                    return min + (r / buckets);
                                }

                                int main(int argc, char **argv) {
                                    int threadCount = 0;
                                    int queueType;
                                    int *threads = malloc(sizeof (int*));
                                    char* fileName1, *fileName2;
                                    //Inputs are type of queue, thread list,
                                    if (argc != 5) {
                                        printf("Usage: <QueueType 1-SQueue, 2-CK, 3-Basic Queue, 4-Multiple Incoming Queues>, \nThreads-1,2,4,6,8,12,16,24,32,48,57,96,114,192,228,384,456,768,912,1024, \nRaw data file name: <name>,  \nSummary file name: <name>, \nClock Frequency in GHz: <3.4>\n");
                                        exit(-1);
                                    } else {
                                        char* arg = argv[1];
                                        queueType = atoi(arg);

                                        switch (queueType) {
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
                                            case 7:
                                                printf("Queue type: Simple thread creation with sleep(0) using OpenMP\n");
                                                break;
                                            case 8:
                                                printf("Multiple Incoming Queues - Single producer multiple consumer variation\n");
                                                break;
                                            default:
                                                printf("Usage: <QueueType 1-SQueue, 2-CK, 3-Basic Queue, 4-Multiple Incoming Queues, 5-RCU LF Queue>, \nThreads-1,2,4,6,8,12,16,24,32,48,57,96,114,192,228,384,456,768,912,1024, \nRaw data file name: <name>,  \nSummary file name: <name>\n");
                                                exit(-1);
                                                break;
                                        }

                                        char* str = argv[2];
                                        char *thread;
                                        thread = strtok(str, ",");
                                        printf("Thread list: ");
                                        while (thread != NULL) {
                                            threads[threadCount] = atoi(thread);
                                            threadCount++;
                                            printf("%s ", thread);
                                            thread = strtok(NULL, ",");
                                        }

                                        printf("\n");

                                        fileName1 = argv[3];
                                        fileName2 = argv[4];

                                        printf("Num of samples: %d\n", NUM_SAMPLES);
                                        printf("Thread list count: %d\n", threadCount);
                                        printf("Output files: %s, %s\n", fileName1, fileName2);
                                    }
                                    int rdtsc_overhead_ticks = 0;

                                    //Start counter for ARM first.                            
#ifdef ARM
                                    enable_ccnt_read(NULL);
#endif

                                    //Open file for storing data

                                    FILE *rfp = fopen(fileName1, "a");
                                    FILE *afp = fopen(fileName2, "a");

                                    struct timezone tz;
                                    struct timeval tvstart, tvstop;
                                    unsigned long long int cycles[2];
                                    unsigned long microseconds;

                                    memset(&tz, 0, sizeof (tz));

                                    gettimeofday(&tvstart, &tz);
                                    cycles[0] = getticks();
                                    gettimeofday(&tvstart, &tz);

                                    usleep(250000);

                                    gettimeofday(&tvstop, &tz);
                                    cycles[1] = getticks();
                                    gettimeofday(&tvstop, &tz);

                                    microseconds = ((tvstop.tv_sec - tvstart.tv_sec)*1000000) + (tvstop.tv_usec - tvstart.tv_usec);

                                    clockFreq = ((cycles[1] - cycles[0])*1.0) / (microseconds * 1000);

                                    printf("Clock Freq Obtained: %f\n", clockFreq);

#ifdef CALIBRATE
                                    //Calibrate RDTSC
                                    ticks start_tick = (ticks) 0;
                                    ticks end_tick = (ticks) 0;
                                    ticks totalTicks = (ticks) 0;
                                    ticks diff_tick = (ticks) 0;

                                    ticks minRdtscTicks = 0;
                                    for (int i = 0; i < NUM_SAMPLES; i++) {
                                        start_tick = getticks();
                                        end_tick = getticks();

                                        diff_tick = (end_tick - start_tick);
                                        totalTicks += diff_tick;
                                        if (i == 0)
                                            minRdtscTicks = diff_tick;
                                        else {
                                            if (minRdtscTicks > diff_tick)
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

#ifdef RAW
#ifdef TITLE
                                    fprintf(rfp, "QueueType EnqueueSamples DequeueSamples EnqueueCycles DequeueCycles NumThreads EnqueueTime(ns) DequeueTime(ns)\n");
#endif
#endif
#ifdef VERBOSE
                                    printf("QueueType EnqueueSamples DequeueSamples EnqueueCycles DequeueCycles NumThreads EnqueueTime(ns) DequeueTime(ns)\n");
#endif
#ifdef LATENCY
#ifdef TITLE
                                    fprintf(afp, "QueueType NumThreads EnqueueSamples DequeueSamples EnqueueMin DequeueMin EnqueueMax DequeueMax EnqueueAverage DequeueAverage EnqueueMedian DequeueMedian EnqueueMinTime DequeueMinTime EnqueueMaxTime DequeueMaxTime EnqueueAverageTime DequeueAverageTime\n");
#endif
#endif
#ifdef HWLOC
                                    /* Allocate and initialize topology object.  */
                                    hwloc_topology_init(&topology);

                                    /* Perform the topology detection.  */
                                    hwloc_topology_load(topology);

                                    /* Get the number of cores (We don't want to use HyperThreading */
                                    int numCores = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
                                    printf("Number of cores: %d\n", numCores);
                                    hwloc_obj_t obj; /* Hwloc object    */
                                    hwloc_cpuset_t cpuset; /* HwLoc cpuset    */
                                    obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, 0);
                                    cpuset = hwloc_cpuset_dup(obj->cpuset);
                                    hwloc_cpuset_singlify(cpuset);

                                    hwloc_set_cpubind(topology, cpuset, HWLOC_CPUBIND_THREAD);

                                    char *str = NULL;
                                    hwloc_cpuset_asprintf(&str, obj->cpuset);
                                    printf("Main thread bound to cpuset %s\n", str);
#else
                                    //printf("Main Thread on CPU %d\n", sched_getcpu());


                                    cpu_set_t set;
                                    CPU_ZERO(&set);
                                    CPU_SET(0, &set);

                                    pthread_setaffinity_np(pthread_self(), sizeof (set), &set);

#endif
                                    //Execute benchmarks for various types of queues
                                    switch (queueType) {
                                        case 1: //SQueue
                                            for (int k = 0; k < threadCount; k++) {
                                                InitQueue();
                                                ResetCounters();
                                                enqueuetimestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES);
                                                dequeuetimestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES);

                                                for (int i = 0; i < NUM_SAMPLES; i++) {
                                                    enqueuetimestamp[i] = (ticks) 0;
                                                    dequeuetimestamp[i] = (ticks) 0;
                                                }
                                                CUR_NUM_THREADS = (threads[k]) / 2;

                                                pthread_t *worker_threads;
                                                pthread_t *enqueue_threads;

                                                worker_threads = (pthread_t *) malloc(sizeof (pthread_t) * CUR_NUM_THREADS);
                                                enqueue_threads = (pthread_t *) malloc(sizeof (pthread_t) * CUR_NUM_THREADS);

                                                //Set number of threads that will call the barrier_wait to total of enqueue and dequeue threads
                                                pthread_barrier_init(&barrier, NULL, threads[k]);

                                                for (int i = 0; i < CUR_NUM_THREADS; i++) {
                                                    pthread_create(&enqueue_threads[i], NULL, enqueue_handler, (void*) (unsigned long) (i));
                                                    pthread_create(&worker_threads[i], NULL, worker_handler, (void*) (unsigned long) (i + CUR_NUM_THREADS));
                                                }

                                                for (int i = 0; i < CUR_NUM_THREADS; i++) {
                                                    pthread_join(enqueue_threads[i], NULL);
                                                    pthread_join(worker_threads[i], NULL);
                                                }

                                                ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp, rdtsc_overhead_ticks);
                                                //printf("%s,%d,%llu,%llu,%d\n", "Squeue", NUM_SAMPLES, enqueue_ticks/NUM_SAMPLES, dequeue_ticks/NUM_SAMPLES, CUR_NUM_THREADS);
                                                //fprintf(afp, "%s,%d,%llu,%llu,%d\n", "Squeue", NUM_SAMPLES, enqueue_ticks/NUM_SAMPLES, dequeue_ticks/NUM_SAMPLES, CUR_NUM_THREADS);

                                                free(enqueuetimestamp);
                                                free(dequeuetimestamp);
                                            }
                                            break;
#ifndef PHI
                                            case 2: //Concurrency Kit or LIBLFDS
                                                    for (int k = 0; k < threadCount; k++)
                                                    {
                                                            ResetCounters();
                                                            //ck_ring_buffer_t *buf;
                                                            //ck_ring_t *ring;

                                                            //size = NUM_SAMPLES; //Hardcoded for benchmarking purposes
                                                            //
                                                            //buf = malloc(sizeof(ck_ring_buffer_t) * size);
                                                            //ring = malloc(sizeof(ck_ring_t) * size);
                                                            //
                                                            //(ring, size);
                                                            //
                                                            //struct arg_struct args;
                                                            //args.ring = ring;
                                                            //args.buf = buf;
                                                            struct lfds711_queue_bmm_element qbmme[NUM_SAMPLES]; // TRD : must be a positive integer power of 2 (2, 4, 8, 16, etc)
                                                            struct lfds711_queue_bmm_state qbmms;

                                                            lfds711_queue_bmm_init_valid_on_current_logical_core( &qbmms, qbmme, NUM_SAMPLES, NULL );

                                                            enqueuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
                                                            dequeuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);

                                                            for (int i=0;i<NUM_SAMPLES;i++)
                                                            {
                                                                    enqueuetimestamp[i] = (ticks)0;
                                                                    dequeuetimestamp[i] = (ticks)0;
                                                            }
                                                            CUR_NUM_THREADS = (threads[k])/2;

                                                            pthread_t *worker_threads;
                                                            pthread_t *enqueue_threads;

                                                            worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);
                                                            enqueue_threads = (pthread_t *) malloc(sizeof(pthread_t) * CUR_NUM_THREADS);

                                                            //Set number of threads that will call the barrier_wait to total of enqueue and dequeue threads
                                                            pthread_barrier_init(&barrier, NULL, threads[k]);

                                                            for (int i = 0; i < CUR_NUM_THREADS; i++)
                                                            {
                                                                    //								pthread_create(&enqueue_threads[i], NULL, ck_enqueue_handler,(void *) &args);
                                                                    //								pthread_create(&worker_threads[i], NULL, ck_worker_handler,(void *) &args);
                                                                    pthread_create(&enqueue_threads[i], NULL, bmm_enqueue_handler,(void*) &qbmms);
                                                                    pthread_create(&worker_threads[i], NULL, bmm_worker_handler,(void*) &qbmms);
                                                            }

                                                            for (int i = 0; i < CUR_NUM_THREADS; i++)
                                                            {
                                                                    pthread_join(enqueue_threads[i], NULL);
                                                                    pthread_join(worker_threads[i], NULL);
                                                            }

                                                            //printf("Failed Dequeues: %d\n", failed_ck_dequeues);
                                                            //fprintf(afp, "Failed Dequeues: %d\n", failed_ck_dequeues);
                                                            ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp, rdtsc_overhead_ticks);

                                                            free(enqueuetimestamp);
                                                            free(dequeuetimestamp);

                                                            lfds711_queue_bmm_cleanup( &qbmms, NULL );
                                                    }
                                                    break;
#endif
                                        case 3: //Basic linux queue
                                            for (int k = 0; k < threadCount; k++) {
                                                InitBasicQueue();
                                                ResetCounters();

                                                enqueuetimestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES);
                                                dequeuetimestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES);
                                                for (int i = 0; i < NUM_SAMPLES; i++) {
                                                    enqueuetimestamp[i] = (ticks) 0;
                                                    dequeuetimestamp[i] = (ticks) 0;
                                                }
                                                CUR_NUM_THREADS = (threads[k]) / 2;

                                                pthread_t *worker_threads;
                                                pthread_t *enqueue_threads;

                                                worker_threads = (pthread_t *) malloc(sizeof (pthread_t) * CUR_NUM_THREADS);
                                                enqueue_threads = (pthread_t *) malloc(sizeof (pthread_t) * CUR_NUM_THREADS);

                                                //Set number of threads that will call the barrier_wait to total of enqueue and dequeue threads
                                                pthread_barrier_init(&barrier, NULL, threads[k]);

                                                for (int i = 0; i < CUR_NUM_THREADS; i++) {
                                                    pthread_create(&enqueue_threads[i], NULL, basicenqueue_handler, NULL);
                                                    pthread_create(&worker_threads[i], NULL, basicworker_handler, NULL);
                                                }

                                                for (int i = 0; i < CUR_NUM_THREADS; i++) {
                                                    pthread_join(enqueue_threads[i], NULL);
                                                    pthread_join(worker_threads[i], NULL);
                                                }

                                                ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp, rdtsc_overhead_ticks);
                                                //printf("%s,%d,%llu,%llu,%d\n", "BQ", NUM_SAMPLES, enqueue_ticks/NUM_SAMPLES, dequeue_ticks/NUM_SAMPLES, CUR_NUM_THREADS);
                                                //fprintf(afp, "%s,%d,%llu,%llu,%d\n", "BQ", NUM_SAMPLES, enqueue_ticks/NUM_SAMPLES, dequeue_ticks/NUM_SAMPLES, CUR_NUM_THREADS);

                                                free(enqueuetimestamp);
                                                free(dequeuetimestamp);
                                            }
                                            break;
                                        case 4://Multiple Incoming Queues
                                            for (int k = 0; k < threadCount; k++) {
                                                CUR_NUM_THREADS = (threads[k]) / 2;
                                                NUM_QUEUES = CUR_NUM_THREADS;

                                                printf("Number of queues: %d\n", NUM_QUEUES);

                                                InitQueues(NUM_QUEUES);
                                                ResetCounters();
                                                enqueuetimestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES);
                                                dequeuetimestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES);
                                                for (int i = 0; i < NUM_SAMPLES; i++) {
                                                    enqueuetimestamp[i] = (ticks) 0;
                                                    dequeuetimestamp[i] = (ticks) 0;
                                                }

                                                pthread_t *worker_threads;
                                                pthread_t *enqueue_threads;

                                                worker_threads = (pthread_t *) malloc(sizeof (pthread_t) * CUR_NUM_THREADS);
                                                enqueue_threads = (pthread_t *) malloc(sizeof (pthread_t) * CUR_NUM_THREADS);
                                                //Set number of threads that will call the barrier_wait to total of enqueue and dequeue threads
                                                pthread_barrier_init(&barrier, NULL, threads[k]);

                                                for (int i = 0; i < CUR_NUM_THREADS; i++) {
                                                    pthread_create(&enqueue_threads[i], NULL, enqueuemultiple_handler, (void*) (unsigned long) (i));
                                                    pthread_create(&worker_threads[i], NULL, workermultiple_handler, (void*) (unsigned long) (i));
                                                }

                                                for (int i = 0; i < CUR_NUM_THREADS; i++) {
                                                    pthread_join(enqueue_threads[i], NULL);
                                                    pthread_join(worker_threads[i], NULL);
                                                }

                                                ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp, rdtsc_overhead_ticks);
                                                //printf("%s,%d,%llu,%llu,%d\n", "MSQ", NUM_SAMPLES, enqueue_ticks/NUM_SAMPLES, dequeue_ticks/NUM_SAMPLES, CUR_NUM_THREADS);
                                                //fprintf(afp, "%s,%d,%llu,%llu,%d\n", "MSQ", NUM_SAMPLES, enqueue_ticks/NUM_SAMPLES, dequeue_ticks/NUM_SAMPLES, CUR_NUM_THREADS);

                                                free(enqueuetimestamp);
                                                free(dequeuetimestamp);
                                            }
                                            break;
#ifndef PHI
                                        case 5: //RCU LF Queue

                                            for (int k = 0; k < threadCount; k++) {
                                                CUR_NUM_THREADS = (threads[k]) / 2;
                                                int ret = 0;

                                                ResetCounters();
                                                enqueuetimestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES);
                                                dequeuetimestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES);
                                                for (int i = 0; i < NUM_SAMPLES; i++) {
                                                    enqueuetimestamp[i] = (ticks) 0;
                                                    dequeuetimestamp[i] = (ticks) 0;
                                                }

                                                pthread_t *worker_threads;
                                                pthread_t *enqueue_threads;

                                                worker_threads = (pthread_t *) malloc(sizeof (pthread_t) * CUR_NUM_THREADS);
                                                enqueue_threads = (pthread_t *) malloc(sizeof (pthread_t) * CUR_NUM_THREADS);

                                                /*
                                                 * Each thread need using RCU read-side need to be explicitly
                                                 * registered.
                                                 */
                                                //rcu_register_thread();

                                                cds_lfq_init_rcu(&myqueue, call_rcu);

                                                //Set number of threads that will call the barrier_wait to total of enqueue and dequeue threads
                                                pthread_barrier_init(&barrier, NULL, threads[k]);

                                                for (int i = 0; i < CUR_NUM_THREADS; i++) {
                                                    pthread_create(&enqueue_threads[i], NULL, rculfenqueue_handler, NULL);
                                                    pthread_create(&worker_threads[i], NULL, rculfdequeue_handler, NULL);
                                                }

                                                for (int i = 0; i < CUR_NUM_THREADS; i++) {
                                                    pthread_join(enqueue_threads[i], NULL);
                                                    pthread_join(worker_threads[i], NULL);
                                                }

                                                ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp, rdtsc_overhead_ticks);
                                                //printf("%s,%d,%llu,%llu,%d\n", "RCU", NUM_SAMPLES, enqueue_ticks/NUM_SAMPLES, dequeue_ticks/NUM_SAMPLES, CUR_NUM_THREADS);
                                                //fprintf(afp, "%s,%d,%llu,%llu,%d\n", "RCU", NUM_SAMPLES, enqueue_ticks/NUM_SAMPLES, dequeue_ticks/NUM_SAMPLES, CUR_NUM_THREADS);

                                                free(enqueuetimestamp);
                                                free(dequeuetimestamp);

                                                /*
                                                 * Release memory used by the queue.
                                                 */
                                                ret = cds_lfq_destroy_rcu(&myqueue);
                                                if (ret) {
                                                    printf("Error destroying queue (non-empty)\n");
                                                }

                                                //rcu_unregister_thread();
                                                return ret;
                                            }
                                            break;
#endif
                                        case 6://SQueue with OpenMP threads
                                            for (int k = 0; k < threadCount; k++) {
                                                struct queue incoming;
                                                struct queue results;
                                                InitQueue();
                                                ResetCounters();

                                                CUR_NUM_THREADS = threads[k];
                                                omp_set_num_threads(CUR_NUM_THREADS);

                                                InitXTaskQueue(&incoming);
                                                InitXTaskQueue(&results);
#ifdef THROUGHPUT
                                                ticks st, et;
                                                st = getticks();
#endif
#pragma omp parallel for schedule(dynamic,1)
                                                for (int i = 0; i < NUM_SAMPLES; i++) {
                                                    EnqueueToQ((i + 1), &incoming);
                                                }
#ifdef THROUGHPUT
                                                et = getticks();
                                                ticks diff_tick = et - st;
                                                double elapsed = (diff_tick / clockFreq);
                                                //printf("Time elapsed for enqueue: %f\n", elapsed);
                                                enqueuethroughput = ((NUM_SAMPLES * 1000000000.0) / elapsed);
#endif

#ifdef THROUGHPUT
                                                ticks deq_st, deq_et;
                                                deq_st = getticks();
#endif
                                                //#pragma omp parallel for schedule(dynamic,1)
                                                //			for(int i=0; i<NUM_SAMPLES; i++)
                                                //			{
                                                //				DequeueFromQ(&incoming);
                                                //				sleep(0);
                                                //				EnqueueToQ((i+1), &results);
                                                //			}
                                                int count = 0;
#pragma omp parallel
#pragma omp single
                                                {
                                                    while (!isQueueEmpty(&incoming)) {
#ifdef VERBOSE
                                                        printf("Producer Thread ID:%d\n", omp_get_thread_num());
#endif
                                                        DequeueFromQ(&incoming);
#pragma omp task
                                                        {
#ifdef VERBOSE
                                                            printf("Thread ID:%d\n", omp_get_thread_num());
#endif
                                                            sleep(0); //asm volatile nop - for nothing
                                                            EnqueueToQ((count + 1), &results);
                                                        }

                                                    }
#pragma omp taskwait
                                                }
#ifdef THROUGHPUT
                                                deq_et = getticks();
                                                ticks deq_diff_tick = deq_et - deq_st;
                                                double deq_elapsed = (deq_diff_tick / clockFreq);
                                                //printf("Time elapsed for dequeue: %f\n", deq_elapsed);
                                                dequeuethroughput = ((NUM_SAMPLES * 1000000000.0) / deq_elapsed);
#endif
#ifdef THROUGHPUT
#ifdef TITLE
                                                fprintf(afp, "NumSamples NumThreads EnqueueThroughput DequeueThroughput\n");
#endif
                                                printf("%d %d %f %f\n", NUM_SAMPLES, CUR_NUM_THREADS, enqueuethroughput, dequeuethroughput);
                                                fprintf(afp, "%d %d %f %f\n", NUM_SAMPLES, CUR_NUM_THREADS, enqueuethroughput, dequeuethroughput);
#endif
                                                ClearXTaskQueue(&incoming);
                                                ClearXTaskQueue(&results);

                                                //#ifdef THROUGHPUT
                                                //			struct timespec tstart, tend;
                                                //#endif
                                                //			omp_set_num_threads(CUR_NUM_THREADS);
                                                //
                                                //#ifdef THROUGHPUT
                                                //			clock_gettime(CLOCK_MONOTONIC, &tstart);
                                                //#endif
                                                //#pragma omp parallel for schedule(static,1)
                                                //			for (int i = 0; i < NUM_SAMPLES; i++)
                                                //			{
                                                //#ifdef LATENCY
                                                //				start_tick = getticks();
                                                //#endif
                                                //				//Enqueue((atom) (i+1));
                                                ////				int tid=omp_get_thread_num();
                                                ////				printf("thread id:%d\n", tid);
                                                //				//nanosleep(&tim, &tim2);
                                                //				//cyclesleep(100);
                                                //				if(i < (NUM_SAMPLES/2))
                                                //					sleep(0);
                                                //				else
                                                //					sleep(1);
                                                //
                                                //				//usleep(1);
                                                //#ifdef LATENCY
                                                //				end_tick = getticks();
                                                //				pthread_mutex_lock(&lock);
                                                //				enqueuetimestamp[numEnqueue] = (end_tick-start_tick);
                                                //				__sync_fetch_and_add(&numEnqueue,1);
                                                //				pthread_mutex_unlock(&lock);
                                                //#endif
                                                //
                                                //			}
                                                //#ifdef THROUGHPUT
                                                //			clock_gettime(CLOCK_MONOTONIC, &tend);
                                                //			pthread_mutex_lock(&lock);
                                                //			double elapsed = ( tend.tv_sec - tstart.tv_sec ) + (( tend.tv_nsec - tstart.tv_nsec )/ 1E9);
                                                //			printf("elapsed time: %lf\n", elapsed);
                                                //			enqueuethroughput += ((NUM_SAMPLES*1.0)/elapsed);
                                                //			pthread_mutex_unlock(&lock);
                                                //#endif

                                                //ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp, rdtsc_overhead_ticks);

                                                //free(enqueuetimestamp);
                                                //free(dequeuetimestamp);
                                            }
                                            break;
                                        case 7:
                                            for (int k = 0; k < threadCount; k++) {
                                                InitQueue();
                                                ResetCounters();
                                                CUR_NUM_THREADS = threads[k];
                                                omp_set_num_threads(CUR_NUM_THREADS);

#ifdef THROUGHPUT
                                                ticks deq_st, deq_et;
                                                deq_st = getticks();
#endif
#pragma omp parallel for schedule(dynamic,1)
                                                for (int i = 0; i < NUM_SAMPLES; i++) {
                                                    pthread_t t;
                                                    pthread_create(&t, NULL, simplesleephandler, NULL);
                                                    pthread_join(t, NULL);
                                                }
#ifdef THROUGHPUT
                                                deq_et = getticks();
                                                ticks deq_diff_tick = deq_et - deq_st;
                                                double deq_elapsed = (deq_diff_tick / clockFreq);
                                                //printf("Time elapsed for dequeue: %f\n", deq_elapsed);
                                                dequeuethroughput = ((NUM_SAMPLES * 1000000000.0) / deq_elapsed);
#endif
#ifdef THROUGHPUT
#ifdef TITLE
                                                fprintf(afp, "NumSamples NumThreads Throughput\n");
#endif
                                                printf("%d %d %f\n", NUM_SAMPLES, CUR_NUM_THREADS, dequeuethroughput);
                                                fprintf(afp, "%d %d %f\n", NUM_SAMPLES, CUR_NUM_THREADS, dequeuethroughput);
#endif
                                            }
                                            break;
                                        case 8://Multiple Incoming SQueues - Single producer multiple consumer variation
                                            for (int k = 0; k < threadCount; k++) {
                                                CUR_NUM_THREADS = threads[k];
                                                NUM_QUEUES = CUR_NUM_THREADS;

                                                printf("Number of queues: %d\n", NUM_QUEUES);
                                                pick = (int *) malloc(sizeof (int) * NUM_QUEUES);

                                                printf("Random number distribution: ");
                                                for (int z = 0; z < NUM_QUEUES; z++) {
                                                    pick[z] = rand_interval(0, NUM_QUEUES);
                                                    printf("%d ", pick[z]);
                                                }

                                                printf("\n");

                                                InitQueues(NUM_QUEUES);
                                                ResetCounters();
                                                enqueuetimestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES);
                                                dequeuetimestamp = (ticks *) malloc(sizeof (ticks) * NUM_SAMPLES);
                                                for (int i = 0; i < NUM_SAMPLES; i++) {
                                                    enqueuetimestamp[i] = (ticks) 0;
                                                    dequeuetimestamp[i] = (ticks) 0;
                                                }

                                                pthread_t *worker_threads;
                                                pthread_t enqueue_thread;

                                                worker_threads = (pthread_t *) malloc(sizeof (pthread_t) * CUR_NUM_THREADS);
                                                enqueue_thread = (pthread_t) malloc(sizeof(pthread_t));

                                                cpu_set_t set;

                                                CPU_ZERO(&set);
                                                CPU_SET(0, &set);

                                                pthread_setaffinity_np(pthread_self(), sizeof (set), &set);

                                                //Set number of threads that will call the barrier_wait to total of enqueue and dequeue threads
                                                pthread_barrier_init(&barrier, NULL, (CUR_NUM_THREADS + 1));
                                                //Start the producer thread
                                                pthread_create(&enqueue_thread, NULL, enqueuespmc_handler, (void*)(1));
                                                //Start the consumer threads
                                                for (int i = 0; i < CUR_NUM_THREADS; i++) {

                                                    pthread_create(&worker_threads[i], NULL, workerspmc_handler, (void*)(unsigned long)(i));
                                                }

                                                pthread_join(enqueue_thread, NULL);
                                                for (int i = 0; i < CUR_NUM_THREADS; i++) {
                                                    pthread_join(worker_threads[i], NULL);
                                                }

                                                ComputeSummary(queueType, CUR_NUM_THREADS, afp, rfp, rdtsc_overhead_ticks);

                                                free(enqueuetimestamp);
                                                free(dequeuetimestamp);
                                            }
                                            break;
                                        default:

                                            break;
                                    }

                                    fclose(rfp);
                                    fclose(afp);
                                    printf("Done!!\n");

                                    return 0;
                                }
