/*
 * barrier.c - Barrier 구현 (보너스 과제)
 *
 * pthread_barrier를 mutex + condition variable로 직접 구현하세요.
 * Barrier: 모든 스레드가 도착할 때까지 대기, 모두 도착하면 한꺼번에 진행
 *
 * 컴파일: gcc -Wall -pthread -o barrier barrier.c
 * 실행:   ./barrier
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

/* ============================================================
 * Barrier 구조체
 * ============================================================ */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int             count;      // 현재 barrier에 도착한 스레드 수
    int             num_threads;// barrier를 통과하는 데 필요한 스레드 수
    int             round;      // 현재 라운드 (재사용 가능한 barrier를 위해)
} my_barrier_t;

/* ============================================================
 * barrier_init: barrier를 초기화한다
 *
 * 매개변수:
 *   b           - barrier 구조체 포인터
 *   num_threads - barrier를 통과하는 데 필요한 스레드 수
 * ============================================================ */
void barrier_init(my_barrier_t *b, int num_threads) {
    // TODO: mutex, cond, count, num_threads, round를 초기화하세요.
    // --- 여기에 코드를 작성하세요 ---
}

/* ============================================================
 * barrier_wait: barrier에서 대기한다
 *
 * 모든 스레드가 이 함수를 호출할 때까지 대기한다.
 * 마지막 스레드가 도착하면 모든 스레드를 깨운다.
 *
 * 중요: barrier는 재사용 가능해야 한다! (여러 라운드)
 *       라운드 번호를 사용하여 다음 라운드의 스레드가
 *       이전 라운드의 signal에 의해 잘못 깨어나는 것을 방지한다.
 * ============================================================ */
void barrier_wait(my_barrier_t *b) {
    // TODO: barrier 구현
    //
    // 구현 단계:
    //   1. mutex 획득
    //   2. 현재 라운드 번호를 지역 변수에 저장
    //   3. count 증가
    //   4. if (마지막 스레드가 도착):
    //        - count를 0으로 리셋
    //        - round를 1 증가 (다음 라운드)
    //        - broadcast로 모든 대기 스레드를 깨움
    //      else:
    //        - while (라운드가 아직 같으면) cond_wait
    //   5. mutex 해제
    //
    // 힌트: 왜 round 변수가 필요한가?
    //   barrier_wait를 빠져나온 스레드가 다시 barrier_wait에 도착하면
    //   count가 아직 이전 라운드의 값일 수 있다.
    //   round 번호로 구분하지 않으면 버그 발생!

    // --- 여기에 코드를 작성하세요 ---
}

/* ============================================================
 * barrier_destroy: barrier의 자원을 해제한다
 * ============================================================ */
void barrier_destroy(my_barrier_t *b) {
    // TODO: mutex와 cond를 destroy하세요.
    // --- 여기에 코드를 작성하세요 ---
}

/* ============================================================
 * 테스트 코드 (수정하지 마세요)
 * ============================================================ */
#define NUM_THREADS 5
#define NUM_ROUNDS  3

static my_barrier_t barrier;
static int shared_counter = 0;
static pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;

void *worker(void *arg) {
    int id = *(int *)arg;

    for (int round = 0; round < NUM_ROUNDS; round++) {
        /* Phase 1: 각 스레드가 작업 수행 */
        printf("  스레드 %d: 라운드 %d 작업 시작\n", id, round);
        usleep((unsigned)(rand() % 100000));

        pthread_mutex_lock(&counter_lock);
        shared_counter++;
        pthread_mutex_unlock(&counter_lock);

        /* Barrier: 모든 스레드가 Phase 1을 완료할 때까지 대기 */
        barrier_wait(&barrier);

        /* Phase 2: 검증 - 모든 스레드의 작업이 반영되었는지 확인 */
        if (id == 0) {
            int expected = NUM_THREADS * (round + 1);
            printf("  [검증] 라운드 %d: counter = %d (기대값: %d) %s\n",
                   round, shared_counter, expected,
                   shared_counter == expected ? "OK" : "FAIL");
            assert(shared_counter == expected);
        }

        /* 다음 라운드 전에 모든 스레드가 검증을 마칠 때까지 대기 */
        barrier_wait(&barrier);
    }

    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    int       ids[NUM_THREADS];

    printf("=== Barrier 테스트 ===\n");
    printf("스레드 수: %d, 라운드 수: %d\n\n", NUM_THREADS, NUM_ROUNDS);

    barrier_init(&barrier, NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, worker, &ids[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    barrier_destroy(&barrier);

    printf("\n=== 모든 테스트 통과 ===\n");
    return 0;
}
