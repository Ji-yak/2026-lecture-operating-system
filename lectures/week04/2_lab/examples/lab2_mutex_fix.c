/*
 * mutex_fix.c - 뮤텍스(Mutex)를 이용한 경쟁 조건 해결
 *
 * [핵심 개념]
 * 뮤텍스(Mutex, Mutual Exclusion)는 상호 배제를 구현하는 동기화 도구이다.
 * 한 번에 하나의 스레드만 임계 영역(critical section)에 진입할 수 있도록
 * 보장하여 경쟁 조건을 방지한다.
 *
 * 이 프로그램은 race_demo.c와 동일한 시나리오에서 뮤텍스를 추가하여
 * 공유 카운터를 안전하게 보호한다. 결과가 항상 정확하게 나온다.
 *
 * 동작 원리:
 *   lock()   -> 다른 스레드가 잠금을 보유 중이면 대기 (블로킹)
 *   counter++ -> 임계 영역: 한 스레드만 실행 가능
 *   unlock() -> 잠금 해제, 대기 중인 스레드가 진입 가능
 *
 * 컴파일: gcc -Wall -pthread -o mutex_fix mutex_fix.c
 * 실행:   ./mutex_fix [스레드_수] [스레드당_증가_횟수]
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* 기본 스레드 수 */
#define DEFAULT_THREADS     4
/* 각 스레드가 수행할 기본 증가 횟수 */
#define DEFAULT_INCREMENTS  1000000

/* 공유 카운터 - 뮤텍스로 보호됨 (volatile 불필요: 뮤텍스가 메모리 가시성 보장) */
int counter = 0;

/*
 * 카운터를 보호하는 뮤텍스
 * PTHREAD_MUTEX_INITIALIZER: 정적 초기화 매크로
 *   - pthread_mutex_init() 호출 없이 뮤텍스를 초기화할 수 있다.
 *   - 전역/정적 변수에만 사용 가능하다.
 */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* 각 스레드가 수행할 증가 횟수 */
int increments_per_thread;

/*
 * 스레드가 실행할 함수
 * arg: 스레드 ID가 저장된 int 포인터
 */
void *increment(void *arg)
{
    int tid = *(int *)arg;
    for (int i = 0; i < increments_per_thread; i++) {
        /*
         * pthread_mutex_lock: 뮤텍스 잠금을 획득한다.
         *   - 잠금이 해제된 상태이면: 즉시 획득하고 진행
         *   - 다른 스레드가 보유 중이면: 해제될 때까지 블로킹(대기)
         *   - 이 함수 호출부터 unlock까지가 임계 영역(critical section)이다.
         */
        pthread_mutex_lock(&lock);

        /*
         * [임계 영역 시작]
         * 이 영역은 한 번에 하나의 스레드만 실행할 수 있다.
         * 뮤텍스가 LOAD-ADD-STORE 전체 과정을 보호하므로
         * 다른 스레드가 중간에 끼어들 수 없다.
         */
        counter++;
        /* [임계 영역 끝] */

        /*
         * pthread_mutex_unlock: 뮤텍스 잠금을 해제한다.
         *   - 대기 중인 다른 스레드가 있으면, 그 중 하나가 깨어나서 잠금을 획득한다.
         *   - 반드시 잠금을 획득한 스레드가 해제해야 한다.
         */
        pthread_mutex_unlock(&lock);
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

    printf("=== Mutex Fix Demo ===\n");
    printf("Threads: %d, Increments per thread: %d\n", nthreads, increments_per_thread);
    printf("Expected final counter: %d\n\n", expected);

    /* 스레드 핸들과 ID 배열을 동적 할당 */
    pthread_t *threads = malloc(sizeof(pthread_t) * nthreads);
    int *tids = malloc(sizeof(int) * nthreads);

    /* 스레드 생성: 각 스레드는 increment 함수를 실행 */
    for (int i = 0; i < nthreads; i++) {
        tids[i] = i;
        /*
         * pthread_create: 새 스레드를 생성하여 increment 함수를 실행시킨다.
         * 생성된 스레드는 즉시 실행을 시작한다.
         */
        pthread_create(&threads[i], NULL, increment, &tids[i]);
    }

    /* 모든 스레드의 종료를 대기 */
    for (int i = 0; i < nthreads; i++) {
        /*
         * pthread_join: 해당 스레드가 완료될 때까지 메인 스레드를 블로킹한다.
         * 모든 스레드가 종료된 후에야 결과를 확인할 수 있다.
         */
        pthread_join(threads[i], NULL);
    }

    /* 결과 확인: 뮤텍스 덕분에 항상 expected와 동일해야 한다 */
    printf("\nExpected: %d\n", expected);
    printf("Actual:   %d\n", counter);

    if (counter == expected) {
        printf("SUCCESS! Mutex prevented the race condition.\n");
    } else {
        printf("ERROR: This should not happen with correct mutex usage.\n");
    }

    /*
     * pthread_mutex_destroy: 뮤텍스 자원을 해제한다.
     *   - 더 이상 사용하지 않는 뮤텍스는 반드시 해제해야 한다.
     *   - 잠금이 해제된 상태에서만 호출해야 한다.
     *   - PTHREAD_MUTEX_INITIALIZER로 초기화한 경우에도 호출 권장.
     */
    pthread_mutex_destroy(&lock);

    /* 동적 할당한 메모리 해제 */
    free(threads);
    free(tids);
    return 0;
}
