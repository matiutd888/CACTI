#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>

typedef struct m_queue queue_t;

queue_t *queue_init(size_t size);

int queue_empty(queue_t *q);

void queue_push(queue_t *q, void *x);

void* queue_pop(queue_t *q);

void queue_destruct(queue_t *q);

size_t queue_size(queue_t *q);

#endif //QUEUE_H