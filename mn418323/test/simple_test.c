//
// Created by mateusz on 28.01.2021.
//

#include "../cacti.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

void hello(void **stateptr, size_t size, void *data) {
//    assert(*stateptr == NULL);
//    printf("hello, i am %ld, and my father is %ld\n", ++, (actor_id_t)data);
//    message_t msg = {
//            .data = &x,
//            .message_type = 1
//    };
//    for(int i=0;i<NO_INCREMENTS;i++){
//        send_message((actor_id_t)data, msg);
//    }
//    //free(data);
//    message_t gdmsg = {
//            .message_type = MSG_GODIE
//    };
//    sleep(1);
//    printf("thread: %ld, actor: %ld\n", pthread_self(), actor_id_self());
//    send_message(actor_id_self(),gdmsg);
}

void fun(void **stateptr, size_t size, void *data) {
    printf("Wykonuję funkcję! %d\n", *(int *) data);
}

int main() {
    const size_t nprompts = 2;
    void (**prompts)(void **, size_t, void *) = malloc(sizeof(void *) * nprompts);
    prompts[0] = &hello;
    prompts[1] = &fun;
    role_t role = {
            .nprompts = nprompts,
            .prompts = prompts
    };

    int x = 12;
    message_t message = {
            .data = &x,
            .message_type = 1,
            .nbytes = sizeof(x)
    };

    actor_id_t actorId;
    if (actor_system_create(&actorId, &role) == 0) {
        printf("Pomyślnie stworzono system!\n");
    }
    send_message(actorId, message);
    message_t godie = {
            .data = NULL,
            .message_type = MSG_GODIE,
            .nbytes = 0
    };

    send_message(actorId, godie);
    actor_system_join(0);

    sleep(5);
    free(prompts);
}