#ifndef RESPONSE_H
#define RESPONSE_H

#include "engine_adapter.h"

/*
 * L2. 응답 직렬화.
 *   소켓에 HTTP status line + headers + JSON body를 직접 쓴다.
 *   중간 버퍼는 함수 내부에서만 들고 있고 호출자는 손대지 않는다.
 */

/*
 * 상태코드와 JSON 문자열을 받아 응답을 기록한다.
 * status_text 예: "OK", "Bad Request".
 * 반환값: 0 성공, -1 write 실패.
 */
int response_write_json(int fd, int status_code,
                        const char *status_text,
                        const char *json_body);

/* EngineResult를 HTTP 응답으로 변환한다. */
int response_write_engine_result(int fd, const EngineResult *r);

/* 에러 응답 숏컷. msg는 평문, 내부에서 JSON {"error":"..."}로 감싼다. */
int response_write_error(int fd, int status_code, const char *msg);

#endif /* RESPONSE_H */
