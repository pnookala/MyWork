#include <boost/thread/thread.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/atomic.hpp>
#include <iostream>
#include <stdio.h>
#include <pthread.h>

const int NUM_THREADS = 1;
static int NUM_SAMPLES = 8;//128000000;
typedef long unsigned int ticks;
ticks *enqueuetimestamp, *dequeuetimestamp;
float clockFreq;

static int numEnqueue = 0;
static int numDequeue = 0;
static int CUR_NUM_THREADS = 0;
volatile int numEnqueueThreadsCreated = 0, numDequeueThreadsCreated = 0;

int enqueuethroughput, dequeuethroughput = 0;

static pthread_barrier_t barrier;

boost::atomic_int producer_count(0);
boost::atomic_int consumer_count(0);

boost::lockfree::queue<int> queue(128);

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

void ResetCounters() {
	numEnqueue = 0;
	numDequeue = 0;
	numEnqueueThreadsCreated = 0;
	numDequeueThreadsCreated = 0;
}

void ComputeSummary(int type, int numThreads, FILE* afp, int rdtsc_overhead)
{

	using namespace std;
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
		if (numEnqueueTicks[i]>enqueuetickMax) enqueuetickMax = numEnqueueTicks[i];
		if (numEnqueueTicks[i]<enqueuetickMin) enqueuetickMin = numEnqueueTicks[i];

		numDequeueTicks[i]= dequeuetimestamp[i]-rdtsc_overhead;

		totalDequeueTicks += numDequeueTicks[i];
		if (numDequeueTicks[i]>dequeuetickMax) dequeuetickMax = numDequeueTicks[i];
		if (numDequeueTicks[i]<dequeuetickMin) dequeuetickMin = numDequeueTicks[i];
	}

	//compute average
	double tickEnqueueAverage = (totalEnqueueTicks/(NUM_SAMPLES));
	double tickDequeueAverage = (totalDequeueTicks/(NUM_SAMPLES));

	std::cout << "Num threads: " << numThreads << " Num samples: " << NUM_SAMPLES << std::endl;
	std::cout << "Enqueue Min: " << enqueuetickMin << std::endl;
	std::cout << "Dequeue Min: " << dequeuetickMin << std::endl;

	std::cout << "Enqueue Max: " << enqueuetickMax << std::endl;
	std::cout << "Dequeue Max: " << dequeuetickMax << std::endl;

	std::cout << "Average Enqueue : " << tickEnqueueAverage <<std::endl;
	std::cout << "Average Dequeue : " << tickDequeueAverage<<std::endl;

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

	std::cout << "Median Enqueue : " << enqueuetickmedian <<std::endl;
	std::cout << "Median Dequeue : " << dequeuetickmedian<<std::endl;

	double enqueueMinTime = ((enqueuetickMin)/clockFreq);
	double dequeueMinTime = ((dequeuetickMin)/clockFreq);
	double enqueueMaxTime = ((enqueuetickMax)/clockFreq);
	double dequeueMaxTime = ((dequeuetickMax)/clockFreq);
	double enqueueAvgTime = ((tickEnqueueAverage)/clockFreq);
	double dequeueAvgTime = ((tickDequeueAverage)/clockFreq);

	std::cout << "Enqueue Min Time (ns): " << enqueueMinTime << std::endl;
	std::cout << "Dequeue Min Time (ns): " << dequeueMinTime << std::endl;

	std::cout << "Enqueue Max Time (ns): " << enqueueMaxTime << std::endl;
	std::cout << "Dequeue Max Time (ns): " << dequeueMaxTime << std::endl;

	std::cout << "Average Enqueue Time (ns): " << enqueueAvgTime << std::endl;
	std::cout << "Average Dequeue Time (ns): " << dequeueAvgTime << std::endl;

	fprintf(afp, "%d %d %d %ld %ld %ld %ld %lf %lf %ld %ld %lf %lf %lf %lf %lf %lf\n",type, numThreads, NUM_SAMPLES, enqueuetickMin, dequeuetickMin, enqueuetickMax, dequeuetickMax, tickEnqueueAverage, tickDequeueAverage, enqueuetickmedian, dequeuetickmedian, enqueueMinTime, dequeueMinTime, enqueueMaxTime, dequeueMaxTime, enqueueAvgTime, dequeueAvgTime);
#endif
#ifdef THROUGHPUT
	printf("NumSamples:%d NumThreads:%d EnqueueThroughput:%d DequeueThroughput:%d\n", NUM_SAMPLES, numThreads, enqueuethroughput, dequeuethroughput);
	fprintf(afp, "NumSamples NumThreads EnqueueThroughput DequeueThroughput\n");
	fprintf(afp, "%d %d %d %d\n", NUM_SAMPLES, numThreads, enqueuethroughput, dequeuethroughput);
#endif
}

boost::atomic<bool> done (false);
void consumer(void)
{
	int value;

	ticks start_tick,end_tick;

	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / NUM_THREADS;
	//Wait until all threads call the barrier_wait. This is used for getting highest contention with threads
		pthread_barrier_wait(&barrier);

	#ifdef VERBOSE
		printf("Dequeue thread woke up\n");
	#endif
#ifdef THROUGHPUT
	start_tick = getticks();
#endif
	for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
	{
#ifdef LATENCY
		start_tick = getticks();
#endif
		while(!queue.pop(value));
#ifdef LATENCY
		end_tick = getticks();
		dequeuetimestamp[numDequeue] = (end_tick-start_tick);
		__sync_fetch_and_add(&numDequeue,1);
#endif
	}
#ifdef THROUGHPUT
	end_tick = getticks();
	ticks diff_tick = end_tick - start_tick;
	double elapsed = (diff_tick*1E-9)/clockFreq;
	printf("ticks: %ld, samples: %d\n", diff_tick, NUM_SAMPLES_PER_THREAD);
	printf("dequeue throughput: %lf\n",((NUM_SAMPLES_PER_THREAD)/elapsed));
	int throughput = (int)((NUM_SAMPLES_PER_THREAD)/elapsed);
	__sync_fetch_and_add(&dequeuethroughput, (throughput));
#endif
}

void producer(void)
{
	ticks start_tick,end_tick;

	int NUM_SAMPLES_PER_THREAD = NUM_SAMPLES / NUM_THREADS;
	pthread_barrier_wait(&barrier);
	#ifdef VERBOSE
		printf("Enqueue thread woke up\n");
	#endif

	#ifdef THROUGHPUT
		start_tick = getticks();
	#endif
	for (int i=0;i<NUM_SAMPLES_PER_THREAD;i++)
	{
#ifdef LATENCY
		start_tick = getticks();
#endif
		while (!queue.push(i));
#ifdef LATENCY
		end_tick = getticks();
		enqueuetimestamp[numEnqueue] = (end_tick-start_tick);
		__sync_fetch_and_add(&numEnqueue,1);
#endif
	}
#ifdef THROUGHPUT
	end_tick = getticks();
	ticks diff_tick = end_tick - start_tick;
	double elapsed = ((diff_tick*1E-9))/clockFreq;
	printf("ticks: %ld, samples: %d\n", diff_tick, NUM_SAMPLES_PER_THREAD);
	printf("enqueue throughput: %lf\n",((NUM_SAMPLES_PER_THREAD)/elapsed));
	int throughput = (int)((NUM_SAMPLES_PER_THREAD)/elapsed);
	__sync_fetch_and_add(&enqueuethroughput, (throughput));
#endif
	return;
}

int main(int argc, char* argv[])
{
	using namespace std;

	boost::thread_group producer_threads, consumer_threads;

	int threadCount=0;
	int queueType;
	int *threads = (int *) malloc(sizeof(int));
	char* fileName1, *fileName2;

	//Inputs are type of queue, thread list,
	if(argc != 6)
	{
		std::cout << "Usage: <QueueType 5-STL Boost Lockfree>, \nThreads-1,2,4,6,8,12,16,24,32,48,57,96,114,192,228,384,456,768,912,1024, \nRaw data file name: <name>,  \nSummary file name: <name>, \nClock Frequency in GHz: <3.4>\n";
		exit(-1);
	}
	else
	{
		char* arg = argv[1];
		queueType = atoi(arg);

		switch(queueType)
		{
		case 5:
			std::cout << "Queue type: STL Boost Lockfree Queue \n";
			break;
		default:
			std::cout << "Usage: <QueueType 5-STL Boost Lockfree>, \nThreads-1,2,4,6,8,12,16,24,32,48,57,96,114,192,228,384,456,768,912,1024, \nRaw data file name: <name>,  \nSummary file name: <name>, \nClock Frequency in GHz: <3.4>\n";
			exit(-1);
			break;
		}

		char* str = argv[2];
		char *thread;
		thread = strtok (str,",");
		std::cout << "Thread list: ";
		while (thread != NULL)
		{
			threads[threadCount] = atoi(thread);
			threadCount++;
			std::cout << " " << *thread;
			thread = strtok (NULL, ",");
		}

		std::cout << endl;

		fileName1 = argv[3];
		fileName2 = argv[4];
		clockFreq = atof(argv[5]);

		std::cout << "Num of samples: " << NUM_SAMPLES << endl;
		std::cout << "Thread list count: " << threadCount << endl;
		std::cout << "Output files: " << fileName1 << " " << fileName2 << endl;
		std::cout << "Clock frequency: " << clockFreq << endl;
	}
	int rdtsc_overhead_ticks = 0;

	//Open file for storing data
#ifdef RAW
	FILE *rfp=fopen(fileName1, "a");
#endif
	FILE *afp=fopen(fileName2, "a");

#ifdef CALIBRATE
	//Calibrate RDTSC
	ticks start_tick = (ticks)0;
	ticks end_tick = (ticks)0;
	ticks totalTicks = (ticks)0;

	for (int i = 0; i < NUM_SAMPLES; i++)
	{
		start_tick = getticks();
		end_tick = getticks();

		totalTicks += (end_tick - start_tick);
	}

	rdtsc_overhead_ticks = (totalTicks/NUM_SAMPLES);
	std::cout << "RDTSC time: " << rdtsc_overhead_ticks <<  endl;
	fprintf(rfp, "RDTSC time: %d\n", rdtsc_overhead_ticks);
	fprintf(afp, "RDTSC time: %d\n", rdtsc_overhead_ticks);

#endif
	//Initialization
	enqueuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);
	dequeuetimestamp = (ticks *)malloc(sizeof(ticks)*NUM_SAMPLES);

	for (int i=0;i<NUM_SAMPLES;i++)
	{
		enqueuetimestamp[i] = (ticks)0;
		dequeuetimestamp[i] = (ticks)0;
	}

#ifdef RAW
	fprintf(rfp, "QueueType NumSamples EnqueueCycles DequeueCycles NumThreads EnqueueTime(ns) DequeueTime(ns)\n");
#endif
#ifdef VERBOSE
	std::cout << "QueueType NumSamples EnqueueCycles DequeueCycles NumThreads EnqueueTime(ns) DequeueTime(ns)\n";
#endif
#ifdef LATENCY
	fprintf(afp, "QueueType NumThreads EnqueueMin DequeueMin EnqueueMax DequeueMax EnqueueAverage DequeueAverage EnqueueMedian DequeueMedian EnqueueMinTime DequeueMinTime EnqueueMaxTime DequeueMaxTime EnqueueAverageTime DequeueAverageTime\n");
#endif
	for (int k = 0; k < threadCount; k++)
	{
		CUR_NUM_THREADS = (threads[k]/2);

		//Set number of threads that will call the barrier_wait to total of enqueue and dequeue threads
		pthread_barrier_init(&barrier, NULL, threads[k]);

		for (int i = 0; i != CUR_NUM_THREADS; i++)
		{
			producer_threads.create_thread(producer);
			consumer_threads.create_thread(consumer);
		}

		producer_threads.join_all();
		consumer_threads.join_all();

		for(int i=0;i<NUM_SAMPLES;i++)
		{
			double enqueueTime = ((enqueuetimestamp[i]-rdtsc_overhead_ticks)/clockFreq);
			double dequeueTime = ((dequeuetimestamp[i]-rdtsc_overhead_ticks)/clockFreq);
#ifdef RAW
			fprintf(rfp, "%d %d %ld %ld %d %lf %lf\n", queueType, NUM_SAMPLES, (enqueuetimestamp[i]-rdtsc_overhead_ticks), (dequeuetimestamp[i]-rdtsc_overhead_ticks), CUR_NUM_THREADS, enqueueTime,dequeueTime);
#endif
#ifdef VERBOSE
			cout << queueType << " " << NUM_SAMPLES << " " << (enqueuetimestamp[i]-rdtsc_overhead_ticks) << " " << (dequeuetimestamp[i]-rdtsc_overhead_ticks) << " " << CUR_NUM_THREADS << " " << enqueueTime << " " << dequeueTime << endl;
#endif
		}

		ComputeSummary(queueType, CUR_NUM_THREADS, afp, rdtsc_overhead_ticks);

		free(enqueuetimestamp);
		free(dequeuetimestamp);

		return 0;
	}
}
