/*
 * Queue.h
 *
 *  Created on: Jan 26, 2017
 *      Author: pnookala
 */

#ifndef QUEUE_H_
#define QUEUE_H_

#include <pthread.h>
#include <semaphore.h>

typedef unsigned long long ticks;
pthread_mutex_t lock;

struct task_desc {
	int task_id;
	int task_type;
	int num_threads;
	void *params;
};

struct queue {
	struct task_desc *tasks; /* the actual queue */
	int capacity;             /* maximum number of task slots */
	int rear;                 /* tasks[rear % capacity] is the last item */
	int front;    /* tasks[(front+1) % capacity] is the first */
	pthread_mutex_t lock;     /* protects access to the queue */
	sem_t task_sem;           /* counts available tasks in queue */
	sem_t spaces_sem;         /* counts available task slots */
};

struct queue *create_queue(int size);
void dispose_queue(struct queue *q);
void enqueue(struct task_desc task, struct queue *q);
struct task_desc dequeue(struct queue *q);

#endif /* QUEUE_H_ */
