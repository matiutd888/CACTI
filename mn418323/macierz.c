#include "cacti.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define MSG_SON 1
#define MSG_CALCULATE 3
#define MSG_SAVE_COLUMN_AND_SPAWN 2
#define MSG_CREATE_SYSTEM 5
#define MSG_CLEAN 4
void hello_origin(void **stateptr, size_t size, void *data);
void get_son_id(void **stateptr, size_t size, void *data);

void save_col_and_spawn(void **stateptr, size_t size, void *data);
void hello(void **stateptr, size_t size, void *data);
void calculate(void **stateptr, size_t size, void *data);
void clean(void **stateptr, size_t size, void *data) {

}

void create_system(void **stateptr, size_t size, void *data);

act_t propmts[5] = {hello, get_son_id, save_col_and_spawn, calculate, clean};

role_t role = {
        .prompts = propmts,
        .nprompts = 3
};

act_t prompts_origin[6] = {hello_origin, get_son_id, save_col_and_spawn, calculate, clean, create_system};

role_t role_father = {
        .prompts = prompts_origin,
        .nprompts = 4
};

typedef struct actor_info {
    actor_id_t next;
    int col;
    actor_id_t prev;
    int **macierz;
    int **milisec;
    int n;
    int k;
} actor;

typedef struct row_info {
    int row;
    int sum;
} row_t;

#define ACTOR_SIZE sizeof(actor)

int **init_2s_array(int columns, int rows) {
    int **m = malloc(sizeof(int *) * columns);
    for (int i = 0; i < columns; i++)
        m[i] = malloc(sizeof(int) * rows);
    return m;
}

int **macierz;
int **milisec;


const int k = 4, n = 3;

void hello(void **stateptr, size_t size, void *data) {
    printf("HELLO I am %ld and my father is %ld\n", actor_id_self(), data);
    *stateptr = malloc(ACTOR_SIZE);
    actor *act = *stateptr;
    act->prev = data;
    message_t get_son = {
            .message_type = MSG_SON,
            .data = actor_id_self()
    };
    send_message(data, get_son);
}

void hello_origin(void **stateptr, size_t size, void *data) {

}

void get_son_id(void **stateptr, size_t size, void *data) {
    actor *act = *stateptr;
    act->next = data;
    printf("SON I am %ld and my son is %ld\n", actor_id_self(), act->next);
    message_t spawn_son = {
            .message_type = MSG_SAVE_COLUMN_AND_SPAWN,
            .data = *stateptr
    };
    send_message(data, spawn_son);

    message_t godie = {
            .message_type = MSG_GODIE,
    };
    send_message(actor_id_self(), godie);
}

// Dostaje numer kolumny w data
void save_col_and_spawn(void **stateptr, size_t size, void *data) {
    actor *act = *stateptr;
    actor *father = data;
    act->col = father->col + 1;
    act->milisec = father->milisec;
    act->macierz = father->macierz;
    act->n = father->n;
    act->k = father->k;
    printf("SPAWN, i am %ld and i am %dth, my father is %ld\n", actor_id_self(), act->col, act->prev);
     if (act->col < father->k - 1) {
        message_t spawn = {
                .message_type = MSG_SPAWN,
                .data = &role
        };
        send_message(actor_id_self(), spawn);
    }
}

void create_system(void **stateptr, size_t size, void *data) {
    *stateptr = malloc(ACTOR_SIZE);
    actor *init = data;
    actor *act = *stateptr;
    act->macierz = init->macierz;
    act->milisec = init->milisec;
    act->n = init->n;
    act->k = init->k;
    init->col = -1;
    message_t spawn_son = {
            .message_type = MSG_SAVE_COLUMN_AND_SPAWN,
            .data = init
    };
    send_message(actor_id_self(), spawn_son);
}

void calculate(void **stateptr, size_t size, void *data) {
    row_t *r = data;
    actor *act = *stateptr;
    printf("Jestem kolumna %d obliczam wiersz %d, którego suma wynosi %d\n", act->col, r->row, r->sum);
    if (act->col + 1 < k) {
        message_t calculate = {
                .data = r,
                .message_type = MSG_CALCULATE
        };
        send_message(act->next, calculate);
    } else {
        printf("KONIEC WIERSZA %d\n", r->row);
    }
}

void calculate_all(void **statpeptr, size_t size, void *data) {
    for (int i = 0; i < n; ++i) {
        row_t *row = malloc(sizeof(row_t));
        row->row = i;
        row->sum = 0;
        message_t calculate = {
                .message_type = MSG_CALCULATE,
                .data = row
        };
        send_message(actor_id_self(), calculate);
    }
}

int main() {
    actor_id_t origin;
    actor_system_create(&origin, &role_father);
    actor init = {
        .macierz = macierz,
        .milisec = milisec,
        .n = n,
        .k = k
    };
    message_t msg = {
        .message_type = MSG_CREATE_SYSTEM,
        .data = &init
    };

    send_message(origin, msg);
    actor_system_join(origin);
//    scanf("%d", &k);
//    scanf("%d", &n);


//    macierz = init_2s_array(k, n);
//    milisec = init_2s_array(k, n);
//    for (int i = 0; i < k; i++)
//        for (int j = 0; j < n; j++) {
//            scanf("%d", &(macierz[i][j]));
//            scanf("%d", &(milisec[i][j]));
//        }
//
//    actor_by_column = malloc(k * sizeof (actor_id_t));
//
//    for (int i = 0; i < k; ++i) {
//        for (int j = 0; j < n; ++j) {
//            printf("%d ", macierz[i][j]);
//        }
//        printf("\n");
//    }
}
