#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "threadpool.h"
#include "engine_adapter.h"

/*
 * L2. 요청 디스패처.
 *   accept 콜백은 DispatchCtx를 malloc해 thread pool에 submit한다.
 *   워커는 DispatchCtx를 받아 파싱 → 라우팅 → 어댑터 → 응답까지 담당하고 free한다.
 */

typedef struct {
    ThreadPool    *pool;    /* 의존성 주입용 묶음 */
    EngineAdapter *engine;
} DispatchDeps;

typedef struct {
    int            client_fd;
    EngineAdapter *engine;
} DispatchCtx;

/* on_accept 콜백: server_run()에 넘긴다. ctx_opaque는 DispatchDeps*. */
void dispatcher_on_accept(int client_fd, void *ctx_opaque);

/* 단일 연결 처리. 테스트와 직접 호출용. */
int dispatcher_handle_client(int client_fd, EngineAdapter *engine);

/* thread pool에 enqueue되는 실제 task 본체. arg는 DispatchCtx* (heap-owned). */
void dispatcher_handle_task(void *arg);

#endif /* DISPATCHER_H */
