#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "cacti.h"

#define MSG_SON 1
#define MSG_FACT 2

typedef struct informacja {
    unsigned long long k_fact;
    int k;
    int stop;
} fact_t;

void spawn(void **stateptr, size_t size, void *data);

void hello_father(void **stateptr, size_t size, void *data);

void hello(void **stateptr, size_t size, void *data);

void get_son_id(void **stateptr, size_t size, void *data);

act_t propmts[3] = {hello, get_son_id, spawn};
role_t role = {
        .prompts = propmts,
        .nprompts = 3
};

void calculate(void **stateptr, size_t size, void *data);
act_t prompts_origin[4] = {hello_father, get_son_id, spawn, calculate};


role_t role_father = {
        .prompts = prompts_origin,
        .nprompts = 4
};
void get_son_id(void **stateptr, size_t size, void *data) {
    // printf("GETSON, i am %ld, and my son is %ld\n", actor_id_self(), (actor_id_t) data);

    fact_t *przekazywane = *stateptr;
    przekazywane->k++;
    przekazywane->k_fact *= przekazywane->k;

    message_t spawn_son = {
            .message_type = MSG_FACT,
            .data = przekazywane
    };

    send_message(data, spawn_son);
    message_t godie = {
            .message_type = MSG_GODIE,
    };
    send_message(actor_id_self(), godie);
}

void spawn(void **stateptr, size_t size, void *data) {
    fact_t *f = data;
    *stateptr = data;

    // printf("SPAWNING, i am %ld, and i am %ldth\n", actor_id_self(), f->k);
    if (f->k == f->stop) {
        printf("%llu\n", f->k_fact);
        message_t godie = {
                .message_type = MSG_GODIE,
        };
        send_message(actor_id_self(), godie);
    } else {
        message_t spawn = {
                .message_type = MSG_SPAWN,
                .data = &role
        };
        send_message(actor_id_self(), spawn);

    }
}

void hello_father(void **stateptr, size_t size, void *data) {

}

void hello(void **stateptr, size_t size, void *data) {
    // printf("HELLO, i am %ld, and my father is %ld\n", actor_id_self(), (actor_id_t) data);
    message_t get_son = {
            .message_type = MSG_SON,
            .data = actor_id_self()
    };
    send_message(data, get_son);
}

void calculate(void **stateptr, size_t size, void *data) {
    // printf("HELLO FATHER, i am %ld\n", actor_id_self());
    fact_t *fact_info = data;
    fact_info->k = 1;
    fact_info->k_fact = 1;
    *stateptr = fact_info;

    message_t fact = {
            .message_type = MSG_FACT,
            .data = fact_info
    };

    send_message(actor_id_self(), fact);
}


int main() {
    int n;
    scanf("%d", &n);
    printf("n factorial counting...\n");
    actor_id_t father;
    fact_t f;
    f.stop = n;
    actor_system_create(&father, &role_father);
    message_t message = {
            .data = &f,
            .message_type = 3,
    };
    send_message(father, message);
    actor_system_join(father);
    sleep(2);
}
