#include "cacti.h"
#include "queue.h"
#include <stdio.h>
#include <pthread.h>
#include "err.h"
#include <stdbool.h>
#include <signal.h>

#define RESIZE_MULTPIER 2
#define INITIAL_ACTORS_SIZE 1000
#define INITIAL_MESSAGES_QUEUE_SIZE 1024
#define ACTOR_SIZE sizeof(actor_t)

#define NO_ACTOR_OF_ID -2

#define ACTOR_IS_DEAD -1

typedef struct actor_struct {
    role_t role;
    actor_id_t actor_id;
    bool is_dead;
    queue_t *messages;
    pthread_mutex_t mutex;
    bool has_messages;
    bool working;
    void *state;
} actor_t;

void terminate() {
    exit(1);
}

typedef struct actors_vec {
    actor_t **vec;
    size_t count;
    size_t size;
    size_t count_dead;
    bool dead;
    bool signaled;
} actor_vec_t;

actor_vec_t actors;
pthread_mutex_t mutex;

struct sigaction action;
sigset_t block_mask;

struct tpool {
    queue_t *q;
    // pthread_mutex_t work_mutex; // mutex
    pthread_cond_t work_cond;
    pthread_cond_t working_cond;
    size_t working_cnt;
    size_t thread_cnt; // How many threads are we using
    bool stop;
};


typedef struct tpool tpool_t;

tpool_t *tm;

//--------------------------------------------------------------

pthread_cond_t join_cond;

static void destroy_system() {
    for (size_t i = 0; i < actors.count; i++) {
        queue_destruct(actors.vec[i]->messages);

        if (actors.vec[i]->state != NULL)
            free(actors.vec[i]->state);

        free(actors.vec[i]);
    }
    free(actors.vec);
}

void tpool_execute_messages(actor_id_t id);

static actor_id_t *tpool_id_get() {
    actor_id_t *working_actor;
    working_actor = queue_pop(tm->q);
    return working_actor;
}

void tpool_wait() {
    if (tm == NULL)
        return;

    pthread_mutex_lock(&(mutex));
    while (1) {
        if ((!tm->stop && tm->working_cnt != 0) || (tm->stop && tm->thread_cnt != 0)) {
            pthread_cond_wait(&(tm->working_cond), &(mutex));
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&(mutex));

}

void tpool_destroy() {
    if (tm == NULL)
        return;

    pthread_cond_broadcast(&(tm->work_cond));
    pthread_mutex_unlock(&(mutex));

    tpool_wait(tm);

    pthread_cond_broadcast(&(join_cond));

    pthread_mutex_lock(&mutex);
    pthread_mutex_unlock(&mutex);

    pthread_cond_destroy(&join_cond);

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&(tm->work_cond));
    pthread_cond_destroy(&(tm->working_cond));

    destroy_system();
    queue_destruct(tm->q);
    actors.dead = true;
    free(tm);
}

// Executes work in the treadpool thats pointed by arg.
static void *tpool_worker() {
    actor_id_t *id;

    while (1) {
        pthread_mutex_lock(&(mutex));

        if (queue_empty(tm->q) && (actors.count_dead == actors.count || actors.signaled)) {
            tm->stop = true; // TODO obsługa GODIE
            tm->thread_cnt--;
            tpool_destroy(); // TODO dealloc actors;
            return NULL;
        }

        while (queue_empty(tm->q) && !tm->stop)
            pthread_cond_wait(&(tm->work_cond), &(mutex));

        if (tm->stop)
            break;

        id = tpool_id_get();
        tm->working_cnt++;
        pthread_mutex_unlock(&(mutex));

        if (id != NULL) {
            tpool_execute_messages(*id);
            free(id);
        }

        pthread_mutex_lock(&(mutex));
        tm->working_cnt--;

        if (!tm->stop && tm->working_cnt == 0 && queue_empty(tm->q)) // TODO o co w sumie z tym chodzi
            pthread_cond_signal(&(tm->working_cond));
        pthread_mutex_unlock(&(mutex));
    }
    tm->thread_cnt--;
    pthread_cond_signal(&(tm->working_cond));
    pthread_mutex_unlock(&(mutex));
    return NULL;
}

void actor_system_join(actor_id_t actor) {
    pthread_mutex_lock(&mutex);
    while (!actors.dead) {
        pthread_cond_wait(&join_cond, &mutex);
    }
}

tpool_t *tpool_create(size_t num) {
    tpool_t *t;
    pthread_t thread;
    size_t i;

    if (num == 0)
        num = 2;
    t = NULL;
    t = calloc(1, sizeof(*t));
    if (t == NULL)
        return NULL;

    t->thread_cnt = num;
    t->stop = false;
    t->working_cnt = 0;

    if (pthread_cond_init(&(t->work_cond), NULL) != 0)
        return NULL;
    if (pthread_cond_init(&(t->working_cond), NULL) != 0)
        return NULL;

    t->q = queue_init(INITIAL_ACTORS_SIZE);

    for (i = 0; i < num; i++) {
        if (pthread_create(&thread, NULL, tpool_worker, t) != 0)
            exit(1);
        pthread_detach(thread);
    }

    return t;
}

bool tpool_add_notify() {
    if (tm == NULL)
        return false;
    pthread_cond_broadcast(&(tm->work_cond));
    return true;
}

//---------------------------------------------------------------

void resize_actors() {
    actors.size = actors.size * RESIZE_MULTPIER + 1;
    actors.vec = realloc(actors.vec, sizeof(actor_t *) * actors.size);
    if (actors.vec == NULL)
        terminate();

    for (size_t i = actors.count; i < actors.size; i++)
        actors.vec = NULL;
}


// Weź id aktora z kolejki aktorow
// wywolaj tpool_execute_messages(actor_id_q.pop())
static void *process_main_queue();

void add_actor(actor_t *actor) {
    if (actors.count == CAST_LIMIT) {
        fprintf(stderr, "Too many actors!");
        terminate();
    }

    if (actors.count == actors.size)
        resize_actors();
    actors.count_dead = 0;
    actors.vec[actors.count] = actor;
    actors.count++;
}

actor_t *new_actor(role_t *role) {
    actor_id_t new_id = actors.count;
    actor_t *new_act = NULL;
    new_act = malloc(ACTOR_SIZE);
    if (new_act == NULL)
        syserr(1, "Nie udało s zaalokować pamięci na aktora");

    new_act->is_dead = false;
    new_act->mutex;
    if (pthread_mutex_init(&(new_act->mutex), 0) != 0)
        syserr(1, "Nie udało się zainicjolować mutexa");

    new_act->actor_id = new_id;
    new_act->messages = queue_init(INITIAL_MESSAGES_QUEUE_SIZE);
    new_act->has_messages = false;
    new_act->role = *role;
    new_act->state = NULL;
    new_act->working = false;
    return new_act;
}

void catch(int sig) {
    if (sig != SIGINT)
        syserr(0, "Błąd obsługi sygnałów");
    pthread_mutex_lock(&mutex);
    actors.signaled = true;
    pthread_mutex_unlock(&mutex);
}

int actor_system_create(actor_id_t *actor, role_t *const role) {
    if (!actors.dead)
        return -1;

    if (pthread_mutex_init(&mutex, 0) != 0)
        return -1;
    if (pthread_cond_init(&join_cond, NULL) != 0)
        return -1;

    pthread_mutex_lock(&mutex);
    actors.vec = calloc(INITIAL_ACTORS_SIZE, sizeof(actor_t *));
    actors.size = INITIAL_ACTORS_SIZE;
    for (size_t i = 0; i < actors.size; i++) {
        actors.vec[i] = NULL;
    }

    actors.count = 0;
    actors.dead = false;
    actors.signaled = false;

    tm = tpool_create(POOL_SIZE);

    if (tm == NULL) {
        terminate();
    }

    actor_t *act = new_actor(role);
    add_actor(act);


    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGINT);
    action.sa_flags = 0;
    action.sa_mask = block_mask;
    action.sa_handler = catch;

    pthread_mutex_unlock(&mutex);
    // TODO ogarnać
}

void run_spawn(actor_t *actor, message_t *msg) {
    role_t *role = msg->data;

    pthread_mutex_lock(&mutex);
    actor_t *new_act = new_actor(role);
    add_actor(new_act);
    pthread_mutex_unlock(&mutex);

    message_t message;
    message.data = (void *) (actor->actor_id);
    message.message_type = MSG_HELLO;
    message.nbytes = sizeof(actor->actor_id);
    send_message(new_act->actor_id, message);
}

void run_message(actor_t *actor, message_t *msg) {
    // TODO check msg type
    if (msg->message_type == MSG_SPAWN) {
        run_spawn(actor, msg);
        return;
    }
    if (msg->message_type == MSG_GODIE) {
        pthread_mutex_lock(&mutex);
        pthread_mutex_lock(&(actor->mutex));
        actor->is_dead = true;
        actors.count_dead++;
        pthread_mutex_unlock(&(actor->mutex));
        pthread_mutex_unlock(&mutex);
        return;
    }

    actor->role.prompts[msg->message_type](&(actor->state), msg->nbytes, msg->data);
    // TODO czy message mogę zwolnić w tym miejscu
}

void free_msg(message_t *msg) {
    // free(msg->data); TODO czy to zwalniać?
    free(msg);
}

void tpool_execute_messages(actor_id_t id) {
    if (id >= actors.count)
        syserr(1, "Execute messages: wrong actor id\n");
    actor_t *actor = actors.vec[id];

    // TODO Lub zwykły mutex
    if (pthread_mutex_lock(&(actor->mutex)) != 0)
        syserr(1, "Execute messages: nie udało się zaalokować mutexa!");

    actor->has_messages = false;
    actor->working = true;

    queue_t *message_q = actor->messages;
    size_t messages_count = queue_size(message_q);
    if (pthread_mutex_unlock(&(actor->mutex)) != 0)
        syserr(1, "Execute messages: nie udało się zwolnić mutexa!");

    while (messages_count > 0) {
        // Wzięcie se mutexa
        if (pthread_mutex_lock(&(actor->mutex)) != 0)
            syserr(1, "Execute messages: nie udało się zaalokować mutexa!");

        message_t *msg = queue_pop(message_q);
        //Zwolnienie mutexa
        if (pthread_mutex_unlock(&(actor->mutex)) != 0)
            syserr(1, "Nie udało się zwolnić mute!xa");
        run_message(actor, msg);
        free_msg(msg);
        messages_count--;
    }

    pthread_mutex_lock(&mutex);
    pthread_mutex_lock(&actor->mutex);
    if (actor->has_messages) {
        actor_id_t *work_id = malloc(sizeof(actor_id_t));
        *work_id = id;
        queue_push(tm->q, work_id);
        tpool_add_notify();
    }
    actor->working = false;
    pthread_mutex_unlock(&actor->mutex);
    pthread_mutex_unlock(&mutex);
}

message_t *create_message(message_t message) {
    message_t *ret = NULL;
    ret = malloc(sizeof(message));
    if (ret == NULL)
        return ret;
    ret->nbytes = message.nbytes;
    ret->data = message.data;
    ret->message_type = message.message_type;
    return ret;
}

// Jeżeli aktora nie ma na kolejce to dodaj id aktora na acotr_id_q
// Obsłuż przypadki msg_spawn i msg_godie
int send_message(actor_id_t id, message_t message) {
    actor_id_t *work_id = NULL;
    pthread_mutex_lock(&mutex);
    if (actors.count <= id) {
        pthread_mutex_unlock(&mutex);
        return NO_ACTOR_OF_ID;
    }

    actor_t *act = actors.vec[id];
    pthread_mutex_lock(&(act->mutex));

    if (act->is_dead) {
        pthread_mutex_unlock(&(act->mutex));
        pthread_mutex_unlock(&(mutex));
        return ACTOR_IS_DEAD;
    }

    if (queue_size(act->messages) == ACTOR_QUEUE_LIMIT) {
        pthread_mutex_unlock(&(act->mutex));
        pthread_mutex_unlock(&(mutex));
        return -3;
    }
    // TODO zdefiniować stałą.

    if (act->role.nprompts <= message.message_type
        && message.message_type != MSG_GODIE
        && message.message_type != MSG_SPAWN) {
        pthread_mutex_unlock(&(act->mutex));
        pthread_mutex_unlock(&(mutex));
        return -4; // TODO zdefiniować stałą
    }

    message_t *m = create_message(message);
    queue_push(act->messages, m);
    if (!act->has_messages) {
        act->has_messages = true;
        if (!act->working) {
            work_id = malloc(sizeof(actor_id_t));
            *work_id = id;
            queue_push(tm->q, work_id);
            tpool_add_notify();
        }
    }

    pthread_mutex_unlock(&(act->mutex));
    pthread_mutex_unlock(&(mutex));
    return 0;
}