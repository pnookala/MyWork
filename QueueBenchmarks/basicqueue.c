#include "basicqueue.h"

void InitBasicQueue()
{
  CIRCLEQ_INIT(&head);
  pthread_mutex_init(&lock, NULL);
}

void BasicEnqueue(int i)
{
	struct basicentry* n = malloc(sizeof(struct basicentry));
	n->elem = i;
	pthread_mutex_lock(&lock);
    CIRCLEQ_INSERT_TAIL(&head, n, entries);
    pthread_mutex_unlock(&lock);
#ifdef VERBOSE
    printf("Added %d to the queue\n", n->elem);
#endif
}

int BasicDequeue()
{
  // Remove a number from the queue
  struct basicentry *n;
  pthread_mutex_lock(&lock);
  n = CIRCLEQ_FIRST(&head);
  CIRCLEQ_REMOVE(&head, head.cqh_first, entries);
  pthread_mutex_unlock(&lock);
#ifdef VERBOSE
  printf("Removed %d from the queue\n", n->elem);
#endif
  return n->elem;
}
