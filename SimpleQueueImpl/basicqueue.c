#include "basicqueue.h"

void InitBasicQueue()
{
  CIRCLEQ_INIT(&head);
  pthread_mutex_init(&lock, NULL);
}

void BasicEnqueue(int i)
{
	struct entry* n = malloc(sizeof(struct entry));
	n->elem = i;
	pthread_mutex_lock(&lock);
    CIRCLEQ_INSERT_TAIL(&head, n, entries);
    pthread_mutex_unlock(&lock);
    printf("Added %d to the queue\n", n->elem);

}

int BasicDequeue()
{
  // Remove a number from the queue
  struct entry *n;
  pthread_mutex_lock(&lock);
  n = CIRCLEQ_FIRST(&head);
  CIRCLEQ_REMOVE(&head, head.cqh_first, entries);
  pthread_mutex_unlock(&lock);
  printf("Removed %d from the queue\n", n->elem);

  return n->elem;
}
