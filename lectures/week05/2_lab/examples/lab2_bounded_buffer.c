/*
 * lab2_bounded_buffer.c
 *
 * 다수의 Producer/Consumer 스레드를 사용하는 Bounded Buffer 구현
 * ================================================================
 *
 * [개요]
 * lab1에서 1:1 구조의 Producer-Consumer를 다루었다면,
 * 이 예제에서는 다수의 Producer와 Consumer가 동시에 하나의 공유 버퍼에 접근하는
 * 더 현실적인 시나리오를 구현한다.
 *
 * [다중 스레드 환경에서의 추가 고려사항]
 * 여러 Producer/Consumer가 동시에 동작하면 다음과 같은 상황이 발생할 수 있다:
 *   - 여러 Producer가 동시에 빈 슬롯 하나를 두고 경쟁
 *   - 여러 Consumer가 동시에 하나의 아이템을 두고 경쟁
 *   - signal로 깨어난 스레드가 mutex를 획득하기 전에 다른 스레드가 먼저 처리
 *
 * [Mesa Semantics와 while 루프의 중요성]
 * Mesa semantics에서 pthread_cond_signal()은 "힌트"에 불과하다.
 * signal을 받고 깨어난 스레드가 실제로 mutex를 획득하여 실행되기 전에,
 * 다른 스레드가 먼저 mutex를 잡고 버퍼 상태를 변경할 수 있다.
 *
 * 예시: Consumer A와 B가 모두 빈 버퍼에서 대기 중
 *   1) Producer가 아이템 1개를 넣고 signal 전송
 *   2) Consumer A가 깨어나서 mutex 획득 시도
 *   3) 그 사이에 Consumer B가 먼저 mutex를 획득하고 아이템을 가져감
 *   4) Consumer A가 mutex를 획득했을 때 버퍼는 다시 비어 있음
 *   => if를 쓰면 빈 버퍼에서 꺼내려고 시도하여 오류 발생!
 *   => while을 쓰면 조건을 다시 확인하고 다시 대기하므로 안전!
 *
 * [signal vs broadcast 사용 기준]
 * - pthread_cond_signal: 대기 중인 스레드 중 정확히 하나만 깨운다.
 *   put/get에서 사용: 공간/데이터가 하나 생겼으므로 하나만 깨우면 충분하다.
 *
 * - pthread_cond_broadcast: 대기 중인 모든 스레드를 깨운다.
 *   종료 시 사용: 모든 Consumer에게 "생산이 끝났으니 종료하라"고 알려야 하므로
 *   broadcast를 사용한다. signal을 쓰면 일부 Consumer가 영원히 대기할 수 있다.
 *
 * [종료 처리]
 * 다중 Consumer 환경에서는 "언제 종료할 것인가"가 중요한 문제이다.
 * 이 예제에서는 done 플래그와 broadcast를 조합하여 해결한다:
 *   1) 모든 Producer가 종료되면 main에서 done = 1로 설정
 *   2) broadcast로 대기 중인 모든 Consumer를 깨움
 *   3) Consumer는 깨어나서 done 플래그를 확인하고 종료
 *
 * 컴파일: gcc -Wall -pthread -o lab2_bounded_buffer lab2_bounded_buffer.c
 * 실행:   ./lab2_bounded_buffer
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

/* ============================================================
 * 설정값
 *
 * BUFFER_SIZE를 의도적으로 작게 설정하여 버퍼가 가득 차거나 비는 상황을
 * 자주 발생시킨다. 이를 통해 동기화 메커니즘이 올바르게 동작하는지 관찰한다.
 * ============================================================ */
#define BUFFER_SIZE     4       // 의도적으로 작은 버퍼 (동기화 이슈를 빈번하게 관찰)
#define NUM_PRODUCERS   3       // Producer 스레드 수
#define NUM_CONSUMERS   3       // Consumer 스레드 수
#define ITEMS_PER_PROD  10      // 각 Producer가 생산할 아이템 수

/* ============================================================
 * Bounded Buffer 구조체
 *
 * lab1과 동일한 원형 버퍼에 추가로 통계 필드와 종료 플래그가 있다.
 * 모든 필드는 lock 뮤텍스로 보호되어야 한다.
 * ============================================================ */
typedef struct {
    int     data[BUFFER_SIZE];  // 원형 버퍼 배열
    int     in;                 // 다음 삽입 위치
    int     out;                // 다음 제거 위치
    int     count;              // 현재 아이템 수

    pthread_mutex_t lock;       // 뮤텍스: 모든 필드에 대한 상호 배제
    pthread_cond_t  not_full;   // 조건 변수: 버퍼에 빈 공간이 있음
    pthread_cond_t  not_empty;  // 조건 변수: 버퍼에 데이터가 있음

    /* 통계 및 종료 제어 */
    int     total_produced;     // 총 생산된 아이템 수 (검증용)
    int     total_consumed;     // 총 소비된 아이템 수 (검증용)
    int     done;               // 모든 생산 완료 플래그 (1이면 더 이상 생산 없음)
} bbuf_t;

/* 전역 bounded buffer 인스턴스 */
static bbuf_t bbuf;

/* ============================================================
 * 초기화
 *
 * 뮤텍스, 조건 변수, 통계 카운터를 모두 초기화한다.
 * ============================================================ */
void bbuf_init(bbuf_t *b) {
    b->in = 0;
    b->out = 0;
    b->count = 0;
    b->total_produced = 0;
    b->total_consumed = 0;
    b->done = 0;   // 아직 생산이 끝나지 않았음을 나타냄

    /* 동기화 객체 초기화: NULL은 기본 속성 사용 */
    pthread_mutex_init(&b->lock, NULL);       // 뮤텍스 초기화
    pthread_cond_init(&b->not_full, NULL);    // "빈 공간 있음" 조건 변수 초기화
    pthread_cond_init(&b->not_empty, NULL);   // "데이터 있음" 조건 변수 초기화
}

/* ============================================================
 * 정리
 *
 * 프로그램 종료 전에 동기화 객체의 자원을 반환한다.
 * ============================================================ */
void bbuf_destroy(bbuf_t *b) {
    pthread_mutex_destroy(&b->lock);          // 뮤텍스 자원 해제
    pthread_cond_destroy(&b->not_full);       // not_full 조건 변수 자원 해제
    pthread_cond_destroy(&b->not_empty);      // not_empty 조건 변수 자원 해제
}

/* ============================================================
 * put: 버퍼에 아이템 삽입 (Producer가 호출)
 *
 * 동작 순서:
 *   1. mutex 획득 (pthread_mutex_lock)
 *   2. 버퍼가 가득 찼으면 while 루프에서 대기 (pthread_cond_wait)
 *   3. 아이템 삽입 및 통계 갱신
 *   4. 대기 중인 Consumer 깨움 (pthread_cond_signal)
 *   5. mutex 해제 (pthread_mutex_unlock)
 * ============================================================ */
void bbuf_put(bbuf_t *b, int item) {
    /* 뮤텍스 획득: 임계 영역 진입. 이 시점부터 다른 스레드는
     * 이 뮤텍스가 해제될 때까지 lock에서 블로킹된다. */
    pthread_mutex_lock(&b->lock);

    /* [핵심] Mesa semantics에 따른 while 루프 조건 대기 패턴
     *
     * 다중 Producer 환경에서 while이 특히 중요한 이유:
     *   - Consumer가 signal을 보냈을 때, 여러 Producer가 동시에 깨어날 수 있다
     *     (정확히는 하나만 깨어나지만, 다른 Producer가 먼저 mutex를 잡을 수 있다)
     *   - 먼저 mutex를 잡은 Producer가 빈 공간을 채워버리면,
     *     뒤늦게 mutex를 잡은 Producer는 다시 대기해야 한다
     *   - while 루프가 이 재확인을 자동으로 수행한다
     *
     * pthread_cond_wait 동작 (원자적으로 수행):
     *   (1) 뮤텍스를 해제한다
     *   (2) 이 스레드를 not_full 대기 큐에 넣고 잠든다
     *   (3) signal/broadcast를 받으면 깨어나서 뮤텍스를 다시 획득한다 */
    while (b->count == BUFFER_SIZE) {
        pthread_cond_wait(&b->not_full, &b->lock);
    }

    /* assert로 불변식(invariant) 확인:
     * while 루프를 통과했다면 버퍼에 반드시 빈 공간이 있어야 한다.
     * 이 검증이 실패하면 동기화 로직에 버그가 있다는 의미이다. */
    assert(b->count < BUFFER_SIZE);

    /* 원형 버퍼에 데이터 삽입 */
    b->data[b->in] = item;
    b->in = (b->in + 1) % BUFFER_SIZE;   // 다음 삽입 위치로 이동 (순환)
    b->count++;                            // 현재 아이템 수 증가
    b->total_produced++;                   // 총 생산 카운터 증가 (통계용)

    /* pthread_cond_signal: 대기 중인 Consumer 중 하나를 깨운다.
     *
     * 여기서 signal을 쓰는 이유:
     *   - 아이템이 1개 추가되었으므로, Consumer 1개만 깨우면 충분하다
     *   - broadcast를 써도 동작하지만, 불필요하게 여러 Consumer를 깨우면
     *     대부분은 while 루프에서 다시 잠들게 되어 비효율적이다
     *   - signal은 대기 큐에서 하나만 깨우므로 더 효율적이다 */
    pthread_cond_signal(&b->not_empty);

    /* 뮤텍스 해제: 임계 영역 탈출 */
    pthread_mutex_unlock(&b->lock);
}

/* ============================================================
 * get: 버퍼에서 아이템 제거 (Consumer가 호출)
 *
 * 반환값:
 *   - 양수/0: 버퍼에서 꺼낸 아이템 값
 *   - -1: 종료 신호 (모든 생산이 끝나고 버퍼가 비었음)
 *
 * 동작 순서:
 *   1. mutex 획득
 *   2. 버퍼가 비어있고 생산이 끝나지 않았으면 대기
 *   3. 생산이 끝나고 버퍼도 비었으면 -1 반환 (종료)
 *   4. 아이템 제거 및 통계 갱신
 *   5. 대기 중인 Producer 깨움
 *   6. mutex 해제
 * ============================================================ */
int bbuf_get(bbuf_t *b) {
    int item;

    /* 뮤텍스 획득: 임계 영역 진입 */
    pthread_mutex_lock(&b->lock);

    /* [핵심] 대기 조건에 done 플래그 포함
     *
     * 단순히 count == 0만 확인하면, 모든 Producer가 종료된 후에도
     * Consumer가 영원히 대기하게 된다.
     * !b->done 조건을 추가하여, 생산이 끝났으면 대기하지 않고 빠져나온다.
     *
     * while 루프를 사용하는 이유 (Mesa semantics):
     *   - 여러 Consumer가 동시에 빈 버퍼에서 대기할 수 있다
     *   - Producer의 signal로 깨어나도, 다른 Consumer가 먼저 아이템을 가져갈 수 있다
     *   - while로 조건을 재확인하면 이 경쟁 상황에서도 안전하다 */
    while (b->count == 0 && !b->done) {
        /* pthread_cond_wait: 뮤텍스를 해제하고 not_empty 대기 큐에서 잠든다.
         * Producer가 signal을 보내거나, main이 broadcast를 보내면 깨어난다. */
        pthread_cond_wait(&b->not_empty, &b->lock);
    }

    /* 종료 조건 확인: 모든 생산이 끝나고(done==1) 버퍼도 비었으면(count==0)
     * 더 이상 소비할 아이템이 없으므로 -1을 반환하여 종료를 알린다. */
    if (b->count == 0 && b->done) {
        pthread_mutex_unlock(&b->lock);   // 반환 전에 반드시 뮤텍스 해제!
        return -1;
    }

    /* assert로 불변식 확인: 이 지점에 도달했다면 아이템이 있어야 한다 */
    assert(b->count > 0);

    /* 원형 버퍼에서 데이터 제거 */
    item = b->data[b->out];
    b->out = (b->out + 1) % BUFFER_SIZE;  // 다음 제거 위치로 이동 (순환)
    b->count--;                            // 현재 아이템 수 감소
    b->total_consumed++;                   // 총 소비 카운터 증가 (통계용)

    /* pthread_cond_signal: 대기 중인 Producer 중 하나를 깨운다.
     * 공간이 1개 생겼으므로, 1개의 Producer만 깨우면 충분하다. */
    pthread_cond_signal(&b->not_full);

    /* 뮤텍스 해제: 임계 영역 탈출 */
    pthread_mutex_unlock(&b->lock);

    return item;
}

/* ============================================================
 * Producer 스레드 함수
 *
 * 각 Producer는 자신의 ID를 기반으로 고유한 아이템 번호를 생성하여
 * ITEMS_PER_PROD개의 아이템을 생산한다.
 * ============================================================ */
void *producer(void *arg) {
    int id = *(int *)arg;

    for (int i = 0; i < ITEMS_PER_PROD; i++) {
        int item = id * 100 + i;    // 고유한 아이템 번호 (Producer ID * 100 + 순번)
        bbuf_put(&bbuf, item);      // 버퍼에 삽입 (내부에서 동기화 처리)

        printf("  P%d: 생산 [%d] (버퍼 사용량 ~%d/%d)\n",
               id, item, bbuf.count, BUFFER_SIZE);
        /* 주의: bbuf.count를 mutex 없이 읽고 있다. 이는 디버깅 출력 목적이므로
         * 정확한 값이 아닐 수 있지만, 프로그램 동작에는 영향 없다. */

        /* 생산 속도 시뮬레이션: 랜덤한 시간만큼 대기 */
        usleep((unsigned)(rand() % 50000));
    }

    printf("  P%d: === 생산 완료 ===\n", id);
    return NULL;
}

/* ============================================================
 * Consumer 스레드 함수
 *
 * 무한 루프로 버퍼에서 아이템을 계속 꺼내 소비한다.
 * bbuf_get이 -1을 반환하면 (생산이 모두 끝나고 버퍼가 비었으면) 종료한다.
 *
 * 각 Consumer가 정확히 몇 개를 소비할지는 미리 알 수 없다.
 * 실행 순서와 스케줄링에 따라 달라지며, 이것이 다중 Consumer의 특징이다.
 * ============================================================ */
void *consumer(void *arg) {
    int id = *(int *)arg;
    int consumed = 0;   // 이 Consumer가 소비한 아이템 수

    while (1) {
        int item = bbuf_get(&bbuf);   // 버퍼에서 아이템 꺼냄
        if (item == -1) {
            break;  // 종료 신호: 더 이상 소비할 아이템이 없음
        }

        consumed++;
        printf("  C%d: 소비 [%d] (버퍼 사용량 ~%d/%d)\n",
               id, item, bbuf.count, BUFFER_SIZE);

        /* 소비 속도 시뮬레이션: 소비가 생산보다 약간 느리게 설정 */
        usleep((unsigned)(rand() % 80000));
    }

    printf("  C%d: === 소비 완료 (총 %d개) ===\n", id, consumed);
    return NULL;
}

/* ============================================================
 * main: 프로그램 진입점
 *
 * 전체 흐름:
 *   1. 버퍼 초기화
 *   2. Producer 스레드 N개 생성
 *   3. Consumer 스레드 M개 생성
 *   4. 모든 Producer 종료 대기 (pthread_join)
 *   5. done 플래그 설정 + broadcast로 모든 Consumer 깨움
 *   6. 모든 Consumer 종료 대기 (pthread_join)
 *   7. 결과 검증 및 자원 정리
 * ============================================================ */
int main(void) {
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    int       prod_ids[NUM_PRODUCERS];
    int       cons_ids[NUM_CONSUMERS];

    int total_items = NUM_PRODUCERS * ITEMS_PER_PROD;

    printf("=== Bounded Buffer: 다중 Producer/Consumer ===\n");
    printf("버퍼 크기: %d\n", BUFFER_SIZE);
    printf("Producer %d개 x %d아이템 = 총 %d아이템\n",
           NUM_PRODUCERS, ITEMS_PER_PROD, total_items);
    printf("Consumer %d개\n\n", NUM_CONSUMERS);

    srand((unsigned)time(NULL));   // 난수 시드 초기화 (시간 기반)
    bbuf_init(&bbuf);              // 버퍼 및 동기화 객체 초기화

    /* Producer 스레드 생성:
     * 각 스레드에 고유한 ID를 인자로 전달한다.
     * prod_ids 배열을 사용하여 각 스레드가 자신만의 ID 주소를 갖도록 한다.
     * (지역 변수의 주소를 전달하면 값이 변경될 수 있으므로 배열 사용) */
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        prod_ids[i] = i;
        pthread_create(&producers[i], NULL, producer, &prod_ids[i]);
    }

    /* Consumer 스레드 생성: Producer와 동일한 방식 */
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        cons_ids[i] = i;
        pthread_create(&consumers[i], NULL, consumer, &cons_ids[i]);
    }

    /* 모든 Producer가 종료될 때까지 대기:
     * pthread_join은 해당 스레드가 return하거나 pthread_exit할 때까지 블로킹된다. */
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }

    /* [핵심] 종료 처리: 모든 생산이 끝났음을 Consumer에게 알림
     *
     * 이 과정에서 mutex를 사용하는 이유:
     *   - done 플래그를 설정하는 것도 공유 자원 접근이므로 mutex 보호 필요
     *   - Consumer가 while(count==0 && !done) 루프에서 대기 중일 수 있으므로
     *     done 설정과 broadcast가 원자적으로 이루어져야 한다
     *
     * broadcast를 사용하는 이유:
     *   - 여러 Consumer가 동시에 not_empty에서 대기 중일 수 있다
     *   - signal을 쓰면 하나의 Consumer만 깨어나고, 나머지는 영원히 대기한다
     *   - broadcast로 모든 Consumer를 깨워야 각각 done 플래그를 확인하고 종료한다
     *   - 이것이 signal과 broadcast의 결정적인 차이를 보여주는 좋은 예시이다 */
    pthread_mutex_lock(&bbuf.lock);
    bbuf.done = 1;  // 더 이상 생산할 아이템이 없음을 표시
    pthread_cond_broadcast(&bbuf.not_empty);  // 모든 대기 중인 Consumer를 깨움
    pthread_mutex_unlock(&bbuf.lock);

    /* 모든 Consumer가 종료될 때까지 대기 */
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], NULL);
    }

    /* 결과 검증: 생산된 아이템 수와 소비된 아이템 수가 일치해야 한다.
     * 동기화가 올바르게 구현되었다면, 하나의 아이템도 유실되거나 중복 소비되지 않는다. */
    printf("\n=== 결과 ===\n");
    printf("총 생산: %d\n", bbuf.total_produced);
    printf("총 소비: %d\n", bbuf.total_consumed);

    if (bbuf.total_produced == total_items &&
        bbuf.total_consumed == total_items) {
        printf("검증 성공: 모든 아이템이 정확히 생산/소비되었습니다.\n");
    } else {
        printf("검증 실패: 아이템 수가 일치하지 않습니다!\n");
        return 1;
    }

    /* 동기화 객체 자원 정리 */
    bbuf_destroy(&bbuf);

    printf("\n=== 프로그램 정상 종료 ===\n");
    return 0;
}
