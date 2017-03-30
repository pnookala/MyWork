//---------------------------------------------------------------
// File: Code130_Queue.c
// Purpose: Implementation file for a demonstration of a queue
//		implemented as an array.    Data type: Character
// Programming Language: C
// Author: Dr. Rick Coleman
// Date: February 11, 2002
//---------------------------------------------------------------
#include <sys/types.h>
#include <stdlib.h>
#include <sched.h>
#include "squeuemultiple.h"

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

//--------------------------------------------
// Function: InitQueue()
// Purpose: Initialize queue to empty.
// Returns: void
//--------------------------------------------
void InitQueues(int numQueues)
{
#ifdef VERBOSE
	printf("Number of queues: %d\n", numQueues);
#endif
	queues = (struct theQueue **) malloc(sizeof(struct theQueue) * numQueues);
	for(int i=0; i<numQueues; i++)
	{
		struct theQueue *queue = malloc(sizeof(struct theQueue));
		queue->head = -1;
		queue->tail = -1;
		for (int i=0;i<MAX_SIZE;i++)
		    queue->data[i] = 0;

		queues[i] = queue;
	}
}

//--------------------------------------------
// Function: ClearQueue()
// Purpose: Remove all items from the queue
// Returns: void
//--------------------------------------------
void ClearQueues()
{
    free(queues);
}

void PrintQueues()
{
//    printf("printing queue");
//    for (int i=0;i<MAX_SIZE;i++)
//    {
//        if (theQueue[i] != 0) printf("%d %d\n", i, theQueue[i]);
//    }
}


//--------------------------------------------
// Function: Enqueue()
// Purpose: Enqueue an item into the queue.
// Returns: TRUE if enqueue was successful
//		or FALSE if the enqueue failed.
// Note: We let head and tail continuing
//		increasing and use [head % MAX_SIZE]
//		and [tail % MAX_SIZE] to get the real
//		indices.  This automatically handles
//		wrap-around when the end of the array
//		is reached.
//--------------------------------------------
//should be thread safe, with no locks, just one atomic operation
inline int EnqueueMultiple(atom elem, struct theQueue *q, int queueID)
{
    // Check to see if the Queue is full
    //could pose problems in concurent enqueue, perhaps leave extra space in the queue...
    //while ((tail - MAX_SIZE) == head);
    //check if its full, and wait...

    // Increment tail index
    int cur_tail = __sync_add_and_fetch(&q->tail,1);

    // Add the item to the Queue
    q->data[cur_tail % MAX_SIZE] = elem;
#ifdef VERBOSE
    printf("QueueID: %d, Enqueue: %d\n", queueID, elem);
#endif
    return TRUE;
}


//--------------------------------------------
// Function: Dequeue()
// Purpose: Dequeue an item from the Queue.
// Returns: TRUE if dequeue was successful
//		or FALSE if the dequeue failed.
//--------------------------------------------

inline atom DequeueMultiple(struct theQueue *q, int queueID)
{
	atom elem = 0;
    int cur_head = __sync_add_and_fetch(&q->head,1);

    volatile atom * target = q->data + (cur_head % MAX_SIZE);

    while (!*target)
    {}

    elem = *target;

    // probably don't need to sync here, but at least do a software barrier
    __sync_fetch_and_and(target,0);

#ifdef VERBOSE
    printf("QueueID: %d, Dequeue: %d\n", queueID, elem);
#endif
    return elem;
    
}

//--------------------------------------------
// Function: isEmpty()
// Purpose: Return true if the queue is empty
// Returns: TRUE if empty, otherwise FALSE
// Note: C has no boolean data type so we use
//	the defined int values for TRUE and FALSE
//	instead.
//--------------------------------------------
//inline int isEmpty()
//{
//    return (head == tail);
//}

//--------------------------------------------
// Function: isFull()
// Purpose: Return true if the queue is full.
// Returns: TRUE if full, otherwise FALSE
// Note: C has no boolean data type so we use
//	the defined int values for TRUE and FALSE
//	instead.
//--------------------------------------------
//inline int isFull()
//{
//    // Queue is full if tail has wrapped around
//    //	to location of the head.  See note in
//    //	Enqueue() function.
//    return ((tail - MAX_SIZE) >= head);
//}
