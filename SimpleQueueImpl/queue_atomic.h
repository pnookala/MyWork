/*
 * Queue.h
 *
 *  Created on: Jan 26, 2017
 *      Author: pnookala
 */

#ifndef QUEUE_ATOMIC_H_
#define QUEUE_ATOMIC_H_

#include <pthread.h>
#include <semaphore.h>
#include "queue.h"

struct queue *create_queue_atomic(int size);
void dispose_queue_atomic(struct queue *q);
void enqueue_atomic(struct task_desc task, struct queue *q);
struct task_desc dequeue_atomic(struct queue *q);

#endif /* QUEUE_H_ */
