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

static bool debug = false;

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


sigset_t block_mask; // TODO oddać maskę
struct sigaction action, old_action;

struct tpool {
    queue_t *q;
    pthread_cond_t work_cond;
    pthread_cond_t working_cond;
    size_t working_cnt;
    size_t thread_cnt;
    bool stop;
};


typedef struct tpool tpool_t;

static tpool_t *tpool;
static pthread_t threads[POOL_SIZE];
static pthread_cond_t join_cond;


void catch(int sig) {
    actors.signaled = true;
    printf("Otrzymano signal!\n");
    pthread_cond_broadcast(&(tpool->work_cond));
}

// Helper methods

static void clean_actors() {
    // printf("Destroying system, actors.count = %d, actors.dead\n", actors.count, actors.count_dead);
    for (size_t i = 0; i < actors.count; i++) {
        queue_destruct(actors.vec[i]->messages);
        // free(actors.vec[i]actor_id);
        free(actors.vec[i]);
    }
    free(actors.vec);
    actors.vec = NULL;
    actors.signaled = false;
}

static void destroy_system() {
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&(tpool->work_cond));
    pthread_cond_destroy(&(tpool->working_cond));

    clean_actors();

    // printf("QUEUE DESTRUCT: %d\n", queue_size(tpool->q));
    print_queue(tpool->q);
    queue_destruct(tpool->q);
    free(tpool);
    tpool = NULL;
}

static void tpool_execute_messages(actor_id_t id);

static actor_id_t tpool_id_get() {
    actor_id_t working_actor;
    if (queue_empty(tpool->q))
        return NO_ACTOR;
    working_actor = (actor_id_t) queue_pop(tpool->q);
    return working_actor;
}

static void tpool_destroy();

// Executes work in the treadpool thats pointed by arg.
static void *tpool_worker();

static bool tpool_add_notify() {
    if (tpool == NULL)
        return false;
    cond_broadcast(&(tpool->work_cond));
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

static tpool_t *tpool_create() {
    tpool_t *t;
    size_t i;
    t = NULL;
    t = safe_malloc(sizeof(tpool_t));
    if (t == NULL)
        return NULL;

    t->thread_cnt = POOL_SIZE;
    t->stop = false;
    t->working_cnt = 0;

    if (pthread_cond_init(&(t->work_cond), NULL) != 0)
        return NULL;
    if (pthread_cond_init(&(t->working_cond), NULL) != 0)
        return NULL;

    t->q = queue_init(INITIAL_ACTORS_SIZE);

    for (i = 0; i < POOL_SIZE; i++) {
        if (pthread_create(&(threads[i]), NULL, tpool_worker, t) != 0)
            exit(1);
    }

    return t;
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

    tpool = tpool_create();

    if (tpool == NULL) {
        exit(1);
    }

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

    // Wysyłanie message hello
    message_t hello = {
            .message_type = MSG_HELLO,
            .nbytes = 0,
            .data = NULL
    };
//    printf("Stworzym system, wysyłam hello!\n");
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
    // printf("Stworzyłęm aktora %d, wysyłam do niego hello\n", new_act->id);
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
        // printf("%d: KOŃCZĘ ŻYWOT %d, COUNT DEAD: %d\n", pthread_self() % 100, actor->id, actors.count_dead);

        unlock_mutex(&(actor->mutex));
        unlock_mutex(&mutex);
        return;
    }

    actor->role.prompts[msg->message_type](&(actor->state), msg->nbytes, msg->data);
}

static void tpool_execute_messages(actor_id_t id) {
    curr_id = id;
    lock_mutex(&mutex);
    if (actors.count <= id)
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
        queue_push(tpool->q, (void *) actor->id);
        tpool_add_notify();
    }
    actor->working = false;
    unlock_mutex(&actor->mutex);
    unlock_mutex(&mutex);
}

static void *tpool_worker() {
    sigaction(SIGINT, &action, 0);

    actor_id_t id = NO_ACTOR;
    while (1) {
        lock_mutex(&(mutex));

//        printf("%lu: Actors count =%zu, Actors dead =%zu, tm.queue.size() %zu\n",
//               pthread_self() % 100, actors.count, actors.count_dead, queue_size(tpool->q));

        while (queue_empty(tpool->q) && !tpool->stop) {
            if (queue_empty(tpool->q) && (actors.count_dead == actors.count || actors.signaled)) {
                //printf("%lu: Zauważyłem, że to koniec!\n", pthread_self() % 100);
                //printf("COUNT: %d COUNT DEAD: %d SIGNALED: %d\n", actors.count, actors.count_dead,actors.signaled);
                tpool->stop = true;
                tpool->thread_cnt--;
                tpool_destroy();
                return NULL;
            }
            cond_wait(&(tpool->work_cond), &(mutex));
        }

        if (tpool->stop) {
            break;
        }

        id = tpool_id_get();
        tpool->working_cnt++;
        unlock_mutex(&(mutex));

        if (id != NO_ACTOR) {
            if (debug) printf("%lu: Mam jakąś pracę! aktorID = %ld\n", pthread_self() % 100, id);
            tpool_execute_messages(id);
        }

        lock_mutex(&(mutex));
        tpool->working_cnt--;

        if (!tpool->stop && tpool->working_cnt == 0 && queue_empty(tpool->q)) // TODO o co w sumie z tym chodzi
            cond_signal(&(tpool->working_cond));
        unlock_mutex(&(mutex));
    }
    tpool->thread_cnt--;
    cond_signal(&(tpool->working_cond));
    unlock_mutex(&(mutex));
    // printf("%lu: Koniec procesu %zu\n", pthread_self() % 100, tpool->thread_cnt);
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
    if (actors.count <= id) {
        unlock_mutex(&mutex);
        return NO_ACTOR_OF_ID;
    }
    actor_t *act = actors.vec[id];
    lock_mutex(&(act->mutex));

//    if (id == 0) {
//        printf("Wysyłanie wiadomości do 0! QUEUE SIZE: %d, IS DEAD %d\n", queue_size(act->messages), act->is_dead);
//    }
    if (act->is_dead || actors.signaled || tpool->stop) {
        unlock_mutex(&(act->mutex));
        unlock_mutex(&(mutex));
        return ACTOR_IS_DEAD;
    }

    if (queue_size(act->messages) == ACTOR_QUEUE_LIMIT) {
        unlock_mutex(&(act->mutex));
        unlock_mutex(&(mutex));
        return QUEUE_LIMIT_REACHED;
    }

    if (act->role.nprompts <= message.message_type
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
            if (debug)
                printf("%lu : (1) Dodaję na kolejkę aktora %ld, który ma w tym momencie %zu wiadomości\n",
                       pthread_self() % 100,
                       id,
                       queue_size(act->messages));

            queue_push(tpool->q, act->id);
            tpool_add_notify();
        }
    }

    unlock_mutex(&(act->mutex));
    unlock_mutex(&(mutex));
    return 0;
}


// System destruction handling

static void tpool_wait() {
    if (tpool == NULL)
        return;

    lock_mutex(&(mutex));
    while (1) {
        if ((!tpool->stop && tpool->working_cnt != 0) || (tpool->stop && tpool->thread_cnt != 0)) {
            cond_wait(&(tpool->working_cond), &(mutex));
        } else {
            break;
        }
    }
    if (debug) printf("Koniec czekania na koniec!\n");
    unlock_mutex(&(mutex));

}

static void tpool_destroy() {
    // if (debug)
    // printf("%d: będę kończył program!\n", pthread_self() % 100);
    if (tpool == NULL)
        return;

    cond_broadcast(&(tpool->work_cond));
    unlock_mutex(&(mutex));

    tpool_wait();
    actors.dead = true;

    if (!joined) {
        destroy_system();
    } else {
        cond_broadcast(&(join_cond));
    }
     printf("Skończyłęm się!\n");
    pthread_exit(NULL);
}

static void join_threads() {
    if (!threads_joined)
        for (int i = 0; i < POOL_SIZE; ++i) {
            pthread_join(threads[i], NULL);
        }
    threads_joined = true;
}

void actor_system_join(actor_id_t actor) {
    if (actor >= actors.count)
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