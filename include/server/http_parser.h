#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <stddef.h>

/*
 * L2. HTTP 요청 파서 — 1차는 Content-Length 기반 단일 요청만 지원.
 * chunked, keep-alive, pipelining은 2차 이후.
 */

typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_METHOD_OTHER
} HttpMethod;

typedef struct {
    HttpMethod method;
    char   path[256];          /* "/sql" */
    char   query[4096];        /* "sql=SELECT..." (URL-encoded 원문) */
    char  *body;               /* malloc'd, nullable */
    size_t body_len;
    int    content_length;     /* 헤더 원본, 없으면 0 */
    int    bad_request;        /* 1이면 라인/헤더 파싱 단계에서 오류 */
    int    payload_too_large;  /* body 제한 초과 */
} HttpRequest;

/*
 * 소켓 fd에서 한 개의 HTTP 요청을 읽어 out에 채운다.
 * 반환값: 0 성공, -1 I/O 실패 또는 EOF.
 * out->bad_request == 1 이면 "연결은 됐지만 형식이 잘못" — 호출자가 400을 내면 된다.
 */
int http_parser_read(int fd, HttpRequest *out);

void http_request_free(HttpRequest *req);

/*
 * POST body 또는 GET query string "sql=..." / "q=..." 에서 SQL을 꺼낸다.
 * POST의 body는 그대로 복사, GET의 query는 URL-decoded 후 복사.
 * 결과를 out 버퍼(cap)에 NUL-terminated로 채운다.
 * 반환값: 0 성공, -1 SQL이 없거나 버퍼 부족.
 */
int http_parser_extract_sql(const HttpRequest *req, char *out, size_t cap);

#endif /* HTTP_PARSER_H */
