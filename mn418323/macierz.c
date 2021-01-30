#include "cacti.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define ACTOR_SIZE sizeof(actor)

#define MSG_SON 1
#define MSG_CALCULATE 3
#define MSG_SAVE_COLUMN_AND_SPAWN 2
#define MSG_CREATE_SYSTEM 5
#define MSG_CLEAN 4
#define MSG_CALCULATE_ALL 6

void hello_origin(void **stateptr, size_t size, void *data);

void get_son_id(void **stateptr, size_t size, void *data);

void save_col_and_spawn(void **stateptr, size_t size, void *data);

void hello(void **stateptr, size_t size, void *data);

void calculate(void **stateptr, size_t size, void *data);

void clean(void **stateptr, size_t size, void *data) {

}

void create_system(void **stateptr, size_t size, void *data);

void calculate_all(void **stateptr, size_t size, void *data);

act_t propmts[7] = {hello, get_son_id, save_col_and_spawn, calculate, clean, NULL, calculate_all};

role_t role = {
        .prompts = propmts,
        .nprompts = 7
};

act_t prompts_origin[7] = {hello_origin, get_son_id, save_col_and_spawn, calculate, clean, create_system,
                           calculate_all};

role_t role_father = {
        .prompts = prompts_origin,
        .nprompts = 7
};

typedef struct actor_info {
    actor_id_t next;
    int col;
    actor_id_t origin;
    int **macierz;
    int **milisec;
    int n;
    int k;
} actor;

typedef struct row_info {
    int row;
    int sum;
} row_t;


int **init_2s_array(int columns, int rows) {
    int **m = malloc(sizeof(int *) * columns);
    for (int i = 0; i < columns; i++)
        m[i] = malloc(sizeof(int) * rows);
    return m;
}

int **macierz;
int **milisec;

int k = 4, n = 3;

void hello(void **stateptr, size_t size, void *data) {
    printf("HELLO I am %ld and my father is %ld\n", actor_id_self(), data);
    *stateptr = malloc(ACTOR_SIZE);
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
    act->origin = father->origin;
    printf("SPAWN, i am %ld and i am %dth, my origin is %ld\n", actor_id_self(), act->col, act->origin);
    if (act->col < father->n - 1) {
        message_t spawn = {
                .message_type = MSG_SPAWN,
                .data = &role
        };
        send_message(actor_id_self(), spawn);
    } else {
        printf("Wywołuję calculate all!\n");
        message_t calc_all = {
                .message_type = MSG_CALCULATE_ALL
        };
        send_message(act->origin, calc_all);
    }
}

void create_system(void **stateptr, size_t size, void *data) {
    printf("DUPA\n");
    *stateptr = malloc(ACTOR_SIZE);
    actor *init = data;
    actor *act = *stateptr;
    act->macierz = init->macierz;
    act->milisec = init->milisec;
    act->n = init->n;
    act->k = init->k;
    act->origin = init->origin;
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
    r->sum += act->macierz[act->col][r->row];
    printf("Jestem kolumna %d obliczam wiersz %d, którego suma wynosi %d\n", act->col, r->row, r->sum);
    sleep(act->milisec[act->col][r->row]);
    if (act->col + 1 < act->n) {
        message_t calculate = {
                .data = r,
                .message_type = MSG_CALCULATE
        };
        actor_id_t next = act->next;
        send_message(next, calculate);
        if (r->row == act->k - 1) {
            free(*stateptr);
            message_t godie = {
                    .message_type = MSG_GODIE,
            };
            send_message(actor_id_self(), godie);
        }
    } else {
        printf("KONIEC WIERSZA %d, suma = %d\n", r->row, r->sum);
        if (r->row == act->k - 1) {
            free(*stateptr);
            message_t godie = {
                    .message_type = MSG_GODIE,
            };
            send_message(actor_id_self(), godie);
        }
        free(r);
    }
}

void calculate_all(void **statpeptr, size_t size, void *data) {
    printf("Wywołano calculate all!\n");
    actor *act = *statpeptr;
    for (int i = 0; i < act->k; ++i) {
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
    scanf("%d", &k);
    scanf("%d", &n);
    printf("%d %d\n", k, n);
    macierz = init_2s_array(n, k);
    milisec = init_2s_array(n, k);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < k; j++) {
            scanf("%d", &(macierz[i][j]));
            scanf("%d", &(milisec[i][j]));
        }

    for (int i = 0; i < k; ++i) {
        for (int j = 0; j < n; ++j) {
            printf("%d ", macierz[j][i]);
        }
        printf("\n");
    }

    // System wywołany
//
    actor_id_t origin;
    actor_system_create(&origin, &role_father);
    actor init = {
            .macierz = macierz,
            .milisec = milisec,
            .n = n,
            .k = k,
            .origin = origin
    };
    message_t msg = {
            .message_type = MSG_CREATE_SYSTEM,
            .data = &init
    };

    send_message(origin, msg);
    actor_system_join(origin);
    for (int i = 0; i < n; ++i) {
        free(macierz[i]);
        free(milisec[i]);
    }
    free(macierz);
    free(milisec);
}
