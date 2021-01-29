#include "cacti.h"
#include "queue.h"
#include <stdio.h>
#include <pthread.h>
#include "err.h"
#include <stdbool.h>
#include <signal.h>
#include <threads.h>

#define RESIZE_MULTPIER 2
#define INITIAL_ACTORS_SIZE 1000
#define INITIAL_MESSAGES_QUEUE_SIZE 1024
#define ACTOR_SIZE sizeof(actor_t)

#define NO_ACTOR_OF_ID (-2)

#define ACTOR_IS_DEAD (-1)

bool debug = false;

thread_local actor_id_t curr_id;

bool joined = false;

void komunikat(char *s) {
    if (debug) printf("%d %s\n", pthread_self() % 100, s);
}

void lock_mutex(pthread_mutex_t *mtx) {
    int result = pthread_mutex_lock(mtx);
    if (result != 0) {
        fprintf(stderr, "Błąd!  %d\n", pthread_self() % 100);
        syserr(result, "Mutex lock error!");
    }
}

void unlock_mutex(pthread_mutex_t *mtx) {
    int result = pthread_mutex_unlock(mtx);
    if (result != 0)
        syserr(result, "Mutex unlock error!");
}

void cond_wait(pthread_cond_t *cond, pthread_mutex_t *mtx) {
    int result = pthread_cond_wait(cond, mtx);
    if (result != 0)
        syserr(result, "Condition wait error");
}

void cond_broadcast(pthread_cond_t *cond) {
    int result = pthread_cond_broadcast(cond);
    if (result != 0)
        syserr(result, "Condition broadcast error");
}

void cond_signal(pthread_cond_t *cond) {
    int result = pthread_cond_broadcast(cond);
    if (result != 0)
        syserr(result, "Condition signal error");
}

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

actor_vec_t actors = {
        .vec = NULL,
        .count = 0,
        .size = 0,
        .count_dead = 0,
        .dead = true,
        .signaled = false};

pthread_mutex_t mutex;
pthread_mutex_t join_mutex;

// struct sigaction action;
// sigset_t block_mask;

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
    printf("Destroying system, actors.count = %d, actors.dead\n", actors.count, actors.count_dead);
    for (size_t i = 0; i < actors.count; i++) {
        queue_destruct(actors.vec[i]->messages);

        if (actors.vec[i]->state != NULL)
            free(actors.vec[i]->state);
        free(actors.vec[i]);
    }
    free(actors.vec);
    actors.vec = NULL;
}

void tpool_execute_messages(actor_id_t id);

static actor_id_t *tpool_id_get() {
    actor_id_t *working_actor;
    working_actor = queue_pop(tm->q);
    if (debug) printf("%lu Zdjąłem ID z kolejki! %d\n", pthread_self(), *working_actor);
    return working_actor;
}

void tpool_wait() {
    if (tm == NULL)
        return;

    lock_mutex(&(mutex));
    while (1) {
        if ((!tm->stop && tm->working_cnt != 0) || (tm->stop && tm->thread_cnt != 0)) {
            cond_wait(&(tm->working_cond), &(mutex));
        } else {
            break;
        }
    }
    if (debug) printf("Koniec czekania na koniec!\n");
    unlock_mutex(&(mutex));

}


void tpool_destroy() {
    // if (debug)
    printf("%d: będę kończył program!\n", pthread_self() % 100);
    if (tm == NULL)
        return;

    cond_broadcast(&(tm->work_cond));
    unlock_mutex(&(mutex));
    // printf("%d: destroy mutex unlock 1\n", pthread_self() % 100);

    tpool_wait();
    // printf("Koniec czekania!\n");
    actors.dead = true;

    pthread_cond_destroy(&(tm->work_cond));
    pthread_cond_destroy(&(tm->working_cond));


    // printf("%d: Destroying\n", pthread_self() % 100);
    pthread_mutex_destroy(&mutex);

    destroy_system();
    queue_destruct(tm->q);
    free(tm);

    if (joined) {
//        printf("Budzę wątek główny!\n");
        cond_broadcast(&(join_cond));
    }
    if (debug) printf("%d: Koniec kończącego procesu!\n", pthread_self() % 100);
}

void actor_system_join(actor_id_t actor) {
    // exit(1);
    printf("JOIN WYWOLANE: %d\n", pthread_self());
    if (actors.dead)
        return;

    // printf("Próbuję wziąć zwykły mutex 1\n");
    lock_mutex(&mutex);
    joined = true;
    pthread_cond_init(&join_cond, NULL);
    pthread_mutex_init(&join_mutex, 0);
    // printf("Próbuję wziąć join mutex 1\n");
    unlock_mutex(&mutex);
    lock_mutex(&join_mutex);
    while (!actors.dead) {
        cond_wait(&join_cond, &join_mutex);
    }
    // printf("%d: join, mutex unlock 1\n", pthread_self() % 100);
    pthread_cond_destroy(&join_cond);
    komunikat("koniec czekania na wątki");
    unlock_mutex(&join_mutex);
    pthread_mutex_destroy(&join_mutex);

}

// Executes work in the treadpool thats pointed by arg.
static void *tpool_worker() {
    actor_id_t *id;

    while (1) {
        lock_mutex(&(mutex));
        if (debug)
            printf("%d: Actors count = %d, Actors dead == %d, tm.queue.size() = %d\n",
                   pthread_self() % 100, actors.count, actors.count_dead, queue_size(tm->q));

        while (queue_empty(tm->q) && !tm->stop) {
            if (queue_empty(tm->q) && (actors.count_dead == actors.count || actors.signaled)) {
                if (debug) printf("%d: Zauważyłem, że to koniec!\n", pthread_self() % 100);
                tm->stop = true; // TODO obsługa GODIE
                tm->thread_cnt--;
                tpool_destroy(); // TODO dealloc actors;
                return NULL;
            }
            cond_wait(&(tm->work_cond), &(mutex));
        }

        if (tm->stop) {
            break;
        }

        id = tpool_id_get();
        tm->working_cnt++;
        unlock_mutex(&(mutex));

        if (id != NULL) {
            if (debug) printf("%d: Mam jakąś pracę! aktorID = %d\n", pthread_self() % 100, *id);
            tpool_execute_messages(*id);
            free(id);
        }

        lock_mutex(&(mutex));
        tm->working_cnt--;

        if (!tm->stop && tm->working_cnt == 0 && queue_empty(tm->q)) // TODO o co w sumie z tym chodzi
            cond_signal(&(tm->working_cond));
        unlock_mutex(&(mutex));
    }
    tm->thread_cnt--;
    cond_signal(&(tm->working_cond));
    unlock_mutex(&(mutex));
    if (debug) printf("%d: Koniec procesu %d\n", pthread_self() % 100, tm->thread_cnt);
    return NULL;
}

actor_id_t actor_id_self() {
    return curr_id;
}

tpool_t *tpool_create(size_t num) {

    tpool_t *t;

    size_t i;

    if (num == 0)
        num = 2;

    t = NULL;
    t = malloc(sizeof(tpool_t));
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
        pthread_t thread;
        if (pthread_create(&thread, NULL, tpool_worker, t) != 0)
            exit(1);
        pthread_detach(thread);
    }

    return t;
}

bool tpool_add_notify() {
    if (debug) printf("Budzę kogoś, q.size() %d\n", queue_size(tm->q));
    if (tm == NULL)
        return false;
    cond_broadcast(&(tm->work_cond));
    return true;
}

//---------------------------------------------------------------

void resize_actors() {
    actors.size = actors.size * RESIZE_MULTPIER + 1;
    actors.vec = realloc(actors.vec, sizeof(actor_t *) * actors.size);
    if (actors.vec == NULL)
        terminate();

    for (size_t i = actors.count; i < actors.size; i++)
        actors.vec[i] = NULL;
}

void add_actor(actor_t *actor) {
    if (actors.count == CAST_LIMIT) {
        fprintf(stderr, "Too many actors!");
        terminate();
    }

    if (actors.count == actors.size)
        resize_actors();

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

//void catch(int sig) {
//    printf("Otrzymano signal!\n");
//    if (sig != SIGINT)
//        syserr(0, "Błąd obsługi sygnałów");
//    lock_mutex(&mutex);
//    actors.signaled = true;
//    unlock_mutex(&mutex);
//}

int actor_system_create(actor_id_t *actor, role_t *const role) {
    if (!actors.dead || actors.vec != NULL)
        return -1;

    if (pthread_mutex_init(&mutex, 0) != 0)
        return -1;

    lock_mutex(&mutex);
    actors.vec = calloc(INITIAL_ACTORS_SIZE, sizeof(actor_t *));
    actors.size = INITIAL_ACTORS_SIZE;
    for (size_t i = 0; i < actors.size; i++) {
        actors.vec[i] = NULL;
    }

    joined = false;
    actors.count = 0;
    actors.dead = false;
    actors.signaled = false;
    actors.count_dead = 0;

    tm = tpool_create(POOL_SIZE);

    if (tm == NULL) {
        terminate();
    }

    actor_t *act = new_actor(role);
    add_actor(act);

    *actor = act->actor_id;

//    sigemptyset(&block_mask);
//    sigaddset(&block_mask, SIGINT);
//    action.sa_flags = 0;
//    action.sa_mask = block_mask;
//    action.sa_handler = catch;

    unlock_mutex(&mutex);

    // Wysyłanie message hello
    message_t hello = {
            .message_type = MSG_HELLO,
            .nbytes = 0,
            .data = NULL
    };
    send_message(act->actor_id, hello);

    return 0;
}

void run_spawn(actor_t *actor, message_t *msg) {
    role_t *role = msg->data;

    lock_mutex(&mutex);
    actor_t *new_act = new_actor(role);
    add_actor(new_act);
    unlock_mutex(&mutex);

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
        lock_mutex(&mutex);
        lock_mutex(&(actor->mutex));
        actor->is_dead = true;
        actors.count_dead++;
        if (debug) {
            size_t debug_ile = actors.count_dead;
            char s[100];
            sprintf("GODIE !!!!! actors count = %d\n", actors.count_dead, s);
            komunikat(s);
        }
        unlock_mutex(&(actor->mutex));
        unlock_mutex(&mutex);
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
//    actor_vec_t *actors_copy = &actors;
//    tpool_t *tpool_copy = &tm;

    curr_id = id;
    lock_mutex(&mutex);
    if (actors.count <= id)
        syserr(1, "Execute messages: wrong actor id\n");
    actor_t *actor = actors.vec[id];
    unlock_mutex(&mutex);
    // TODO Lub zwykły mutex
    lock_mutex(&(actor->mutex));

    actor->has_messages = false;
    actor->working = true;

    queue_t *message_q = actor->messages;
    size_t messages_count = queue_size(message_q);
    if (debug)
        printf("%d: Aktor %d ma %d wiadomości\n", pthread_self() % 100, actor->actor_id, messages_count);

    unlock_mutex(&(actor->mutex));

    while (messages_count > 0) {
        // Wzięcie se mutexa
        lock_mutex(&(actor->mutex));

        message_t *msg = queue_pop(message_q);
        //Zwolnienie mutexa
        unlock_mutex(&(actor->mutex));

        if (debug) printf("%d: Obsługuję wiadomość %d\n", pthread_self() % 100, msg->message_type);
        run_message(actor, msg);
        free_msg(msg);
        messages_count--;
    }

    lock_mutex(&mutex);
    lock_mutex(&actor->mutex);
    if (actor->has_messages) {
//        printf("%d : (2) Dodaję na kolejkę aktora %d, który ma w tym momencie %d wiadomości\n",
//               pthread_self() % 100,
//               id,
//               queue_size(actor->messages));
        actor_id_t *work_id = malloc(sizeof(actor_id_t));
        *work_id = id;
        queue_push(tm->q, work_id);
        tpool_add_notify();
    } else {
//        printf("%d : (2) Aktor %d ma w tym momencie %d wiadomości, ale go nie dodaję!\n",
//               pthread_self() % 100,
//               id,
//               queue_size(actor->messages));
    }
    actor->working = false;
    if (debug)
        printf("%d: Skończyłem! actor %d ma %d wiadomości\n", pthread_self() % 100, actor->actor_id,
               queue_size(actor->messages));
    unlock_mutex(&actor->mutex);
    unlock_mutex(&mutex);
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
    lock_mutex(&mutex);
    if (actors.count <= id) {
        unlock_mutex(&mutex);
        return NO_ACTOR_OF_ID;
    }
    if (id == 0) {
        // count_debug++;
        // printf("%lu\n", count_debug);
    }

    actor_t *act = actors.vec[id];
    lock_mutex(&(act->mutex));

    if (act->is_dead) {
        unlock_mutex(&(act->mutex));
        unlock_mutex(&(mutex));
        return ACTOR_IS_DEAD;
    }

    if (queue_size(act->messages) == ACTOR_QUEUE_LIMIT) {
        unlock_mutex(&(act->mutex));
        unlock_mutex(&(mutex));
        return -3;
    }
    // TODO zdefiniować stałą.

    if (act->role.nprompts <= message.message_type
        && message.message_type != MSG_GODIE
        && message.message_type != MSG_SPAWN) {
        unlock_mutex(&(act->mutex));
        unlock_mutex(&(mutex));
        return -4; // TODO zdefiniować stałą
    }

    message_t *m = create_message(message);
    queue_push(act->messages, m);


    if (!act->has_messages) {
        act->has_messages = true;
        if (!act->working) {
            if (debug)
                printf("%d : (1) Dodaję na kolejkę aktora %d, który ma w tym momencie %d wiadomości\n",
                       pthread_self() % 100,
                       id,
                       queue_size(act->messages));
            work_id = malloc(sizeof(actor_id_t));
            *work_id = id;
            queue_push(tm->q, work_id);
            tpool_add_notify();
        }
    }

    unlock_mutex(&(act->mutex));
    unlock_mutex(&(mutex));
    return 0;
}