#include "../cacti.h"
#include "../queue.h"

#include <stdlib.h>
#include <stdio.h>

#define N 2

int main(){
    queue_t *q = queue_init(2);
    int* one = malloc(sizeof (int));
    *one = 1;
    int* two = malloc(sizeof (int));
    *two = 2;
    int* three = malloc(sizeof (int));
    *three = 3;
    int* four = malloc(sizeof (int));
    *four = 4;

    int* five = malloc(sizeof (int));
    *five = 5;

    int* six = malloc(sizeof (int));
    *six = 6;

    int* seven = malloc(sizeof (int));
    *seven = 7;
    int* eight = malloc(sizeof (int));
    *eight = 8;

    queue_push(q, one);
    print_queue(q); // 1

    queue_push(q, seven);
    print_queue(q); // 1 -> 7

    queue_pop(q); // 7
    queue_push(q, eight); // 7-> 8

    print_queue(q);  // 7 -> 8

    queue_push(q, three);
    queue_push(q, two); // 7->8->3->2

    print_queue(q); // 7->8->3->2

    queue_pop(q);
    queue_pop(q);
    queue_pop(q); // 2

    print_queue(q); // 2

    queue_push(q, three); // 2-3

    print_queue(q); // 2-3

    queue_push(q, four); // 2-3-4

    print_queue(q); // 2-3-4

    queue_push(q, five); // 2-3-4-5

    print_queue(q); // 2-3-4-5

    queue_push(q, six); // 2-3-4-5-6

    print_queue(q); // 2-3-4-5-6
    queue_push(q, one); // 2-3-4-5-6-1


    print_queue(q); // 2-3-4-5-6-1
    int tmp = queue_size(q);

    queue_destruct(q);
    return 0;
}