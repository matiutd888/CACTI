#include "cacti.h"
#include "queue.h"
#include <stdio.h>
#include <pthread.h>
#include "err.h"
#include <stdbool.h>
#include <signal.h>

#define RESIZE_MULTPIER 2

#define INITIAL_ACTORS_SIZE 3
#define INITIAL_MESSAGES_QUEUE_SIZE ACTOR_QUEUE_LIMIT
#define ACTOR_SIZE sizeof(actor_t)

#define NO_ACTOR_OF_ID (-2)

#define ACTOR_IS_DEAD (-1)

#define QUEUE_LIMIT_REACHED (-3)

#define WRONG_TYPE (-4)

#define NO_ACTOR (-1)

_Thread_local actor_id_t curr_id;

static bool threads_joined = false;
static bool joined = false;

static void lock_mutex(pthread_mutex_t *mtx) {
    int result = pthread_mutex_lock(mtx);
    if (result != 0) {
        syserr(result, "Mutex lock error!");
    }
}

static void unlock_mutex(pthread_mutex_t *mtx) {
    int result = pthread_mutex_unlock(mtx);
    if (result != 0)
        syserr(result, "Mutex unlock error!");
}

static void cond_wait(pthread_cond_t *cond, pthread_mutex_t *mtx) {
    int result = pthread_cond_wait(cond, mtx);
    if (result != 0)
        syserr(result, "Condition wait error");
}

static void cond_broadcast(pthread_cond_t *cond) {
    int result = pthread_cond_broadcast(cond);
    if (result != 0)
        syserr(result, "Condition broadcast error");
}

static void cond_signal(pthread_cond_t *cond) {
    int result = pthread_cond_broadcast(cond);
    if (result != 0)
        syserr(result, "Condition signal error");
}

static void *safe_malloc(size_t size) {
    void *d = NULL;
    d = malloc(size);
    if (d == NULL)
        syserr(1, "Memory alloc failed!\n");
    return d;
}

typedef struct actor_struct {
    role_t role;
    actor_id_t id;
    bool is_dead;
    queue_t *messages;
    pthread_mutex_t mutex;
    bool has_messages;
    bool working;
    void *state;
} actor_t;

typedef struct actors_vec {
    actor_t **vec;
    size_t count;
    size_t size;
    size_t count_dead;
    bool dead;
    bool signaled;
} actor_vec_t;

static actor_vec_t actors = {
        .vec = NULL,
        .count = 0,
        .size = 0,
        .count_dead = 0,
        .dead = true,
        .signaled = false};

static pthread_mutex_t mutex;


sigset_t block_mask;
struct sigaction action, old_action;

struct t_pool {
    queue_t *q;
    pthread_cond_t work_cond;
    pthread_cond_t wait_for_end;
    size_t thread_count;
    bool stop;
};


typedef struct t_pool t_pool_t;

static t_pool_t t_pool;
static pthread_t threads[POOL_SIZE];
static pthread_cond_t join_cond;


void catch(int sig) {
    if (sig == SIGINT) {
        actors.signaled = true;
        pthread_cond_broadcast(&(t_pool.work_cond));
    }
}

// Helper methods

static void clean_actors() {
    for (size_t i = 0; i < actors.count; i++) {
        queue_destruct(actors.vec[i]->messages);
        free(actors.vec[i]);
    }
    free(actors.vec);
    actors.vec = NULL;
    actors.signaled = false;
}

static void destroy_system() {
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&(t_pool.work_cond));
    pthread_cond_destroy(&(t_pool.wait_for_end));

    clean_actors();

    queue_destruct(t_pool.q);
}

static void t_pool_execute_messages(actor_id_t id);

static void t_pool_destruct();

// Executes work in the treadpool thats pointed by arg.
static void *t_pool_search_for_work();

static bool t_pool_add_notify() {
    cond_broadcast(&(t_pool.work_cond));
    return true;
}

static void resize_actors() {
    actors.size = actors.size * RESIZE_MULTPIER + 1;
    actors.vec = realloc(actors.vec, sizeof(actor_t *) * actors.size);
    if (actors.vec == NULL)
        exit(1);

    for (size_t i = actors.count; i < actors.size; i++)
        actors.vec[i] = NULL;
}

static void add_actor(actor_t *actor) {
    if (actors.count == CAST_LIMIT) {
        fprintf(stderr, "Too many actors!");
        exit(1);
    }

    if (actors.count == actors.size)
        resize_actors();

    actors.vec[actors.count] = actor;
    actors.count++;
}

static actor_t *new_actor(role_t *role) {
    actor_id_t new_id = actors.count;
    actor_t *new_act = NULL;
    new_act = safe_malloc(ACTOR_SIZE);
    if (new_act == NULL)
        syserr(1, "Nie udało s zaalokować pamięci na aktora");

    new_act->is_dead = false;
    if (pthread_mutex_init(&(new_act->mutex), 0) != 0)
        syserr(1, "Nie udało się zainicjolować mutexa");


    new_act->id = new_id;
    new_act->messages = queue_init(INITIAL_MESSAGES_QUEUE_SIZE);
    new_act->has_messages = false;
    new_act->role = *role;
    new_act->state = NULL;
    new_act->working = false;
    return new_act;
}

actor_id_t actor_id_self() {
    return curr_id;
}

static void t_pool_init(t_pool_t *t) {
    t->thread_count = POOL_SIZE;
    t->stop = false;

    if (pthread_cond_init(&(t->work_cond), NULL) != 0)
        exit(1);
    if (pthread_cond_init(&(t->wait_for_end), NULL) != 0)
        exit(1);

    t->q = queue_init(INITIAL_ACTORS_SIZE);

    for (size_t i = 0; i < POOL_SIZE; i++) {
        if (pthread_create(&(threads[i]), NULL, t_pool_search_for_work, t) != 0)
            exit(1);
    }
}

int actor_system_create(actor_id_t *actor, role_t *const role) {
    if (!actors.dead || actors.vec != NULL)
        return -1;

    if (pthread_mutex_init(&mutex, 0) != 0)
        return -1;

    lock_mutex(&mutex);
    actors.vec = calloc(INITIAL_ACTORS_SIZE, sizeof(actor_t *));
    actors.size = INITIAL_ACTORS_SIZE;
    for (size_t i = 0; i < actors.size; i++)
        actors.vec[i] = NULL;

    threads_joined = false;
    joined = false;
    actors.count = 0;
    actors.dead = false;
    actors.signaled = false;
    actors.count_dead = 0;

    t_pool_init(&t_pool);

    actor_t *act = new_actor(role);
    add_actor(act);

    *actor = act->id;

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGINT);

    action.sa_flags = 0;
    action.sa_mask = block_mask;
    action.sa_handler = catch;
    sigaction(SIGINT, &action, &old_action);

    unlock_mutex(&mutex);

    message_t hello = {
            .message_type = MSG_HELLO,
            .nbytes = 0,
            .data = NULL
    };
    send_message(act->id, hello);
    return 0;
}


// Messages handling

static void run_spawn(actor_t *actor, message_t *msg) {
    if (actors.signaled)
        return;

    role_t *role = msg->data;

    lock_mutex(&mutex);
    actor_t *new_act = new_actor(role);
    add_actor(new_act);
    unlock_mutex(&mutex);

    message_t message;
    message.data = (void *) actor->id;
    message.message_type = MSG_HELLO;
    message.nbytes = sizeof(actor_id_t);
    send_message(new_act->id, message);
}

static void run_message(actor_t *actor, message_t *msg) {
    if (msg->message_type == MSG_SPAWN) {
        run_spawn(actor, msg);
        return;
    }
    if (msg->message_type == MSG_GODIE) {
        lock_mutex(&mutex);
        lock_mutex(&(actor->mutex));
        actor->is_dead = true;
        actors.count_dead++;

        unlock_mutex(&(actor->mutex));
        unlock_mutex(&mutex);
        return;
    }

    actor->role.prompts[msg->message_type](&(actor->state), msg->nbytes, msg->data);
}

// Execute messages of actor.
// Executes n messages, where n is equal to number of
// messages on actors queue just before processing the queue.
static void t_pool_execute_messages(actor_id_t id) {
    curr_id = id;
    lock_mutex(&mutex);
    if (id < 0 || actors.count <= (size_t) id)
        syserr(1, "Execute messages: wrong actor id\n");
    actor_t *actor = actors.vec[id];
    unlock_mutex(&mutex);
    lock_mutex(&(actor->mutex));

    actor->has_messages = false;
    actor->working = true;

    queue_t *message_q = actor->messages;
    size_t messages_count = queue_size(message_q);

    unlock_mutex(&(actor->mutex));

    while (messages_count > 0) {
        lock_mutex(&(actor->mutex));

        message_t *msg = queue_pop(message_q);

        unlock_mutex(&(actor->mutex));

        run_message(actor, msg);
        free(msg);
        messages_count--;
    }

    lock_mutex(&mutex);
    lock_mutex(&actor->mutex);
    if (actor->has_messages && !actor->is_dead) {
        queue_push(t_pool.q, (void *) actor->id);
        t_pool_add_notify();
    }
    actor->working = false;
    unlock_mutex(&actor->mutex);
    unlock_mutex(&mutex);
}

// In loop searches for work.
// Tries to take id from thread pools queue of ids.
// If sees,  that queue is empty and
// signal has occurred or all actors are dead executes queue destruct.
static void *t_pool_search_for_work() {
    sigaction(SIGINT, &action, 0);

    while (!t_pool.stop) {
        lock_mutex(&(mutex));

        while (queue_empty(t_pool.q) && !t_pool.stop) {
            if (queue_empty(t_pool.q) && (actors.count_dead == actors.count || actors.signaled)) {
                t_pool.stop = true;
                t_pool.thread_count--;
                t_pool_destruct();
                return NULL;
            }
            cond_wait(&(t_pool.work_cond), &(mutex));
        }

        if (t_pool.stop)
            break;

        actor_id_t id;
        if (queue_empty(t_pool.q))
            id = NO_ACTOR;
        else
            id = (actor_id_t) queue_pop(t_pool.q);

        unlock_mutex(&(mutex));

        if (id != NO_ACTOR)
            t_pool_execute_messages(id);

        lock_mutex(&(mutex));

        unlock_mutex(&(mutex));
    }
    t_pool.thread_count--;
    cond_signal(&(t_pool.wait_for_end));
    unlock_mutex(&(mutex));
    return NULL;
}

static message_t *create_message(message_t message) {
    message_t *ret = NULL;
    ret = safe_malloc(sizeof(message));
    if (ret == NULL)
        return ret;
    ret->nbytes = message.nbytes;
    ret->data = message.data;
    ret->message_type = message.message_type;
    return ret;
}

int send_message(actor_id_t id, message_t message) {
    lock_mutex(&mutex);
    if (id < 0 || actors.count <= (size_t) id) {
        unlock_mutex(&mutex);
        return NO_ACTOR_OF_ID;
    }
    actor_t *act = actors.vec[id];
    lock_mutex(&(act->mutex));

    if (act->is_dead || actors.signaled || t_pool.stop) {
        unlock_mutex(&(act->mutex));
        unlock_mutex(&(mutex));
        return ACTOR_IS_DEAD;
    }

    if (queue_size(act->messages) == ACTOR_QUEUE_LIMIT) {
        unlock_mutex(&(act->mutex));
        unlock_mutex(&(mutex));
        return QUEUE_LIMIT_REACHED;
    }

    if ((message.message_type < 0 ||
         act->role.nprompts <= (size_t) message.message_type)
        && message.message_type != MSG_GODIE
        && message.message_type != MSG_SPAWN) {
        unlock_mutex(&(act->mutex));
        unlock_mutex(&(mutex));
        return WRONG_TYPE;
    }

    message_t *m = create_message(message);
    queue_push(act->messages, m);


    if (!act->has_messages) {
        act->has_messages = true;
        if (!act->working) {
            queue_push(t_pool.q, (void *) act->id);
            t_pool_add_notify();
        }
    }

    unlock_mutex(&(act->mutex));
    unlock_mutex(&(mutex));
    return 0;
}


// System destruction handling

static void t_pool_destruct() {
    cond_broadcast(&(t_pool.work_cond));

    while (t_pool.thread_count != 0) {
        cond_wait(&(t_pool.wait_for_end), &(mutex));
    }
    unlock_mutex(&(mutex));

    actors.dead = true;

    if (!joined) {
        destroy_system();
    } else {
        cond_broadcast(&(join_cond));
    }
}

static void join_threads() {
    if (!threads_joined)
        for (int i = 0; i < POOL_SIZE; ++i) {
            pthread_join(threads[i], NULL);
        }
    threads_joined = true;
}

void actor_system_join(actor_id_t actor) {
    if (actor < 0 || actors.count <= (size_t) actor)
        syserr(1, "Brak aktora %ld w systemie!", actor);

    if (actors.dead) {
        join_threads();
        sigaction(SIGINT, &old_action, NULL);
        return;
    }
    lock_mutex(&mutex);
    joined = true;
    pthread_cond_init(&join_cond, NULL);
    while (!actors.dead) {
        cond_wait(&join_cond, &mutex);
    }
    join_threads();
    unlock_mutex(&mutex);
    destroy_system();
    sigaction(SIGINT, &old_action, NULL);
}