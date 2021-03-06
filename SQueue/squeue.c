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
#include "squeue.h"


// Declare these as static so no code outside of this source
// can access them.
int head, tail;	// Declare global indices to head and tail of queue


//atom *theQueue;	// The queue
//atom theQueue[MAX_SIZE];	// The queue
//static volatile atom theQueue[MAX_SIZE];
atom theQueue[MAX_SIZE] __attribute__((aligned (4096)));
//static atom theQueue[MAX_SIZE];


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
void InitQueue()
{
    head = tail = -1;
 
    //dynamically allocate memory to hold data samples
    //theQueue = (atom *)malloc(sizeof(atom)*MAX_SIZE);
    
    for (int i=0;i<MAX_SIZE;i++)
        theQueue[i] = 0;
}

//--------------------------------------------
// Function: ClearQueue()
// Purpose: Remove all items from the queue
// Returns: void
//--------------------------------------------
void ClearQueue()
{
    head = tail = -1; // Reset indices to start over
    for (int i=0;i<MAX_SIZE;i++)
        theQueue[i] = 0;
}

void PrintQueue()
{
    printf("printing queue");
    for (int i=0;i<MAX_SIZE;i++)
    {
        if (theQueue[i] != 0) printf("%d %d\n", i, theQueue[i]);
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
inline int Enqueue(atom elem)
{
    // Check to see if the Queue is full
    //could pose problems in concurent enqueue, perhaps leave extra space in the queue...
    //while ((tail - MAX_SIZE) == head);
    //check if its full, and wait...
    
    // Increment tail index
    int cur_tail = __sync_add_and_fetch(&tail,1);
    
    // Add the item to the Queue
    theQueue[cur_tail % MAX_SIZE] = elem;
    
    //printf("Enqueue: %d\n", elem);

    return TRUE;
}


//--------------------------------------------
// Function: Dequeue()
// Purpose: Dequeue an item from the Queue.
// Returns: TRUE if dequeue was successful
//		or FALSE if the dequeue failed.
//--------------------------------------------

inline atom Dequeue()
{
    atom elem;
    
    
    
    int cur_head = __sync_add_and_fetch(&head,1);
    
    volatile atom * target = theQueue + (cur_head % MAX_SIZE);
    
    while (!*target)
    {}
    
    
    //elem = __sync_val_compare_and_swap(&theQueue[cur_head % MAX_SIZE], theQueue[cur_head % MAX_SIZE], 0);
    elem = *target;
            
    // probably don't need to sync here, but at least do a software barrier
    __sync_fetch_and_and(target,0);
    
    //printf("Dequeue: %d\n", elem);

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
inline int isEmpty()
{
    return (head == tail);
}

//--------------------------------------------
// Function: isFull()
// Purpose: Return true if the queue is full.
// Returns: TRUE if full, otherwise FALSE
// Note: C has no boolean data type so we use
//	the defined int values for TRUE and FALSE
//	instead.
//--------------------------------------------
inline int isFull()
{
    // Queue is full if tail has wrapped around
    //	to location of the head.  See note in
    //	Enqueue() function.
    return ((tail - MAX_SIZE) >= head);
}
