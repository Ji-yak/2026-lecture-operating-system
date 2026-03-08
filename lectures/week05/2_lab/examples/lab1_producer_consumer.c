/*
 * lab1_producer_consumer.c
 *
 * Producer-Consumer 패턴 (생산자-소비자 패턴)
 * ============================================
 *
 * [개요]
 * Producer-Consumer는 운영체제에서 가장 대표적인 동기화 문제 중 하나이다.
 * Producer(생산자)는 데이터를 만들어 공유 버퍼에 넣고,
 * Consumer(소비자)는 버퍼에서 데이터를 꺼내 사용한다.
 *
 * [Bounded Buffer (유한 버퍼)]
 * 버퍼의 크기가 유한하기 때문에 두 가지 상황에서 동기화가 필요하다:
 *   (1) 버퍼가 가득 찬 경우 - Producer는 빈 공간이 생길 때까지 대기해야 한다
 *   (2) 버퍼가 비어 있는 경우 - Consumer는 데이터가 들어올 때까지 대기해야 한다
 *
 * [사용하는 동기화 도구]
 *   - pthread_mutex_t : 상호 배제(mutual exclusion)를 보장하여 공유 자원에
 *                       한 번에 하나의 스레드만 접근하도록 한다.
 *   - pthread_cond_t  : 조건 변수(condition variable)로, 특정 조건이 만족될 때까지
 *                       스레드를 대기(wait)시키고, 조건이 만족되면 깨운다(signal).
 *
 * [Mesa Semantics (메사 의미론)]
 * pthread의 조건 변수는 Mesa semantics를 따른다.
 * Mesa semantics에서는 signal을 받고 깨어난 스레드가 즉시 실행되는 것이 아니라,
 * 다시 mutex를 획득한 후에야 실행을 재개한다. 그 사이에 다른 스레드가 먼저
 * mutex를 잡고 조건을 변경할 수 있다.
 *
 * 따라서 조건 확인 시 반드시 while 루프를 사용해야 한다:
 *   - if(조건) wait  -> 잘못된 코드! (Hoare semantics에서만 안전)
 *   - while(조건) wait -> 올바른 코드! (Mesa semantics에서 안전)
 *
 * while을 사용하면 깨어난 후에도 조건을 다시 확인하므로,
 * 다른 스레드가 먼저 조건을 변경했더라도 안전하게 동작한다.
 * 또한 OS나 라이브러리가 이유 없이 깨우는 "spurious wakeup"에도 대비할 수 있다.
 *
 * [signal vs broadcast]
 *   - pthread_cond_signal   : 대기 중인 스레드 중 하나만 깨운다.
 *                             1:1 관계에서 효율적이다.
 *   - pthread_cond_broadcast : 대기 중인 모든 스레드를 깨운다.
 *                              여러 스레드가 대기할 때 사용한다.
 *   이 예제에서는 Producer 1개, Consumer 1개이므로 signal로 충분하다.
 *
 * 컴파일: gcc -Wall -pthread -o lab1_producer_consumer lab1_producer_consumer.c
 * 실행:   ./lab1_producer_consumer
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/* ============================================================
 * Bounded Buffer 정의
 *
 * 원형 버퍼(circular buffer)로 구현한다.
 * in과 out 인덱스를 BUFFER_SIZE로 나눈 나머지를 사용하여
 * 배열의 끝에 도달하면 다시 처음으로 돌아간다.
 * ============================================================ */
#define BUFFER_SIZE 5       // 버퍼 크기 (슬롯 수)
#define NUM_ITEMS   20      // 생산/소비할 아이템 총 수

typedef struct {
    int     data[BUFFER_SIZE];  // 원형 버퍼 배열 (실제 데이터 저장 공간)
    int     in;                 // 다음 삽입 위치 (Producer가 여기에 넣음)
    int     out;                // 다음 제거 위치 (Consumer가 여기서 꺼냄)
    int     count;              // 현재 버퍼에 들어있는 아이템 수

    pthread_mutex_t mutex;      // 뮤텍스: 버퍼 접근에 대한 상호 배제 보장
    pthread_cond_t  not_full;   // 조건 변수: "버퍼가 가득 차지 않음" 조건을 나타냄
    pthread_cond_t  not_empty;  // 조건 변수: "버퍼가 비어있지 않음" 조건을 나타냄
} bounded_buffer_t;

/* 전역 bounded buffer 인스턴스 */
bounded_buffer_t buffer;

/* ============================================================
 * 버퍼 초기화
 *
 * 뮤텍스와 조건 변수를 초기화한다.
 * pthread_mutex_init/pthread_cond_init의 두 번째 인자가 NULL이면
 * 기본 속성(default attributes)을 사용한다.
 * ============================================================ */
void buffer_init(bounded_buffer_t *buf) {
    buf->in    = 0;
    buf->out   = 0;
    buf->count = 0;

    /* 뮤텍스 초기화: 공유 자원 보호용 잠금 장치 생성 */
    pthread_mutex_init(&buf->mutex, NULL);

    /* 조건 변수 초기화: 각각 "가득 차지 않음"과 "비어있지 않음"을 위한 대기 큐 생성 */
    pthread_cond_init(&buf->not_full, NULL);
    pthread_cond_init(&buf->not_empty, NULL);
}

/* ============================================================
 * 버퍼 삽입 (Producer가 호출)
 *
 * 동작 순서:
 *   1. pthread_mutex_lock   - 뮤텍스 획득 (임계 영역 진입)
 *   2. while + cond_wait    - 버퍼가 가득 찼으면 대기
 *   3. 데이터 삽입           - 원형 버퍼에 아이템 추가
 *   4. pthread_cond_signal  - 대기 중인 Consumer 깨움
 *   5. pthread_mutex_unlock - 뮤텍스 해제 (임계 영역 탈출)
 * ============================================================ */
void buffer_put(bounded_buffer_t *buf, int item) {
    /* 뮤텍스 획득: 이 시점부터 다른 스레드는 이 뮤텍스를 잡을 수 없다.
     * 공유 자원(buf->count, buf->data 등)에 안전하게 접근할 수 있다. */
    pthread_mutex_lock(&buf->mutex);

    /* [핵심] Mesa semantics에 따른 while 루프 패턴
     *
     * 왜 if가 아니라 while인가?
     *   - pthread_cond_wait()에서 깨어났다고 해서 조건이 반드시 참인 것은 아니다.
     *   - 깨어난 후 mutex를 재획득하기까지 사이에 다른 스레드가 먼저 실행되어
     *     조건을 다시 거짓으로 만들 수 있다. (Mesa semantics의 핵심)
     *   - 또한 "spurious wakeup" (가짜 깨어남)이 발생할 수도 있다.
     *   - while 루프는 깨어난 후 조건을 다시 확인하므로 이 모든 경우에 안전하다.
     */
    while (buf->count == BUFFER_SIZE) {
        printf("  [Producer] 버퍼 가득 참, 대기 중...\n");
        /* pthread_cond_wait 동작 (원자적으로 수행):
         *   (1) 뮤텍스를 해제한다 (다른 스레드가 임계 영역에 진입 가능)
         *   (2) 이 스레드를 not_full 조건 변수의 대기 큐에 넣고 잠든다
         *   (3) signal/broadcast를 받으면 깨어나서 뮤텍스를 다시 획득한다 */
        pthread_cond_wait(&buf->not_full, &buf->mutex);
        /* 이 지점에 도달하면 뮤텍스를 다시 획득한 상태이다.
         * while 루프 조건을 다시 확인하여 진짜 공간이 있는지 검증한다. */
    }

    /* 원형 버퍼에 데이터 삽입:
     * in 위치에 데이터를 넣고, in을 다음 위치로 이동시킨다.
     * % BUFFER_SIZE 연산으로 배열 끝에서 다시 처음으로 순환한다. */
    buf->data[buf->in] = item;
    buf->in = (buf->in + 1) % BUFFER_SIZE;
    buf->count++;

    printf("  [Producer] 생산: %d (버퍼: %d/%d)\n",
           item, buf->count, BUFFER_SIZE);

    /* pthread_cond_signal: 대기 중인 Consumer 스레드 중 하나를 깨운다.
     *
     * Consumer가 not_empty 조건 변수에서 대기 중일 수 있다.
     * 아이템이 하나 들어왔으므로 "버퍼가 비어있지 않다"는 조건이 참이 되었고,
     * 대기 중인 Consumer를 깨워서 소비할 수 있게 한다.
     *
     * signal은 하나의 스레드만 깨우므로, Consumer가 1개인 이 예제에서는
     * broadcast 대신 signal로 충분하다. */
    pthread_cond_signal(&buf->not_empty);

    /* 뮤텍스 해제: 임계 영역을 벗어나 다른 스레드가 접근할 수 있게 한다. */
    pthread_mutex_unlock(&buf->mutex);
}

/* ============================================================
 * 버퍼에서 제거 (Consumer가 호출)
 *
 * 동작 순서:
 *   1. pthread_mutex_lock   - 뮤텍스 획득 (임계 영역 진입)
 *   2. while + cond_wait    - 버퍼가 비어있으면 대기
 *   3. 데이터 제거           - 원형 버퍼에서 아이템 꺼냄
 *   4. pthread_cond_signal  - 대기 중인 Producer 깨움
 *   5. pthread_mutex_unlock - 뮤텍스 해제 (임계 영역 탈출)
 * ============================================================ */
int buffer_get(bounded_buffer_t *buf) {
    int item;

    /* 뮤텍스 획득: 공유 자원에 안전하게 접근하기 위해 잠금 */
    pthread_mutex_lock(&buf->mutex);

    /* [핵심] Mesa semantics에 따른 while 루프 패턴
     *
     * 버퍼가 비어있으면 데이터가 들어올 때까지 대기한다.
     * if가 아닌 while을 사용하는 이유는 buffer_put()의 주석과 동일하다:
     *   - 깨어난 후 다른 Consumer가 먼저 데이터를 가져갔을 수 있다
     *   - spurious wakeup이 발생할 수 있다
     *   - while로 조건을 재확인하면 이 모든 상황에서 안전하다 */
    while (buf->count == 0) {
        printf("  [Consumer] 버퍼 비어있음, 대기 중...\n");
        /* pthread_cond_wait: 뮤텍스를 해제하고 not_empty 대기 큐에서 잠든다.
         * Producer가 signal을 보내면 깨어나서 뮤텍스를 다시 획득한다. */
        pthread_cond_wait(&buf->not_empty, &buf->mutex);
    }

    /* 원형 버퍼에서 데이터 제거:
     * out 위치에서 데이터를 꺼내고, out을 다음 위치로 이동시킨다. */
    item = buf->data[buf->out];
    buf->out = (buf->out + 1) % BUFFER_SIZE;
    buf->count--;

    printf("  [Consumer] 소비: %d (버퍼: %d/%d)\n",
           item, buf->count, BUFFER_SIZE);

    /* pthread_cond_signal: 대기 중인 Producer 스레드 중 하나를 깨운다.
     *
     * Producer가 not_full 조건 변수에서 대기 중일 수 있다.
     * 아이템 하나를 꺼내서 공간이 생겼으므로 "버퍼가 가득 차지 않았다"는
     * 조건이 참이 되었고, 대기 중인 Producer를 깨워서 생산할 수 있게 한다. */
    pthread_cond_signal(&buf->not_full);

    /* 뮤텍스 해제: 임계 영역을 벗어남 */
    pthread_mutex_unlock(&buf->mutex);

    return item;
}

/* ============================================================
 * 버퍼 정리
 *
 * 사용이 끝난 뮤텍스와 조건 변수의 자원을 해제한다.
 * pthread_*_destroy는 초기화된 동기화 객체의 자원을 정리한다.
 * ============================================================ */
void buffer_destroy(bounded_buffer_t *buf) {
    pthread_mutex_destroy(&buf->mutex);       // 뮤텍스 자원 해제
    pthread_cond_destroy(&buf->not_full);     // not_full 조건 변수 자원 해제
    pthread_cond_destroy(&buf->not_empty);    // not_empty 조건 변수 자원 해제
}

/* ============================================================
 * Producer 스레드 함수
 *
 * NUM_ITEMS개의 아이템을 생성하여 버퍼에 넣는다.
 * 각 아이템 생산 후 랜덤한 시간만큼 대기하여 실제 작업을 시뮬레이션한다.
 * ============================================================ */
void *producer(void *arg) {
    int id = *(int *)arg;

    for (int i = 0; i < NUM_ITEMS; i++) {
        int item = id * 1000 + i;   // 고유한 아이템 번호 생성 (Producer ID * 1000 + 순번)
        buffer_put(&buffer, item);  // 버퍼에 삽입 (내부에서 동기화 처리)
        usleep(rand() % 100000);    // 0~100ms 랜덤 대기 (생산에 걸리는 시간 시뮬레이션)
    }

    printf("  [Producer %d] 생산 완료\n", id);
    return NULL;
}

/* ============================================================
 * Consumer 스레드 함수
 *
 * NUM_ITEMS개의 아이템을 버퍼에서 꺼내 소비한다.
 * 각 아이템 소비 후 랜덤한 시간만큼 대기하여 실제 처리를 시뮬레이션한다.
 * ============================================================ */
void *consumer(void *arg) {
    int id = *(int *)arg;

    for (int i = 0; i < NUM_ITEMS; i++) {
        int item = buffer_get(&buffer);  // 버퍼에서 아이템 꺼냄 (내부에서 동기화 처리)
        (void)item;                      // 사용하지 않는 변수 경고 제거 (컴파일러 경고 방지)
        usleep(rand() % 150000);         // 0~150ms 랜덤 대기 (소비에 걸리는 시간 시뮬레이션)
    }

    printf("  [Consumer %d] 소비 완료\n", id);
    return NULL;
}

/* ============================================================
 * main: 프로그램 진입점
 *
 * 1. 버퍼를 초기화한다
 * 2. Producer와 Consumer 스레드를 각각 1개씩 생성한다
 * 3. 두 스레드가 모두 종료될 때까지 대기한다 (pthread_join)
 * 4. 자원을 정리하고 종료한다
 * ============================================================ */
int main(void) {
    pthread_t prod_thread, cons_thread;
    int prod_id = 1, cons_id = 1;

    printf("=== Producer-Consumer 문제 ===\n");
    printf("버퍼 크기: %d, 생산/소비 아이템 수: %d\n\n", BUFFER_SIZE, NUM_ITEMS);

    /* 버퍼 초기화: 뮤텍스와 조건 변수 생성 */
    buffer_init(&buffer);

    /* pthread_create: 새로운 스레드를 생성한다.
     * 인자: (스레드 핸들, 속성(NULL=기본), 스레드 함수, 함수에 전달할 인자)
     * Producer와 Consumer가 동시에 실행되며 버퍼를 공유한다. */
    pthread_create(&prod_thread, NULL, producer, &prod_id);
    pthread_create(&cons_thread, NULL, consumer, &cons_id);

    /* pthread_join: 해당 스레드가 종료될 때까지 main 스레드가 대기한다.
     * join하지 않으면 main이 먼저 끝나 프로세스가 종료될 수 있다. */
    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, NULL);

    /* 동기화 객체 자원 정리 */
    buffer_destroy(&buffer);

    printf("\n=== 프로그램 정상 종료 ===\n");
    return 0;
}
