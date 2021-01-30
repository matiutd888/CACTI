#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include "../cacti.h"

#define NO_INCREMENTS 5

int x=1;

void hello(void** stateptr, size_t size, void* data) {
    assert(*stateptr == NULL);
    printf("hello, i am %ld, and my father is %ld\n", actor_id_self(), (actor_id_t)data);
    message_t msg = {
            .data = &x,
            .message_type = 1
    };
    int i;
    for(i=0;i<NO_INCREMENTS;i++) {
        printf("thread %ld, actor %ld: każę liczyć, i = %d\n", pthread_self(), actor_id_self(), i);
        send_message((actor_id_t)data, msg);
    }
    printf("thread %ld, actor %ld Skończyłem! i = %d\n", pthread_self(), actor_id_self(), i);
    //free(data);
    message_t gdmsg = {
            .message_type = MSG_GODIE
    };
    sleep(1);
    printf("HELLO: thread: %ld, actor: %ld\n", pthread_self(), actor_id_self());
    send_message(actor_id_self(),gdmsg);
}

void fun(void** stateptr, size_t size, void* data) {
    printf("%d: %ld is incrementing data: %d\n", pthread_self() % 100, actor_id_self(), ++*(int*)data);
    message_t gdmsg = {
            .message_type = MSG_GODIE
    };

    if(*(int*)data == 1+2*NO_INCREMENTS || *(int*)data == 1+4*NO_INCREMENTS) {
        printf("FUNL: thread: %ld, actor: %ld\n", pthread_self(), actor_id_self());
        send_message(actor_id_self(),gdmsg);
    }
}

int main(){
    const size_t nprompts = 2;
    void (**prompts)(void**, size_t, void*) = malloc(sizeof(void*) * nprompts);
    printf("MAIN THREAD ID %d\n", pthread_self() % 100);
    prompts[0] = &hello;
    prompts[1] = &fun;
    role_t role = {
            .nprompts = nprompts,
            .prompts = prompts
    };

    message_t msgSpawn = {
            .message_type = MSG_SPAWN,
            .data = &role
    };

    message_t msgGoDie = {
            .message_type = MSG_GODIE
    };


    actor_id_t actorId;
//    actor_system_create(&actorId, &role);
//
//    send_message(actorId, msgSpawn);
//    send_message(actorId, msgSpawn);
//  //  sleep(2);
//    send_message(0, msgGoDie);
//    actor_system_join(0);
//    actor_system_join(0);

    // sleep(3);
printf(" ========= ROUND 2 ========\n");
    actor_system_create(&actorId, &role);

    send_message(actorId, msgSpawn);
    send_message(actorId, msgSpawn);

    actor_system_join(0);
    actor_system_join(0);
   // sleep(3);
//printf(" ========= ROUND 3 ========\n");
//
//    if (actor_system_create(&actorId, &role) != 0) {
//        printf("Nie udało się stworzyć systemu aktorów!\n");
//    }
//
//    send_message(actorId, msgGoDie);
//    send_message(actorId, msgSpawn);
//    send_message(actorId, msgSpawn);
//    send_message(actorId, msgSpawn);
//    printf("Zaraz przechodzę przez JOINa\n");
//    actor_system_join(0);
//    printf("Przeszedłem przez JOINa\n");
//    actor_system_join(0);

    free(prompts);
    return 0;
}