#include <stdlib.h>
#include "queue_atomic.h"
#include <sys/syscall.h>

#define QUEUE_SIZE (32*1024*1024)
static const struct task_desc EmptyTask;

struct queue *create_queue_atomic(int size)
{
	struct queue *q = (struct queue *) malloc(sizeof(struct queue));

	q->tasks = (struct task_desc *) malloc(sizeof(struct task_desc) * size);
	q->capacity = size;
	q->front = -1;
	q->rear = -1;
	pthread_mutex_init(&q->lock, NULL);
	sem_init(&q->task_sem, 0, 0);
	sem_init(&q->spaces_sem, 0, size);

	return q;
}

void dispose_queue_atomic(struct queue *q)
{
	free(q->tasks);
	pthread_mutex_destroy(&q->lock);
	sem_destroy(&q->task_sem);
	sem_destroy(&q->spaces_sem);
	free(q);
}

__inline__ void enqueue_atomic(struct task_desc task, struct queue *q)
{
	struct task_desc emptyTask = EmptyTask;
	while(1)
	{
		int last = __sync_add_and_fetch(&(q->rear), 1);
		if(__atomic_compare_exchange(&(q->tasks[(last) % (QUEUE_SIZE)]), &emptyTask, &task, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
		{
			//printf("Enqueue: %d\n", task.task_id);
			break;
		}
	}
	//q->tasks[(last) % (QUEUE_SIZE)] = task;

	return;
}

__inline__ struct task_desc dequeue_atomic(struct queue *q)
{
	struct task_desc task = EmptyTask;
	struct task_desc emptyTask = EmptyTask;
	//Do we need a way to check queue is not empty
	do
	{
		int first = __sync_add_and_fetch(&(q->front), 1);
		__atomic_exchange(&(q->tasks[(first) % QUEUE_SIZE]), &emptyTask, &task,  __ATOMIC_RELAXED);
		/*if(task.task_id != 0)
		{
			printf("Dequeue: %d [%u]\n", task.task_id, pthread_self());
		}*/
	}
	while(task.task_id == 0);
	//s__asm__ __volatile__("" : : : "memory");

	return task;
}
