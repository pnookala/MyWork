/*
 * basicqueue.h
 *
 *  Created on: Feb 20, 2017
 *      Author: pnookala
 */

#ifndef BASICQUEUE_H_
#define BASICQUEUE_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <pthread.h>

CIRCLEQ_HEAD(circleq, entry) head;
struct circleq *headp;              /* Circular queue head. */
struct entry {
  CIRCLEQ_ENTRY(entry) entries;   /* Circular queue. */
  int elem;
};
pthread_mutex_t lock;

void InitBasicQueue();
void BasicEnqueue(int i);
int BasicDequeue();

#endif /* BASICQUEUE_H_ */
