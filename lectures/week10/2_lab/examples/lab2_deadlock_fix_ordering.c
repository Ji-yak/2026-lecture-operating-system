/*
 * lab2_deadlock_fix_ordering.c - Lock Ordering으로 Deadlock 해결
 *
 * [개요]
 *   모든 스레드가 동일한 순서(mutex_A -> mutex_B)로 lock을 잡도록 하여
 *   deadlock을 방지하는 프로그램이다.
 *   lab1_deadlock_demo.c에서 발생한 deadlock을 해결하는 첫 번째 방법이다.
 *
 * [Coffman 조건 중 어떤 것을 깨뜨리는가?]
 *   4번 조건인 "Circular Wait (순환 대기)"를 깨뜨린다.
 *   모든 스레드가 항상 같은 순서로 lock을 획득하면,
 *   순환 대기 그래프가 형성될 수 없다.
 *
 *   증명 (귀류법):
 *     - 전역 순서를 mutex_A < mutex_B로 정한다.
 *     - 모든 스레드는 낮은 순서의 lock을 먼저 획득해야 한다.
 *     - 순환 대기가 발생하려면 Thread X가 mutex_A를 보유하고 mutex_B를 대기하면서
 *       동시에 Thread Y가 mutex_B를 보유하고 mutex_A를 대기해야 한다.
 *     - 그런데 Thread Y가 mutex_B를 보유하려면 먼저 mutex_A를 획득해야 한다.
 *     - Thread X가 이미 mutex_A를 보유하고 있으므로 Thread Y는 mutex_A 단계에서
 *       이미 대기 중이어야 하고, mutex_B를 보유할 수 없다. 모순!
 *     - 따라서 순환 대기는 발생할 수 없다.
 *
 * [xv6 커널에서의 lock ordering 예시]
 *   xv6 커널도 동일한 전략을 사용한다:
 *   - wait_lock은 항상 p->lock보다 먼저 획득해야 한다.
 *   - 파일 시스템에서: inode lock -> buffer lock 순서를 항상 준수한다.
 *   이 순서를 어기면 커널 내부에서도 deadlock이 발생할 수 있다.
 *
 * [컴파일]
 *   gcc -Wall -pthread -o lab2_deadlock_fix_ordering lab2_deadlock_fix_ordering.c
 *
 * [실행]
 *   ./lab2_deadlock_fix_ordering
 */

#include <stdio.h>      /* printf(), perror() */
#include <stdlib.h>     /* exit() */
#include <pthread.h>    /* pthread_mutex_t, pthread_create(), pthread_join() 등 */
#include <unistd.h>     /* usleep() */

/* 두 개의 mutex를 정적으로 초기화한다.
 * PTHREAD_MUTEX_INITIALIZER: pthread_mutex_init() 없이 정적 mutex를 초기화하는 매크로.
 *
 * Lock ordering 규칙: 항상 mutex_A를 먼저 잠그고, 그 다음 mutex_B를 잠근다.
 * 이 순서를 모든 스레드가 준수해야 deadlock이 방지된다. */
pthread_mutex_t mutex_A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_B = PTHREAD_MUTEX_INITIALIZER;

/*
 * thread1_func: Thread 1의 실행 함수
 *
 * 잠금 순서: mutex_A -> mutex_B (전역 순서 규칙 준수)
 * 5회 반복하여 deadlock이 발생하지 않음을 확인한다.
 */
void *thread1_func(void *arg)
{
    (void)arg;  /* 사용하지 않는 매개변수 경고 억제 */

    for (int i = 0; i < 5; i++) {
        printf("[Thread 1] 반복 %d: mutex_A 잠금 시도...\n", i + 1);
        /* pthread_mutex_lock(): mutex_A를 먼저 잠근다 (순서 규칙의 첫 번째).
         * 다른 스레드가 이미 잠근 상태이면 해제될 때까지 blocking한다. */
        pthread_mutex_lock(&mutex_A);
        printf("[Thread 1] 반복 %d: mutex_A 잠금 성공\n", i + 1);

        /* usleep(): 경쟁 조건을 유도하기 위한 짧은 대기.
         * 10ms 동안 CPU를 양보하여 Thread 2가 실행될 기회를 준다.
         * 그래도 Thread 2 역시 mutex_A를 먼저 잡으려 하므로
         * deadlock 없이 순서대로 진행된다. */
        usleep(10000);  /* 10ms */

        printf("[Thread 1] 반복 %d: mutex_B 잠금 시도...\n", i + 1);
        /* pthread_mutex_lock(): mutex_B를 두 번째로 잠근다 (순서 규칙 준수).
         * Thread 2도 같은 순서를 따르므로 circular wait가 발생하지 않는다. */
        pthread_mutex_lock(&mutex_B);
        printf("[Thread 1] 반복 %d: mutex_B 잠금 성공\n", i + 1);

        /* 임계 영역 (critical section): 두 lock을 모두 보유한 상태에서 작업 수행 */
        printf("[Thread 1] 반복 %d: === 임계 영역 진입 (A, B 모두 보유) ===\n", i + 1);
        usleep(5000);  /* 5ms 동안 작업 시뮬레이션 */

        /* pthread_mutex_unlock(): mutex를 해제한다.
         * 해제 순서는 잠금의 역순(B -> A)으로 하는 것이 일반적이다.
         * (스택처럼 LIFO 순서) */
        pthread_mutex_unlock(&mutex_B);
        printf("[Thread 1] 반복 %d: mutex_B 해제\n", i + 1);
        pthread_mutex_unlock(&mutex_A);
        printf("[Thread 1] 반복 %d: mutex_A 해제\n", i + 1);
        printf("\n");
    }

    return NULL;
}

/*
 * thread2_func: Thread 2의 실행 함수
 *
 * 잠금 순서: mutex_A -> mutex_B (Thread 1과 동일한 순서!)
 *
 * 핵심: 원래 lab1에서는 mutex_B -> mutex_A 순서였지만,
 *       lock ordering 규칙에 따라 mutex_A -> mutex_B로 변경했다.
 *       이 한 가지 변경만으로 deadlock이 완전히 해결된다.
 */
void *thread2_func(void *arg)
{
    (void)arg;  /* 사용하지 않는 매개변수 경고 억제 */

    for (int i = 0; i < 5; i++) {
        /*
         * [수정 전 - deadlock 발생 (lab1의 코드)]
         *   pthread_mutex_lock(&mutex_B);  // B를 먼저 잠금
         *   pthread_mutex_lock(&mutex_A);  // A를 나중에 잠금
         *
         * [수정 후 - lock ordering 적용]
         *   pthread_mutex_lock(&mutex_A);  // A를 먼저 잠금 (순서 통일!)
         *   pthread_mutex_lock(&mutex_B);  // B를 나중에 잠금
         */
        printf("[Thread 2] 반복 %d: mutex_A 잠금 시도...\n", i + 1);
        /* pthread_mutex_lock(): 전역 순서 규칙에 따라 mutex_A를 먼저 잠근다.
         * Thread 1이 이미 mutex_A를 보유 중이면 여기서 대기한다.
         * Thread 1이 mutex_B까지 사용하고 둘 다 해제한 후에야
         * Thread 2가 mutex_A를 획득할 수 있다.
         * -> 이렇게 하면 두 스레드가 동시에 서로의 lock을 보유하는 상태가
         *    원천적으로 불가능하다. */
        pthread_mutex_lock(&mutex_A);
        printf("[Thread 2] 반복 %d: mutex_A 잠금 성공\n", i + 1);

        usleep(10000);  /* 10ms 대기 - 경쟁 상황 유도 */

        printf("[Thread 2] 반복 %d: mutex_B 잠금 시도...\n", i + 1);
        /* pthread_mutex_lock(): mutex_B를 두 번째로 잠근다 (순서 규칙 준수) */
        pthread_mutex_lock(&mutex_B);
        printf("[Thread 2] 반복 %d: mutex_B 잠금 성공\n", i + 1);

        /* 임계 영역: 두 lock을 모두 보유한 상태에서 작업 수행 */
        printf("[Thread 2] 반복 %d: === 임계 영역 진입 (A, B 모두 보유) ===\n", i + 1);
        usleep(5000);  /* 5ms 동안 작업 시뮬레이션 */

        /* mutex 해제: 잠금의 역순(B -> A)으로 해제 */
        pthread_mutex_unlock(&mutex_B);
        printf("[Thread 2] 반복 %d: mutex_B 해제\n", i + 1);
        pthread_mutex_unlock(&mutex_A);
        printf("[Thread 2] 반복 %d: mutex_A 해제\n", i + 1);
        printf("\n");
    }

    return NULL;
}

int main(void)
{
    pthread_t t1, t2;  /* 스레드 식별자 */

    printf("==============================================\n");
    printf("  Lock Ordering으로 Deadlock 해결 데모\n");
    printf("==============================================\n");
    printf("\n");
    printf("해결 방법: 모든 스레드가 동일한 순서로 lock 획득\n");
    printf("  Lock 순서 규칙: 항상 mutex_A -> mutex_B\n");
    printf("\n");
    printf("이것은 xv6 커널에서 사용하는 방식과 동일합니다.\n");
    printf("예: wait_lock은 항상 p->lock보다 먼저 획득\n");
    printf("\n");
    printf("----------------------------------------------\n\n");

    /* pthread_create(): 새로운 스레드를 생성하고 지정한 함수를 실행한다.
     * 인자: (스레드 ID 포인터, 속성(NULL=기본), 실행 함수, 함수 인자)
     * 두 스레드 모두 같은 순서(A -> B)로 lock을 잡으므로 deadlock 없음 */
    if (pthread_create(&t1, NULL, thread1_func, NULL) != 0) {
        perror("pthread_create (t1)");  /* 시스템 에러 메시지 출력 */
        exit(1);
    }
    if (pthread_create(&t2, NULL, thread2_func, NULL) != 0) {
        perror("pthread_create (t2)");
        exit(1);
    }

    /* pthread_join(): 각 스레드가 종료될 때까지 main 스레드가 대기한다.
     * lock ordering 덕분에 deadlock 없이 두 스레드 모두 정상 종료된다. */
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("----------------------------------------------\n");
    printf("두 스레드 모두 정상 종료! Deadlock이 발생하지 않았습니다.\n");
    printf("\n");
    printf("[핵심 원리]\n");
    printf("  모든 스레드가 동일한 순서(A -> B)로 lock을 획득하면\n");
    printf("  circular wait 조건이 깨져서 deadlock이 발생하지 않습니다.\n");

    /* pthread_mutex_destroy(): mutex가 사용하는 자원을 해제한다.
     * 정적 초기화(PTHREAD_MUTEX_INITIALIZER)한 경우에도
     * 프로그램 종료 전에 호출하는 것이 좋은 습관이다. */
    pthread_mutex_destroy(&mutex_A);
    pthread_mutex_destroy(&mutex_B);

    return 0;
}
