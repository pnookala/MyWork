//---------------------------------------------------------------
// File: Code130_Queue.c
// Purpose: Implementation file for a demonstration of a queue
//		implemented as an array.    Data type: Character
// Programming Language: C
// Author: Dr. Rick Coleman
// Date: February 11, 2002
//---------------------------------------------------------------
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sched.h>
#include "xtaskqueue.h"

//--------------------------------------------
// Function: InitQueue()
// Purpose: Initialize queue to empty.
// Returns: void
//--------------------------------------------
void InitXTaskQueue(struct queue *q)
{
    q->head = -1;
    q->tail = -1;
    
    for (int i=0;i<MAX_SIZE;i++)
        q->data[i] = 0;
}

//--------------------------------------------
// Function: ClearQueue()
// Purpose: Remove all items from the queue
// Returns: void
//--------------------------------------------
void ClearXTaskQueue(struct queue *q)
{
    q->head = q->tail = -1; // Reset indices to start over
    for (int i=0;i<MAX_SIZE;i++)
        q->data[i] = 0;
}

void PrintXTaskQueue(struct queue *q)
{
    printf("printing queue\n");
    for (int i=0;i<MAX_SIZE;i++)
    {
        if (q->data[i] != 0) printf("%d %d\n", i, q->data[i]);
    }
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
inline int EnqueueToQ(atom elem, struct queue *q)
{
    // Increment tail index
    int cur_tail = __sync_add_and_fetch(&q->tail,1);
    // Add the item to the Queue
    q->data[cur_tail % MAX_SIZE] = elem;
#ifdef VERBOSE
    printf("Enqueue: %d\n", elem);
#endif
    return TRUE;
}


//--------------------------------------------
// Function: Dequeue()
// Purpose: Dequeue an item from the Queue.
// Returns: TRUE if dequeue was successful
//		or FALSE if the dequeue failed.
//--------------------------------------------

inline atom DequeueFromQ(struct queue *q)
{
    atom elem;

    int cur_head = __sync_add_and_fetch(&q->head,1);
    
    volatile atom * target = q->data + (cur_head % MAX_SIZE);
    
    while (!*target)
    {}
    
    elem = *target;
            
    // probably don't need to sync here, but at least do a software barrier
    __sync_fetch_and_and(target,0);
#ifdef VERBOSE
    printf("Dequeue: %d\n", elem);
#endif
    return elem;
    
}
