#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>

// Queue implemented as dynamic cyclic array of pointers.
// Propably woud've been implemented as dynamic array of
// actor_id_t, but i thought, that dynamic array of messages is also nessesary.
// (Very late answear on forum proved me wrong :) )

typedef struct m_queue queue_t;

queue_t *queue_init(size_t size);

int queue_empty(queue_t *q);

void queue_push(queue_t *q, void *x);

void* queue_pop(queue_t *q);

void queue_destruct(queue_t *q);

size_t queue_size(queue_t *q);

#endif //QUEUE_H
