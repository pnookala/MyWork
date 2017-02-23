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

#define MAX_SIZE    (32*1024*1024)        // Define maximum length of the queue
typedef int atom;


// List Function Prototypes
void InitQueue();             // Initialize the queue
void ClearQueue();            // Remove all items from the queue
void PrintQueue();            // Print all items from the queue
int Enqueue(atom elem);      // Enter an item in the queue
atom Dequeue();             // Remove an item from the queue
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
