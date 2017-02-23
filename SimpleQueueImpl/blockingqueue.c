#include <stdlib.h>
#include "blockingqueue.h"

struct queue *create_queue_blocking(int size)
{
	struct queue *q = (struct queue *) malloc(sizeof(struct queue));

	q->tasks = (struct task_desc *) malloc(sizeof(struct task_desc) * size);
	q->capacity = size;
	q->front = 0;
	q->rear = 0;
	pthread_mutex_init(&q->lock, NULL);
	sem_init(&q->task_sem, 0, 0);
	sem_init(&q->spaces_sem, 0, size);

	return q;
}

void dispose_queue_blocking(struct queue *q)
{
	free(q->tasks);
	pthread_mutex_destroy(&q->lock);
	sem_destroy(&q->task_sem);
	sem_destroy(&q->spaces_sem);
	free(q);
}

__inline__ void enqueue_blocking(struct task_desc task, struct queue *q)
{
	sem_wait(&q->spaces_sem);
	pthread_mutex_lock(&q->lock);
	//int last = __sync_fetch_and_add(&(q->rear), 1);
	q->tasks[(++q->rear) % (QUEUE_SIZE)] = task;
	pthread_mutex_unlock(&q->lock);
	sem_post(&q->task_sem);

	return;
}

__inline__ struct task_desc dequeue_blocking(struct queue *q)
{
	struct task_desc task;
	sem_wait(&q->task_sem);
	pthread_mutex_lock(&q->lock);
	//int first = __sync_fetch_and_add(&(q->front), 1);
	task = q->tasks[(++q->front) % QUEUE_SIZE];
	/*pthread_t self_id;
	self_id=pthread_self();
	printf("Dequeue: %d [%u]\n", task.task_id, self_id);*/
	pthread_mutex_unlock(&q->lock);
	sem_post(&q->spaces_sem);

	return task;
}
