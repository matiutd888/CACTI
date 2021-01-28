#include <pthread.h>
#include "thread_pool.h"
#include <stdlib.h>
#include "queue.h"

struct tpool_work {
    thread_func_t func;
    void *arg;
};
typedef struct tpool_work tpool_work_t;

struct tpool {
    queue_t *q;
    pthread_mutex_t work_mutex; // mutex
    pthread_cond_t work_cond;
    pthread_cond_t working_cond;
    size_t working_cnt;
    size_t thread_cnt; // How many threads are we using
    bool stop;
};

// Creates new work
static tpool_work_t *tpool_work_create(thread_func_t func, void *arg) {
    tpool_work_t *work;

    if (func == NULL)
        return NULL;

    work = malloc(sizeof(*work));
    work->func = func;
    work->arg = arg;
    return work;
}

static void tpool_work_destroy(tpool_work_t *work) {
    if (work == NULL)
        return;
    free(work);
}

// Returns beginning of linked list of tpools
static tpool_work_t *tpool_work_get(tpool_t *tm) {
    tpool_work_t *work;

    work = queue_pop(tm->q);
    return work;
}

// Executes work in the treadpool thats pointed by arg.
static void *tpool_worker(void *arg) {
    tpool_t *tm = arg;
    tpool_work_t *work;

    while (1) {
        pthread_mutex_lock(&(tm->work_mutex));

        while (tm->work_first == NULL && !tm->stop)
            pthread_cond_wait(&(tm->work_cond), &(tm->work_mutex));

        if (tm->stop)
            break;

        work = tpool_work_get(tm);
        tm->working_cnt++;
        pthread_mutex_unlock(&(tm->work_mutex));

        if (work != NULL) {
            work->func(work->arg);
            tpool_work_destroy(work);
        }

        pthread_mutex_lock(&(tm->work_mutex));
        tm->working_cnt--;
        if (!tp->stop && tm->working_cnt == 0 && tm->work_first == NULL)
            pthread_cond_signal(&(tm->working_cond));
        pthread_mutex_unlock(&(tm->work_mutex));
    }

    tm->thread_cnt--;
    pthread_cond_signal(&(tm->working_cond));
    pthread_mutex_unlock(&(tm->work_mutex));
    return NULL;
}

tpool_t *tpool_create(size_t num) {
    tpool_t *tm;
    pthread_t thread;
    size_t i;

    if (num == 0)
        num = 2;

    tm = NULL;
    tm = calloc(1, sizeof(*tm));

    if (tm == NULL)
        return NULL;

    tm->thread_cnt = num;

    if (pthread_mutex_init(&(tm->work_mutex), NULL) != 0)
        return NULL;
    if (pthread_cond_init(&(tm->work_cond), NULL) != 0)
        return NULL;
    if (pthread_cond_init(&(tm->working_cond), NULL) != 0)
        return NULL;

    tm->q = init_queue(INIT_QUEUE_SIZE);

    for (i = 0; i < num; i++) {
        if (pthread_create(&thread, NULL, tpool_worker, tm) != 0)
            exit(1);
        pthread_detach(thread);
    }

    return tm;
}

void tpool_destroy(tpool_t *tm) {
    if (tm == NULL)
        return;

    pthread_mutex_lock(&(tm->work_mutex));
    queue_destruct(tm->q);
    tm->stop = true;

    pthread_cond_broadcast(&(tm->work_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    tpool_wait(tm);

    pthread_mutex_destroy(&(tm->work_mutex));
    pthread_cond_destroy(&(tm->work_cond));
    pthread_cond_destroy(&(tm->working_cond));

    free(tm);
}

bool tpool_add_work(tpool_t *tm, thread_func_t func, void *arg) {
    tpool_work_t *work;

    if (tm == NULL)
        return false;

    work = tpool_work_create(func, arg);
    if (work == NULL)
        return false;

    pthread_mutex_lock(&(tm->work_mutex));
    queue_push(tm->q, work);

    pthread_cond_broadcast(&(tm->work_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    return true;
}

void tpool_wait(tpool_t *tm) {
    if (tm == NULL)
        return;

    pthread_mutex_lock(&(tm->work_mutex));
    while (1) {
        if ((!tp->stop && tp->working_cnt != 0) || (tp->stop && tp->thread_cnt != 0)) {
            pthread_cond_wait(&(tm->working_cond), &(tm->work_mutex));
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&(tm->work_mutex));
}