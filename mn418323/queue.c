#include "queue.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#define ELEMENT_SIZE sizeof(void *)
#define RESIZE_MULTIPLIER 2
#define EXIT_CODE_MALLOC_FAIL 1

struct m_queue {
    size_t write, read;
    void **arr;
    size_t size;
    size_t count;
};

queue_t *queue_init(size_t size) {
    queue_t *q = NULL;
    q = malloc(sizeof(queue_t));
    if (q == NULL)
        return NULL;

    q->arr = NULL;
    q->arr = calloc(size, sizeof(void *));
    if (q->arr == NULL) {
        return NULL;
    }
    q->size = size;
    q->write = 0;
    q->read = 0;
    q->count = 0;
    return q;
}

static bool is_full(queue_t *q) {
    return q->count == q->size;
}

int queue_empty(queue_t *q) {
    return q->count == 0;
}

//void swap(void **a, void **b) {
//    void *tmp = *a;
//    *a = *b;
//    *b = tmp;
//}

void reverse(queue_t *q, size_t l, size_t r) {
    size_t mid = (l + r) / 2;
    for (size_t i = l; i <= mid; i++) {
        void *foo = q->arr[i];
        q->arr[i] = q->arr[r - i + l];
        q->arr[r - i + l] = foo;
    }
}

void cyclic(queue_t *q) {
    if (q->size < 2)
        return;
    if (q->read == 0)
        return;

    reverse(q, 0, q->write - 1);
    reverse(q, q->read, q->size - 1);
    reverse(q, 0, q->size - 1);
    q->read = 0;
    q->write = q->size;
}

static void resize(queue_t *q) {
    q->size = RESIZE_MULTIPLIER * q->size + 1;
    q->arr = realloc(q->arr, ELEMENT_SIZE * q->size);

    if (q->arr == NULL)
        exit(EXIT_CODE_MALLOC_FAIL);

    for (size_t i = q->count; i < q->size; i++) {
        q->arr[i] = NULL;
    }
}

static void enhance_queue(queue_t *q) {
    print_queue(q);
    cyclic(q);
    print_queue(q);
    resize(q);
}

void queue_push(queue_t *q, void *v) {
    if (is_full(q))
        enhance_queue(q);
    q->arr[q->write] = v;
    q->write = (q->write + 1) % q->size;
    q->count++;
}

void *queue_pop(queue_t *q) {
    if (q->count == 0) {
        return NULL;
    }
    void *r = q->arr[q->read];
    q->arr[q->read] = NULL;
    q->read = (q->read + 1) % q->size;
    q->count--;
    return r;
}

void free_array(void **arr, size_t length) {
    if (arr == NULL)
        return;
    for (size_t i = 0; i < length; i++) {
        if (arr[i] != NULL)
            free(arr[i]);
    }
    free(arr);
}

void queue_destruct(queue_t *q) {
    if (q == NULL)
        return;
    free_array(q->arr, q->size);
    free(q);
}

size_t queue_size(queue_t *q) {
    return q->count;
}

void print_queue(queue_t *q) {
    for (int i = 0; i < q->count; ++i) {
        printf("%d -> ", *(int *) q->arr[(q->read + i) % (q->size)]);
    }
    printf("\n");
}
