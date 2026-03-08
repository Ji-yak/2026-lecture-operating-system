/*
 * spinlock_impl.c - 스핀락(Spinlock) 직접 구현
 *
 * [핵심 개념]
 * 스핀락(Spinlock)은 잠금을 획득할 때까지 반복문(busy-wait)으로
 * 계속 시도하는 동기화 방식이다. 뮤텍스와 달리 스레드를 재우지 않고
 * CPU를 소모하며 대기하므로, 잠금 보유 시간이 매우 짧을 때 효율적이다.
 *
 * 이 프로그램은 xv6 커널의 acquire()/release() 함수와 동일한
 * 원자적(atomic) 연산인 __sync_lock_test_and_set을 사용하여
 * 스핀락을 구현한다.
 *
 * xv6 커널 코드 (kernel/spinlock.c):
 *   while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
 *     ;
 *
 * 스핀락 vs 뮤텍스 비교:
 *   - 스핀락: busy-wait (CPU 소모), 짧은 임계 영역에 적합, 커널에서 주로 사용
 *   - 뮤텍스: sleep-wait (CPU 양보), 긴 임계 영역에 적합, 유저 공간에서 주로 사용
 *
 * 컴파일: gcc -Wall -pthread -o spinlock_impl spinlock_impl.c
 * 실행:   ./spinlock_impl [스레드_수] [스레드당_증가_횟수]
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* 기본 스레드 수 */
#define DEFAULT_THREADS     4
/* 각 스레드가 수행할 기본 증가 횟수 */
#define DEFAULT_INCREMENTS  1000000

/* ---- 스핀락 구현부 ---- */

struct spinlock {
    /*
     * locked 필드: 잠금 상태를 나타내는 변수
     *   - 0: 잠금 해제 상태 (unlocked)
     *   - 1: 잠금 보유 상태 (locked)
     * volatile: 컴파일러가 이 변수의 접근을 최적화로 생략하지 않도록 보장
     */
    volatile int locked;
};

/*
 * spinlock_init: 스핀락을 초기화한다.
 * 잠금 해제 상태(0)로 설정한다.
 */
void spinlock_init(struct spinlock *lk)
{
    lk->locked = 0;
}

/*
 * spinlock_acquire: 스핀락을 획득한다 (xv6의 acquire 함수에 해당).
 *
 * __sync_lock_test_and_set(&lk->locked, 1)의 동작:
 *   이 함수는 하나의 원자적(atomic) 연산으로 다음을 수행한다:
 *   1. lk->locked의 현재 값을 읽는다 (이전 값)
 *   2. lk->locked를 1로 설정한다
 *   3. 이전 값을 반환한다
 *
 *   이전 값이 0이었다면: 잠금이 해제 상태였으므로 획득 성공
 *   이전 값이 1이었다면: 다른 스레드가 보유 중이므로 계속 반복(spin)
 *
 *   이 연산이 원자적이므로 두 스레드가 동시에 잠금을 획득하는 것은 불가능하다.
 *   (하드웨어가 LOAD와 STORE를 하나의 명령어로 보장)
 */
void spinlock_acquire(struct spinlock *lk)
{
    /* 잠금을 획득할 때까지 무한 반복 (busy-wait, spin) */
    while (__sync_lock_test_and_set(&lk->locked, 1) != 0) {
        /*
         * 여기서 계속 회전(spin)하며 대기한다.
         * CPU 자원을 소모하지만, 잠금이 곧 해제될 것으로
         * 예상되는 짧은 임계 영역에서는 컨텍스트 스위치
         * 비용보다 효율적이다.
         *
         * 실제 OS 커널(xv6 등)에서는 스핀 전에 인터럽트를
         * 비활성화하여 잠금 보유 중 선점(preemption)을 방지한다.
         */
    }
    /*
     * __sync_synchronize: 메모리 배리어(memory barrier/fence)를 삽입한다.
     *
     * CPU와 컴파일러는 성능 최적화를 위해 메모리 접근 순서를
     * 바꿀 수 있다(재정렬, reordering). 메모리 배리어는 이 배리어
     * 이전의 모든 메모리 연산이 완료된 후에야 이후 연산이
     * 실행되도록 강제한다.
     *
     * 잠금 획득 후 임계 영역의 읽기/쓰기가 잠금 획득보다
     * 먼저 실행되는 것을 방지한다.
     *
     * 참고: __sync_lock_test_and_set은 이미 acquire 배리어를
     * 포함하지만, __sync_synchronize로 명시적으로 보장한다.
     */
    __sync_synchronize();
}

/*
 * spinlock_release: 스핀락을 해제한다 (xv6의 release 함수에 해당).
 *
 * __sync_lock_release: 원자적으로 lk->locked를 0으로 설정하고
 * release 메모리 배리어를 수행한다.
 */
void spinlock_release(struct spinlock *lk)
{
    /*
     * __sync_synchronize: 임계 영역의 모든 메모리 연산이
     * 잠금 해제 전에 완료되도록 보장한다.
     * 임계 영역 내의 쓰기가 잠금 해제 이후로 지연되면
     * 다른 스레드가 불완전한 데이터를 볼 수 있기 때문이다.
     */
    __sync_synchronize();

    /*
     * __sync_lock_release: 원자적으로 locked를 0으로 설정한다.
     * 단순한 lk->locked = 0 대입과 달리, 메모리 순서를
     * 올바르게 보장하는 release 의미(semantics)를 가진다.
     */
    __sync_lock_release(&lk->locked);
}

/* ---- 스핀락을 사용한 카운터 증가 데모 ---- */

/* 공유 카운터 - 스핀락으로 보호됨 */
int counter = 0;
/* 전역 스핀락 인스턴스 */
struct spinlock lock;
/* 각 스레드가 수행할 증가 횟수 */
int increments_per_thread;

/*
 * 스레드가 실행할 함수
 * 스핀락으로 보호하여 카운터를 안전하게 증가시킨다.
 */
void *increment(void *arg)
{
    int tid = *(int *)arg;
    for (int i = 0; i < increments_per_thread; i++) {
        /* 스핀락 획득: 잠금을 얻을 때까지 busy-wait */
        spinlock_acquire(&lock);

        /* [임계 영역] 한 번에 하나의 스레드만 이 코드를 실행 */
        counter++;

        /* 스핀락 해제: 대기 중인 스레드가 진입 가능 */
        spinlock_release(&lock);
    }
    printf("[Thread %d] finished %d increments\n", tid, increments_per_thread);
    return NULL;
}

int main(int argc, char *argv[])
{
    /* 명령행 인자 파싱 */
    int nthreads = (argc > 1) ? atoi(argv[1]) : DEFAULT_THREADS;
    increments_per_thread = (argc > 2) ? atoi(argv[2]) : DEFAULT_INCREMENTS;

    if (nthreads <= 0 || increments_per_thread <= 0) {
        fprintf(stderr, "Usage: %s [num_threads] [increments_per_thread]\n", argv[0]);
        return 1;
    }

    /* 기대 결과값 계산 */
    int expected = nthreads * increments_per_thread;

    printf("=== Spinlock Implementation Demo ===\n");
    printf("Threads: %d, Increments per thread: %d\n", nthreads, increments_per_thread);
    printf("Expected final counter: %d\n\n", expected);

    /* 스핀락 초기화 (locked = 0) */
    spinlock_init(&lock);

    /* 스레드 핸들과 ID 배열을 동적 할당 */
    pthread_t *threads = malloc(sizeof(pthread_t) * nthreads);
    int *tids = malloc(sizeof(int) * nthreads);

    /* 스레드 생성 */
    for (int i = 0; i < nthreads; i++) {
        tids[i] = i;
        /*
         * pthread_create: 새 스레드를 생성하여 increment 함수를 병렬로 실행한다.
         * 모든 스레드가 동일한 전역 counter와 lock을 공유한다.
         */
        pthread_create(&threads[i], NULL, increment, &tids[i]);
    }

    /* 모든 스레드 종료 대기 */
    for (int i = 0; i < nthreads; i++) {
        /* pthread_join: 각 스레드가 완료될 때까지 메인 스레드를 블로킹한다 */
        pthread_join(threads[i], NULL);
    }

    /* 결과 확인: 스핀락이 올바르게 동작하면 항상 expected와 동일 */
    printf("\nExpected: %d\n", expected);
    printf("Actual:   %d\n", counter);

    if (counter == expected) {
        printf("SUCCESS! Our spinlock works correctly.\n");
    } else {
        printf("ERROR: Spinlock implementation has a bug.\n");
    }

    /* 동적 할당한 메모리 해제 */
    free(threads);
    free(tids);
    return 0;
}
