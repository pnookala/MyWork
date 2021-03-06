//---------------------------------------------------------------
// File: Code130_Queue.h
// Purpose: Header file for a demonstration of a queue implemented
//        as an array.  Data type: Character
// Programming Language: C
// Author: Dr. Rick Coleman
//---------------------------------------------------------------
#ifndef SQUEUE_H
#define SQUEUE_H

#include <stdio.h>

#define NUM_SAMPLES 262144//14400000 //2^23
#define MAX_SIZE NUM_SAMPLES        // Define maximum length of the queue
typedef int atom;


// List Function Prototypes
void InitQueue();             // Initialize the queue
void ClearQueue();            // Remove all items from the queue
void PrintQueue();            // Print all items from the queue
int Enqueue(atom elem);      // Enter an item in the queue
atom Dequeue();             // Remove an item from the queue
int isEmpty();                // Return true if queue is empty
int isFull();                 // Return true if queue is full

// Define TRUE and FALSE if they have not already been defined
#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif

#endif // End of queue header
