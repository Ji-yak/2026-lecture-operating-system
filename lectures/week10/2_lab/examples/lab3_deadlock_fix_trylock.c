/*
 * lab3_deadlock_fix_trylock.c - pthread_mutex_trylock으로 Deadlock 회피
 *
 * [개요]
 *   trylock + back-off 전략을 사용하여 deadlock을 회피하는 프로그램이다.
 *   lock 획득에 실패하면 이미 보유 중인 lock을 모두 해제(back-off)하고
 *   잠시 후 다시 시도한다.
 *   lab1_deadlock_demo.c에서 발생한 deadlock을 해결하는 두 번째 방법이다.
 *
 * [Coffman 조건 중 어떤 것을 깨뜨리는가?]
 *   2번 조건인 "Hold and Wait (점유 대기)"를 깨뜨린다.
 *   - 일반적인 pthread_mutex_lock()은 이미 lock을 보유한 채로
 *     다른 lock을 blocking으로 기다린다 (Hold and Wait 성립).
 *   - trylock은 lock을 즉시 획득하지 못하면 기다리지 않고 실패를 반환한다.
 *   - 실패 시 이미 보유한 lock을 모두 해제하므로
 *     "자원을 보유한 채 대기"하는 상황이 발생하지 않는다.
 *
 * [trylock vs lock 비교]
 *   pthread_mutex_lock():
 *     - lock을 획득할 수 있을 때까지 blocking (스레드 중단)
 *     - 반드시 성공하지만, deadlock 위험이 있음
 *   pthread_mutex_trylock():
 *     - lock을 즉시 시도하고, 실패하면 바로 EBUSY 반환 (non-blocking)
 *     - 실패할 수 있지만, deadlock을 회피할 수 있음
 *     - 반환값: 0이면 성공, EBUSY이면 다른 스레드가 이미 보유 중
 *
 * [back-off 전략]
 *   1. 첫 번째 lock은 일반 lock()으로 획득한다.
 *   2. 두 번째 lock은 trylock()으로 시도한다.
 *   3. trylock 실패 시:
 *      a. 보유 중인 첫 번째 lock을 해제한다 (back-off)
 *      b. 랜덤 시간 동안 대기한다 (경쟁 완화)
 *      c. 처음부터 다시 시도한다
 *
 * [livelock 주의]
 *   trylock + back-off 방식의 단점: livelock이 발생할 수 있다.
 *   livelock: 두 스레드가 계속 lock을 잡았다 놓았다를 반복하면서
 *   실제 작업은 전혀 진행하지 못하는 상태.
 *   랜덤 대기 시간을 사용하면 livelock 확률을 크게 줄일 수 있다.
 *
 * [컴파일]
 *   gcc -Wall -pthread -o lab3_deadlock_fix_trylock lab3_deadlock_fix_trylock.c
 *
 * [실행]
 *   ./lab3_deadlock_fix_trylock
 */

#include <stdio.h>      /* printf(), perror() */
#include <stdlib.h>     /* exit(), srand(), rand() */
#include <pthread.h>    /* pthread_mutex_t, pthread_mutex_trylock(), pthread_create() 등 */
#include <unistd.h>     /* usleep() */

/* 두 개의 mutex를 정적으로 초기화한다.
 * 이 두 mutex를 서로 반대 순서로 잡지만,
 * trylock + back-off 전략으로 deadlock을 회피한다. */
pthread_mutex_t mutex_A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_B = PTHREAD_MUTEX_INITIALIZER;

/*
 * thread1_func: Thread 1의 실행 함수
 *
 * 잠금 순서: mutex_A (lock) -> mutex_B (trylock)
 * trylock이 실패하면 mutex_A를 해제하고 재시도한다 (back-off).
 */
void *thread1_func(void *arg)
{
    (void)arg;  /* 사용하지 않는 매개변수 경고 억제 */
    int success = 0;   /* 두 lock 모두 획득 성공 여부 */
    int attempts = 0;  /* 시도 횟수 카운터 */

    /* back-off 루프: 두 lock을 모두 획득할 때까지 반복 시도 */
    while (!success) {
        attempts++;

        printf("[Thread 1] 시도 %d: mutex_A 잠금...\n", attempts);
        /* pthread_mutex_lock(): 첫 번째 lock은 일반 blocking lock으로 획득한다.
         * 다른 스레드가 보유 중이면 해제될 때까지 대기한다. */
        pthread_mutex_lock(&mutex_A);
        printf("[Thread 1] 시도 %d: mutex_A 잠금 성공\n", attempts);

        /* usleep(): 10ms 대기하여 Thread 2가 mutex_B를 잡을 기회를 준다.
         * 이렇게 하면 trylock 실패(경쟁 상황)를 관찰할 수 있다. */
        usleep(10000);  /* 10ms */

        printf("[Thread 1] 시도 %d: mutex_B trylock 시도...\n", attempts);
        /* pthread_mutex_trylock(): 두 번째 lock은 non-blocking으로 시도한다.
         * 반환값 0: lock 획득 성공
         * 반환값 EBUSY: 다른 스레드가 이미 보유 중 (실패)
         * 일반 lock()과 달리 blocking하지 않으므로 deadlock이 발생하지 않는다. */
        if (pthread_mutex_trylock(&mutex_B) == 0) {
            /* trylock 성공: 두 lock을 모두 획득했다 */
            printf("[Thread 1] 시도 %d: mutex_B trylock 성공!\n", attempts);
            success = 1;

            /* 임계 영역 (critical section): 두 lock을 모두 보유한 상태에서 작업 수행 */
            printf("[Thread 1] === 임계 영역 진입 (A, B 모두 보유) ===\n");
            usleep(5000);  /* 5ms 동안 작업 시뮬레이션 */

            /* pthread_mutex_unlock(): 사용이 끝난 mutex를 해제한다.
             * 대기 중인 스레드가 있으면 그 중 하나가 깨어난다. */
            pthread_mutex_unlock(&mutex_B);
            printf("[Thread 1] mutex_B 해제\n");
            pthread_mutex_unlock(&mutex_A);
            printf("[Thread 1] mutex_A 해제\n");
        } else {
            /* trylock 실패: back-off 수행
             * 핵심: 이미 보유한 mutex_A를 즉시 해제한다.
             * 이렇게 하면 "자원을 보유한 채 대기"(Hold and Wait)가 깨진다.
             * Thread 2가 mutex_A를 획득할 수 있게 되어 진행이 가능해진다. */
            printf("[Thread 1] 시도 %d: mutex_B trylock 실패! -> back-off\n", attempts);
            pthread_mutex_unlock(&mutex_A);
            printf("[Thread 1] 시도 %d: mutex_A 해제 (back-off)\n", attempts);

            /* usleep(): 랜덤 시간(0~50ms) 동안 대기 후 재시도한다.
             * 랜덤 대기가 중요한 이유:
             *   - 고정 대기 시간을 사용하면 두 스레드가 동시에 깨어나
             *     다시 같은 순서로 lock을 시도하여 livelock이 발생할 수 있다.
             *   - 랜덤 대기 시간은 두 스레드의 실행 타이밍을 어긋나게 하여
             *     한쪽이 먼저 두 lock을 모두 획득할 확률을 높인다.
             * 이 개념은 네트워크의 Ethernet CSMA/CD 프로토콜의
             * exponential back-off와 유사하다. */
            usleep((unsigned)(rand() % 50000));  /* 0~50ms 랜덤 대기 */
        }
    }

    printf("[Thread 1] 완료 (총 %d회 시도)\n\n", attempts);
    return NULL;
}

/*
 * thread2_func: Thread 2의 실행 함수
 *
 * 잠금 순서: mutex_B (lock) -> mutex_A (trylock)
 * Thread 1과 반대 순서이지만, trylock + back-off로 deadlock을 회피한다.
 * trylock이 실패하면 mutex_B를 해제하고 재시도한다 (back-off).
 */
void *thread2_func(void *arg)
{
    (void)arg;  /* 사용하지 않는 매개변수 경고 억제 */
    int success = 0;   /* 두 lock 모두 획득 성공 여부 */
    int attempts = 0;  /* 시도 횟수 카운터 */

    /* back-off 루프: 두 lock을 모두 획득할 때까지 반복 시도 */
    while (!success) {
        attempts++;

        printf("[Thread 2] 시도 %d: mutex_B 잠금...\n", attempts);
        /* pthread_mutex_lock(): 첫 번째 lock(mutex_B)을 blocking으로 획득한다.
         * Thread 1과 반대 순서이지만, 두 번째 lock을 trylock으로 시도하므로
         * 순서가 달라도 deadlock이 발생하지 않는다. */
        pthread_mutex_lock(&mutex_B);
        printf("[Thread 2] 시도 %d: mutex_B 잠금 성공\n", attempts);

        usleep(10000);  /* 10ms 대기 - 경쟁 상황 유도 */

        printf("[Thread 2] 시도 %d: mutex_A trylock 시도...\n", attempts);
        /* pthread_mutex_trylock(): mutex_A를 non-blocking으로 시도한다.
         * Thread 1이 이미 mutex_A를 보유 중이면 즉시 EBUSY를 반환한다.
         * 이때 blocking하지 않으므로 deadlock 고리가 형성되지 않는다. */
        if (pthread_mutex_trylock(&mutex_A) == 0) {
            /* trylock 성공: 두 lock을 모두 획득했다 */
            printf("[Thread 2] 시도 %d: mutex_A trylock 성공!\n", attempts);
            success = 1;

            /* 임계 영역 (critical section) */
            printf("[Thread 2] === 임계 영역 진입 (A, B 모두 보유) ===\n");
            usleep(5000);  /* 5ms 동안 작업 시뮬레이션 */

            /* mutex 해제 */
            pthread_mutex_unlock(&mutex_A);
            printf("[Thread 2] mutex_A 해제\n");
            pthread_mutex_unlock(&mutex_B);
            printf("[Thread 2] mutex_B 해제\n");
        } else {
            /* trylock 실패: back-off 수행
             * 보유 중인 mutex_B를 해제하여 Hold and Wait 조건을 깨뜨린다.
             * Thread 1이 mutex_B를 획득할 수 있게 된다. */
            printf("[Thread 2] 시도 %d: mutex_A trylock 실패! -> back-off\n", attempts);
            pthread_mutex_unlock(&mutex_B);
            printf("[Thread 2] 시도 %d: mutex_B 해제 (back-off)\n", attempts);

            /* 랜덤 대기(0~50ms) 후 재시도 - livelock 방지 */
            usleep((unsigned)(rand() % 50000));  /* 0~50ms 랜덤 대기 */
        }
    }

    printf("[Thread 2] 완료 (총 %d회 시도)\n\n", attempts);
    return NULL;
}

int main(void)
{
    pthread_t t1, t2;  /* 스레드 식별자 */

    /* srand(): 난수 생성기의 시드를 설정한다.
     * back-off 대기 시간을 랜덤하게 하기 위해 사용한다.
     * 고정 시드(42)를 사용하여 실행 결과를 재현 가능하게 한다.
     * 실제 시스템에서는 time(NULL) 등을 시드로 사용한다. */
    srand(42);

    printf("==============================================\n");
    printf("  trylock + Back-off으로 Deadlock 회피 데모\n");
    printf("==============================================\n");
    printf("\n");
    printf("Thread 1: mutex_A -> trylock(mutex_B)\n");
    printf("Thread 2: mutex_B -> trylock(mutex_A)\n");
    printf("\n");
    printf("두 스레드가 반대 순서로 lock을 시도하지만,\n");
    printf("trylock이 실패하면 보유 중인 lock을 해제하고\n");
    printf("잠시 후 다시 시도합니다 (back-off 전략).\n");
    printf("\n");
    printf("----------------------------------------------\n\n");

    /* pthread_create(): 두 스레드를 생성하여 동시에 실행한다.
     * 두 스레드가 반대 순서로 lock을 시도하지만,
     * trylock + back-off 덕분에 deadlock 없이 모두 완료된다. */
    if (pthread_create(&t1, NULL, thread1_func, NULL) != 0) {
        perror("pthread_create (t1)");  /* 시스템 에러 메시지 출력 */
        exit(1);
    }
    if (pthread_create(&t2, NULL, thread2_func, NULL) != 0) {
        perror("pthread_create (t2)");
        exit(1);
    }

    /* pthread_join(): 각 스레드가 종료될 때까지 main 스레드가 대기한다.
     * trylock + back-off 전략 덕분에 두 스레드 모두 정상 종료된다. */
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("----------------------------------------------\n");
    printf("두 스레드 모두 정상 종료! Deadlock이 발생하지 않았습니다.\n");
    printf("\n");
    printf("[핵심 원리]\n");
    printf("  trylock은 lock을 즉시 획득하지 못하면 차단하지 않고 실패를 반환합니다.\n");
    printf("  실패 시 이미 보유한 lock을 해제(back-off)하면\n");
    printf("  Hold & Wait 조건이 깨져서 deadlock이 방지됩니다.\n");
    printf("\n");
    printf("[주의]\n");
    printf("  trylock + back-off 방식은 livelock(계속 재시도만 반복)이\n");
    printf("  발생할 수 있으므로, 랜덤 대기 시간을 두는 것이 중요합니다.\n");

    /* pthread_mutex_destroy(): mutex 자원을 해제한다.
     * 프로그램 종료 전에 호출하여 자원을 정리한다. */
    pthread_mutex_destroy(&mutex_A);
    pthread_mutex_destroy(&mutex_B);

    return 0;
}
