#ifndef ENGINE_ADAPTER_H
#define ENGINE_ADAPTER_H

#include "../interface.h"

/*
 * L3. 엔진 어댑터.
 *   기존 SQL 엔진을 전역 rwlock 하나로 감싸서 스레드 안전한 단일 진입을 제공한다.
 *   다음을 invariant로 가진다:
 *     - lexer_*, parser_*, schema_*, index_*, db_select*, db_insert 등 엔진 함수는
 *       engine_adapter_execute() 안에서만 호출된다.
 *     - 어댑터 공개 함수는 engine_adapter_execute() 하나. 재진입 금지.
 *     - 락 획득/해제는 반드시 단일 exit path로 짝을 이룬다.
 */

typedef enum {
    ENGINE_OK = 0,
    ENGINE_ERR_PARSE,       /* lexer / parser 실패 → 400 */
    ENGINE_ERR_SCHEMA,      /* schema_load 실패 → 404 */
    ENGINE_ERR_VALIDATE,    /* schema_validate 실패 → 400 */
    ENGINE_ERR_UNSUPPORTED, /* 현재 엔진 미지원 SQL → 501 */
    ENGINE_ERR_EXEC,        /* executor 실패 → 500 */
    ENGINE_ERR_INTERNAL     /* index_init 등 내부 실패 → 500 */
} EngineStatus;

typedef enum {
    SQL_KIND_UNKNOWN = 0,
    SQL_KIND_SELECT,
    SQL_KIND_INSERT,
    SQL_KIND_WRITE_UNSUPPORTED
} SqlKind;

typedef struct {
    EngineStatus status;
    SqlKind      kind;
    int          is_select;       /* 1이면 rows가 의미 있음 */
    int          affected_rows;   /* INSERT 성공 시 1 */
    ResultSet   *rows;            /* SELECT 성공 시 non-NULL */
    char         error[512];      /* human-readable 메시지 */
} EngineResult;

typedef struct EngineAdapter EngineAdapter;

EngineAdapter *engine_adapter_create(void);
void           engine_adapter_destroy(EngineAdapter *ea);

/*
 * SQL 한 statement를 실행한다. (여러 개 입력이면 첫 번째만 수행)
 * 내부에서 rwlock을 획득/해제한다.
 * 반환값: 0 성공 경로 종료. out->status로 세부 상태를 확인.
 */
int engine_adapter_execute(EngineAdapter *ea, const char *sql,
                           EngineResult *out);

void engine_result_free(EngineResult *r);

#endif /* ENGINE_ADAPTER_H */
