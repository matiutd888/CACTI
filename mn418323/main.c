#include <stdlib.h>
#include <stdio.h>
#include "queue.h"

#define N 20

int main() {
    queue_t *q = queue_init(3);
    for(int i = 0; i < N; i++) {
        int *x = malloc(sizeof(int));
        *x = i;
        queue_push(q, (void*)x);
    }

    while(!queue_empty(q)) {
        int *x = queue_pop(q);

        printf("zdjalem %d\n", *x);
        free(x);
    }

    queue_destruct(q);
}

//
// Created by mateusz on 26.01.2021.
//

