#!/bin/bash
LOCK_PORT=8080
NOLOCK_PORT=8081
TOTAL=2000
BATCH=50
WORKERS=2
BASE_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "========================================"
echo "  원자성 테스트 (INSERT $TOTAL 건)"
echo "  락 있음  → port $LOCK_PORT"
echo "  락 없음  → port $NOLOCK_PORT"
echo "========================================"

rm -f "$BASE_DIR/data/users_lock.dat"
rm -f "$BASE_DIR/data/users_nolock.dat"

cd "$BASE_DIR"
./sqlpd        $LOCK_PORT   $WORKERS 4096 > /dev/null 2>&1 &
LOCK_PID=$!
./sqlpd_nolock $NOLOCK_PORT $WORKERS 4096 > /dev/null 2>&1 &
NOLOCK_PID=$!
sleep 2

send_batch() {
    local port=$1
    local table=$2
    local from=$3
    local to=$4
    for i in $(seq $from $to); do
        curl -s -o /dev/null -X POST "http://localhost:$port/sql" \
            -d "sql=INSERT INTO $table VALUES ($i, 'user$i', $((20 + i % 50)), 'u$i@test.com')" &
    done
    wait
}

echo ""
echo "[1/2] 락 없는 서버 INSERT $TOTAL 건..."
for batch_start in $(seq 1 $BATCH $TOTAL); do
    batch_end=$((batch_start + BATCH - 1))
    [ $batch_end -gt $TOTAL ] && batch_end=$TOTAL
    send_batch $NOLOCK_PORT "users_nolock" $batch_start $batch_end
    printf "  진행: %d / %d\r" $batch_end $TOTAL
done
echo ""
echo "  완료"

echo ""
echo "[2/2] 락 있는 서버 INSERT $TOTAL 건..."
for batch_start in $(seq 1 $BATCH $TOTAL); do
    batch_end=$((batch_start + BATCH - 1))
    [ $batch_end -gt $TOTAL ] && batch_end=$TOTAL
    send_batch $LOCK_PORT "users_lock" $batch_start $batch_end
    printf "  진행: %d / %d\r" $batch_end $TOTAL
done
echo ""
echo "  완료"

kill $LOCK_PID $NOLOCK_PID 2>/dev/null
sleep 1

NOLOCK_COUNT=0
LOCK_COUNT=0
[ -f "$BASE_DIR/data/users_nolock.dat" ] && NOLOCK_COUNT=$(wc -l < "$BASE_DIR/data/users_nolock.dat")
[ -f "$BASE_DIR/data/users_lock.dat"   ] && LOCK_COUNT=$(wc -l < "$BASE_DIR/data/users_lock.dat")

LOST=$((TOTAL - NOLOCK_COUNT))

echo ""
echo "========================================"
echo "  결과"
echo "----------------------------------------"
printf "  락 없음: 요청 %5d 건 → 실제 저장 %5d 건\n" $TOTAL $NOLOCK_COUNT
printf "  락 있음: 요청 %5d 건 → 실제 저장 %5d 건\n" $TOTAL $LOCK_COUNT
printf "  유실된 레코드 (락 없음): %d 건\n" $LOST
echo "========================================"
