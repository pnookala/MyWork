//---------------------------------------------------------------
// File: Code130_Queue.h
// Purpose: Header file for a demonstration of a queue implemented
//        as an array.  Data type: Character
// Programming Language: C
// Author: Dr. Rick Coleman
//---------------------------------------------------------------
#ifndef XTASKQUEUE_H
#define XTASKQUEUE_H

#include <stdio.h>
#include "squeue.h"

struct queue {
	atom *data __attribute__((aligned (4096)));
	int head, tail;
};

// List Function Prototypes
void InitXTaskQueue(struct queue *q);             // Initialize the queue
void ClearXTaskQueue(struct queue *q);            // Remove all items from the queue
void PrintXTaskQueue(struct queue *q);            // Print all items from the queue
int EnqueueToQ(atom elem, struct queue *q);      // Enter an item in the queue
atom DequeueFromQ(struct queue *q);             // Remove an item from the queue
int isQueueEmpty(struct queue *q);                // Return true if queue is empty
int isQueueFull(struct queue *q);

#endif // End of queue header
