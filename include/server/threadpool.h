#ifndef THREADPOOL_H
#define THREADPOOL_H

/*
 * L2. 고정 크기 thread pool.
 *   1차에서는 min_workers == max_workers로 쓴다.
 *   max_workers 필드는 2차의 auto-scaling을 위한 자리만 남겨둔다.
 */

typedef struct {
    int min_workers;
    int max_workers;      /* 1차에서는 min_workers와 같게 둔다 */
    int queue_capacity;   /* 링버퍼 크기 */
    int reject_when_full;  /* 1이면 큐 full 시 submit이 즉시 실패 */
} ThreadPoolConfig;

typedef void (*task_fn)(void *arg);

typedef struct {
    task_fn fn;
    void   *arg;
} Task;

typedef struct ThreadPool ThreadPool;

#define THREADPOOL_OK          0
#define THREADPOOL_ERR        -1
#define THREADPOOL_QUEUE_FULL -2

ThreadPool *threadpool_create(const ThreadPoolConfig *cfg);

/*
 * 큐 뒤에 task를 추가한다.
 * reject_when_full이면 큐가 가득 찼을 때 THREADPOOL_QUEUE_FULL을 반환한다.
 * 아니면 submit 호출 스레드가 not_full 조건에서 잠깐 block — backpressure.
 * shutdown 중이면 -1 반환.
 */
int threadpool_submit(ThreadPool *tp, task_fn fn, void *arg);

/*
 * drain == 1: 큐에 남은 task까지 모두 처리한 뒤 종료.
 * 모든 워커를 pthread_join한다.
 */
void threadpool_shutdown(ThreadPool *tp, int drain);

void threadpool_destroy(ThreadPool *tp);

#endif /* THREADPOOL_H */
