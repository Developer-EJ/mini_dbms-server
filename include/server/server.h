#ifndef SERVER_H
#define SERVER_H

/*
 * L1. 네트워크 레이어
 *   listen / accept 루프만 담당한다.
 *   HTTP, SQL 등 상위 프로토콜은 모른다.
 *   연결 처리 방법은 on_accept 콜백으로 의존성 주입된다.
 */

typedef struct {
    int port;
    int backlog;
} ServerConfig;

typedef struct Server Server;

/* accept 루프에 필요한 fd만 만들어 둔다. 실패 시 NULL. */
Server *server_create(const ServerConfig *cfg);

/*
 * accept 루프를 시작한다.
 * on_accept(client_fd, ctx)는 새 연결이 들어올 때마다 호출된다.
 *   - 콜백은 client_fd의 소유권을 가져간다 (close 책임 포함).
 * server_stop()이 호출되면 리턴한다. 성공 시 0.
 */
int server_run(Server *s,
               void (*on_accept)(int client_fd, void *ctx),
               void *ctx);

/*
 * accept 루프를 깨운다. 시그널 핸들러 등 외부에서 호출.
 * listen fd를 닫아서 blocking accept()에서 빠져나오게 한다.
 */
void server_stop(Server *s);

void server_destroy(Server *s);

#endif /* SERVER_H */
