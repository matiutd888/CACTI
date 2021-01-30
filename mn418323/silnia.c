#include <stdio.h>
#include "cacti.h"

#define UNUSED(x) (void)(x)

#define MSG_SON 1
#define MSG_FACT 2

typedef struct informacja {
    unsigned long long k_fact;
    int k;
    int stop;
} fact_t;

void save_col_and_spawn(void **stateptr, size_t size, void *data);

void hello_origin(void **stateptr, size_t size, void *data);

void hello(void **stateptr, size_t size, void *data);

void get_son_id(void **stateptr, size_t size, void *data);

act_t propmts[3] = {hello, get_son_id, save_col_and_spawn};
role_t role = {
        .prompts = propmts,
        .nprompts = 3
};

void calculate(void **stateptr, size_t size, void *data);
act_t prompts_origin[4] = {hello_origin, get_son_id, save_col_and_spawn, calculate};


role_t role_origin = {
        .prompts = prompts_origin,
        .nprompts = 4
};
void get_son_id(void **stateptr, size_t size, void *data) {
    UNUSED(size);
    fact_t *przekazywane = *stateptr;
    przekazywane->k++;
    przekazywane->k_fact *= przekazywane->k;

    message_t spawn_son = {
            .message_type = MSG_FACT,
            .data = przekazywane
    };

    send_message((actor_id_t) data, spawn_son);
    message_t godie = {
            .message_type = MSG_GODIE,
    };
    send_message(actor_id_self(), godie);
}

void save_col_and_spawn(void **stateptr, size_t size, void *data) {
    UNUSED(size);
    fact_t *f = data;
    *stateptr = data;
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

void hello_origin(void **stateptr, size_t size, void *data) {
    UNUSED(stateptr);
    UNUSED(size);
    UNUSED(data);
}

void hello(void **stateptr, size_t size, void *data) {
    UNUSED(size);
    UNUSED(stateptr);
    message_t get_son = {
            .message_type = MSG_SON,
            .data = actor_id_self()
    };
    send_message(data, get_son);
}

void calculate(void **stateptr, size_t size, void *data) {
    UNUSED(size);

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
    actor_id_t origin;
    fact_t f;
    f.stop = n;
    actor_system_create(&origin, &role_origin);
    message_t message = {
            .data = &f,
            .message_type = 3,
    };
    send_message(origin, message);
    actor_system_join(origin);
}
