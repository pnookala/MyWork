/*
 * semiblockingqueue.h
 *
 *  Created on: Jan 31, 2017
 *      Author: pnookala
 */

#ifndef SEMIBLOCKINGQUEUE_H_
#define SEMIBLOCKINGQUEUE_H_

struct queue_root;

struct queue_head {
	struct queue_head *next;
};

struct thread_data {
	pthread_t thread_id;
	struct queue_root *queue;
	double locks_ps;
	unsigned long long rounds;
};

struct queue_root *ALLOC_QUEUE_ROOT();
void INIT_QUEUE_HEAD(struct queue_head *head);

void queue_put(struct queue_head *new, struct queue_root *root);

struct queue_head *queue_get(struct queue_root *root);

#endif /* SEMIBLOCKINGQUEUE_H_ */
