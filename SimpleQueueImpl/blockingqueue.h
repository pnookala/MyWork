/*
 * Queue.h
 *
 *  Created on: Jan 26, 2017
 *      Author: pnookala
 */

#ifndef QUEUE_BLOCKING_H_
#define QUEUE_BLOCKING_H_

#include <pthread.h>
#include <semaphore.h>
#include "queue.h"

#define QUEUE_SIZE (32*1024*1024)

struct queue *create_queue_blocking(int size);
void dispose_queue_blocking(struct queue *q);
void enqueue_blocking(struct task_desc task, struct queue *q);
struct task_desc dequeue_blocking(struct queue *q);

#endif /* QUEUE_H_ */
