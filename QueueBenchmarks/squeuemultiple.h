//---------------------------------------------------------------
// File: Code130_Queue.h
// Purpose: Header file for a demonstration of a queue implemented
//        as an array.  Data type: Character
// Programming Language: C
// Author: Dr. Rick Coleman
//---------------------------------------------------------------
#ifndef SQUEUEMULTIPLE_H
#define SQUEUEMULTIPLE_H

#include <stdio.h>
#include "squeue.h"

#define MAX_SIZE NUM_SAMPLES        // Define maximum length of the queue

struct theQueue
{
	int head;
	int tail;
	atom *data __attribute__((aligned (4096)));
};

struct theQueue **queues;

// List Function Prototypes
void InitQueues(int numQueues);             // Initialize the queue
void ClearQueues();            // Remove all items from the queue
void PrintQueues();            // Print all items from the queue
int EnqueueMultiple(atom elem, struct theQueue *queue, int queueID);      // Enter an item in the queue
atom DequeueMultiple(struct theQueue *q, int queueID);             // Remove an item from the queue
//inline int isEmpty();                // Return true if queue is empty
//inline int isFull();                 // Return true if queue is full

// Define TRUE and FALSE if they have not already been defined
#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif

#endif // End of queue header
