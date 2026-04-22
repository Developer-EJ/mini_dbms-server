#include "server/threadpool.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * threadpool.c
 *
 * 고정 크기 FIFO 링버퍼 + mutex/cond 두 개로 구성된 thread pool.
 *
 *   - submit: 큐 full이면 정책에 따라 즉시 실패하거나 not_full 대기
 *             → 큐에 push → not_empty signal
 *   - worker: not_empty 조건에서 대기 → 큐에서 pop → lock 밖에서 실행
 *
 * shutdown(drain=1):
 *   - flag를 세우고 broadcast
 *   - 워커들은 남은 task를 처리한 뒤 큐가 비면 루프 탈출
 *   - main이 모든 워커를 join
 *
 * 1차 범위에서 즉시 중단 모드(drain=0)는 지원하지 않는다.
 */

struct ThreadPool {
    pthread_mutex_t  mtx;
    pthread_cond_t   not_empty;
    pthread_cond_t   not_full;

    Task            *queue;
    int              cap;
    int              head;
    int              tail;
    int              size;

    pthread_t       *workers;
    int              n_workers;

    int              shutdown;     /* 0=run, 1=drain */
    ThreadPoolConfig cfg;
};

static void *worker_loop(void *arg) {
    ThreadPool *tp = (ThreadPool *)arg;

    for (;;) {
        pthread_mutex_lock(&tp->mtx);

        /* spurious wakeup을 대비해 항상 while로 조건 확인 */
        while (tp->size == 0 && tp->shutdown == 0)
            pthread_cond_wait(&tp->not_empty, &tp->mtx);

        /* drain 모드인데 큐도 비었으면 종료 */
        if (tp->size == 0 && tp->shutdown != 0) {
            pthread_mutex_unlock(&tp->mtx);
            return NULL;
        }

        Task t = tp->queue[tp->head];
        tp->head = (tp->head + 1) % tp->cap;
        tp->size--;

        /* submit 쪽이 not_full을 기다릴 수 있으므로 깨운다 */
        pthread_cond_signal(&tp->not_full);
        pthread_mutex_unlock(&tp->mtx);

        /* 락 밖에서 실행 — 엔진이 오래 걸려도 큐가 막히지 않는다 */
        t.fn(t.arg);
    }
}

ThreadPool *threadpool_create(const ThreadPoolConfig *cfg) {
    if (!cfg || cfg->min_workers <= 0 || cfg->queue_capacity <= 0)
        return NULL;
    if (cfg->min_workers != cfg->max_workers)
        return NULL;

    int n = cfg->min_workers;   /* 1차: min == max */

    ThreadPool *tp = (ThreadPool *)calloc(1, sizeof(ThreadPool));
    if (!tp) return NULL;

    tp->cfg       = *cfg;
    tp->cap       = cfg->queue_capacity;
    tp->queue     = (Task *)calloc((size_t)tp->cap, sizeof(Task));
    tp->workers   = (pthread_t *)calloc((size_t)n, sizeof(pthread_t));
    tp->n_workers = n;

    if (!tp->queue || !tp->workers) {
        free(tp->queue);
        free(tp->workers);
        free(tp);
        return NULL;
    }

    pthread_mutex_init(&tp->mtx, NULL);
    pthread_cond_init(&tp->not_empty, NULL);
    pthread_cond_init(&tp->not_full, NULL);

    for (int i = 0; i < n; i++) {
        if (pthread_create(&tp->workers[i], NULL, worker_loop, tp) != 0) {
            /*
             * 부분 실패: 지금까지 띄운 워커에게 shutdown을 알리고 join해서 정리한다.
             * 이 경로에 들어왔다는 것은 시스템 리소스가 부족하다는 뜻이므로
             * 조용히 실패시키는 것이 맞다.
             */
            tp->n_workers = i;
            threadpool_shutdown(tp, 1);
            threadpool_destroy(tp);
            return NULL;
        }
    }

    return tp;
}

int threadpool_submit(ThreadPool *tp, task_fn fn, void *arg) {
    if (!tp || !fn) return THREADPOOL_ERR;

    pthread_mutex_lock(&tp->mtx);

    /*
     * 큐가 가득 차 있으면 1차 기본 정책은 즉시 실패(503 응답용).
     * reject_when_full=0이면 accept 스레드가 잠깐 block → backpressure.
     * shutdown 중이면 즉시 실패.
     */
    if (tp->size == tp->cap && tp->cfg.reject_when_full) {
        pthread_mutex_unlock(&tp->mtx);
        return THREADPOOL_QUEUE_FULL;
    }

    while (tp->size == tp->cap && tp->shutdown == 0)
        pthread_cond_wait(&tp->not_full, &tp->mtx);

    if (tp->shutdown != 0) {
        pthread_mutex_unlock(&tp->mtx);
        return THREADPOOL_ERR;
    }

    tp->queue[tp->tail].fn  = fn;
    tp->queue[tp->tail].arg = arg;
    tp->tail = (tp->tail + 1) % tp->cap;
    tp->size++;

    pthread_cond_signal(&tp->not_empty);
    pthread_mutex_unlock(&tp->mtx);
    return THREADPOOL_OK;
}

void threadpool_shutdown(ThreadPool *tp, int drain) {
    if (!tp) return;
    (void)drain; /* 1차는 drain=1만 의미를 가진다 */

    pthread_mutex_lock(&tp->mtx);
    tp->shutdown = 1;
    pthread_cond_broadcast(&tp->not_empty);
    pthread_cond_broadcast(&tp->not_full);
    pthread_mutex_unlock(&tp->mtx);

    for (int i = 0; i < tp->n_workers; i++)
        pthread_join(tp->workers[i], NULL);
}

void threadpool_destroy(ThreadPool *tp) {
    if (!tp) return;

    pthread_mutex_destroy(&tp->mtx);
    pthread_cond_destroy(&tp->not_empty);
    pthread_cond_destroy(&tp->not_full);

    free(tp->queue);
    free(tp->workers);
    free(tp);
}
