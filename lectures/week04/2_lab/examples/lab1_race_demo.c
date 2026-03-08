/*
 * race_demo.c - 경쟁 조건(Race Condition) 시연 프로그램
 *
 * [핵심 개념]
 * 경쟁 조건(Race Condition)이란 두 개 이상의 스레드가 공유 자원에
 * 동시에 접근할 때, 실행 순서에 따라 결과가 달라지는 현상이다.
 *
 * 이 프로그램에서는 여러 스레드가 동기화(synchronization) 없이
 * 공유 카운터를 증가시킨다. counter++ 연산은 원자적(atomic)이지 않기
 * 때문에 최종 결과는 기대값보다 작게 나온다.
 *
 * counter++ 는 실제로 다음 3단계로 실행된다:
 *   1. 메모리에서 counter 값을 레지스터로 읽기 (LOAD)
 *   2. 레지스터 값에 1을 더하기 (ADD)
 *   3. 레지스터 값을 메모리에 저장하기 (STORE)
 * 이 3단계 사이에 다른 스레드가 끼어들면 증가 연산이 손실된다.
 *
 * 컴파일: gcc -Wall -pthread -o race_demo race_demo.c
 * 실행:   ./race_demo [스레드_수] [스레드당_증가_횟수]
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* 기본 스레드 수 */
#define DEFAULT_THREADS     4
/* 각 스레드가 수행할 기본 증가 횟수 */
#define DEFAULT_INCREMENTS  1000000

/*
 * 공유 카운터 - 어떤 동기화 장치로도 보호되지 않음
 * volatile 키워드는 컴파일러 최적화로 인해 메모리 접근이
 * 생략되는 것을 방지하지만, 원자성(atomicity)을 보장하지는 않는다.
 */
volatile int counter = 0;

/* 각 스레드가 수행할 증가 횟수 (모든 스레드가 공유) */
int increments_per_thread;

/*
 * 스레드가 실행할 함수
 * arg: 스레드 ID가 저장된 int 포인터
 */
void *increment(void *arg)
{
    /* void 포인터를 int 포인터로 캐스팅한 후 역참조하여 스레드 ID 획득 */
    int tid = *(int *)arg;
    for (int i = 0; i < increments_per_thread; i++) {
        /*
         * [경쟁 조건 발생 지점]
         * counter++ 는 원자적(atomic) 연산이 아니다.
         * 어셈블리 수준에서 대략 다음과 같이 변환된다:
         *   1. LOAD  counter -> 레지스터   (메모리에서 값 읽기)
         *   2. ADD   1       -> 레지스터   (레지스터 값 +1)
         *   3. STORE 레지스터 -> counter   (메모리에 값 쓰기)
         *
         * 예시: 두 스레드 A, B가 동시에 counter=100을 읽으면
         *   A: LOAD 100, ADD -> 101, STORE 101
         *   B: LOAD 100, ADD -> 101, STORE 101
         *   결과: counter=101 (102가 아닌 101, 증가 1회 손실)
         */
        counter++;
    }
    printf("[Thread %d] finished %d increments\n", tid, increments_per_thread);
    return NULL;
}

int main(int argc, char *argv[])
{
    /* 명령행 인자로 스레드 수와 증가 횟수를 설정 (없으면 기본값 사용) */
    int nthreads = (argc > 1) ? atoi(argv[1]) : DEFAULT_THREADS;
    increments_per_thread = (argc > 2) ? atoi(argv[2]) : DEFAULT_INCREMENTS;

    /* 유효하지 않은 입력값 검증 */
    if (nthreads <= 0 || increments_per_thread <= 0) {
        fprintf(stderr, "Usage: %s [num_threads] [increments_per_thread]\n", argv[0]);
        return 1;
    }

    /* 기대 결과값 = 스레드 수 x 스레드당 증가 횟수 */
    int expected = nthreads * increments_per_thread;

    printf("=== Race Condition Demo ===\n");
    printf("Threads: %d, Increments per thread: %d\n", nthreads, increments_per_thread);
    printf("Expected final counter: %d\n\n", expected);

    /* 스레드 핸들과 ID 배열을 동적 할당 */
    pthread_t *threads = malloc(sizeof(pthread_t) * nthreads);
    int *tids = malloc(sizeof(int) * nthreads);

    /* 스레드 생성 루프 */
    for (int i = 0; i < nthreads; i++) {
        tids[i] = i;
        /*
         * pthread_create: 새로운 스레드를 생성한다.
         *   - &threads[i]: 생성된 스레드의 핸들을 저장할 위치
         *   - NULL: 기본 스레드 속성 사용
         *   - increment: 스레드가 실행할 함수
         *   - &tids[i]: 스레드 함수에 전달할 인자 (스레드 ID)
         */
        pthread_create(&threads[i], NULL, increment, &tids[i]);
    }

    /* 모든 스레드가 종료될 때까지 대기 */
    for (int i = 0; i < nthreads; i++) {
        /*
         * pthread_join: 해당 스레드가 종료될 때까지 메인 스레드를 블로킹한다.
         *   - threads[i]: 대기할 스레드의 핸들
         *   - NULL: 스레드의 반환값을 받지 않음
         */
        pthread_join(threads[i], NULL);
    }

    /* 결과 비교: 경쟁 조건으로 인해 actual < expected 가 되는 것을 확인 */
    printf("\nExpected: %d\n", expected);
    printf("Actual:   %d\n", counter);

    if (counter != expected) {
        /* 경쟁 조건으로 인해 손실된 증가 횟수를 출력 */
        printf("MISMATCH! Lost %d increments due to race condition.\n",
               expected - counter);
    } else {
        printf("No race detected this run. Try increasing increments or threads.\n");
    }

    /* 동적 할당한 메모리 해제 */
    free(threads);
    free(tids);
    return 0;
}
