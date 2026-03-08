/*
 * lab1_deadlock_demo.c - Deadlock 발생 데모 프로그램
 *
 * [개요]
 *   두 스레드가 두 개의 mutex를 서로 반대 순서로 잡아 deadlock이 발생하는
 *   것을 직접 관찰하는 프로그램이다. 실행하면 프로그램이 멈추므로
 *   Ctrl+C로 종료해야 한다.
 *
 * [Deadlock의 4가지 필요조건 (Coffman 조건)]
 *   Deadlock이 발생하려면 다음 4가지 조건이 동시에 성립해야 한다:
 *   1. Mutual Exclusion (상호 배제):
 *      - 자원(mutex)은 한 번에 하나의 스레드만 사용할 수 있다.
 *      - 이 프로그램에서: mutex_A, mutex_B는 각각 한 스레드만 잠글 수 있다.
 *   2. Hold and Wait (점유 대기):
 *      - 스레드가 이미 자원을 하나 보유한 채로 다른 자원을 요청한다.
 *      - 이 프로그램에서: Thread 1은 mutex_A를 보유한 채 mutex_B를 요청,
 *        Thread 2는 mutex_B를 보유한 채 mutex_A를 요청.
 *   3. No Preemption (비선점):
 *      - 이미 잡은 자원을 강제로 빼앗을 수 없다.
 *      - 이 프로그램에서: pthread_mutex_lock()은 블로킹이므로 중간에 취소 불가.
 *   4. Circular Wait (순환 대기):
 *      - 스레드들이 원형으로 서로의 자원을 기다린다.
 *      - 이 프로그램에서: Thread 1 -> mutex_B(Thread 2 보유) -> mutex_A(Thread 1 보유)
 *        순환 고리가 형성됨.
 *
 * [이 프로그램의 deadlock 시나리오]
 *   시간 순서:
 *     t=0ms:  Thread 1이 mutex_A를 잠금 성공
 *     t=0ms:  Thread 2가 mutex_B를 잠금 성공
 *     t=100ms: Thread 1이 mutex_B를 요청 -> Thread 2가 보유 중 -> 대기!
 *     t=100ms: Thread 2가 mutex_A를 요청 -> Thread 1이 보유 중 -> 대기!
 *     => 둘 다 영원히 대기 (deadlock)
 *
 * [컴파일]
 *   gcc -Wall -pthread -o lab1_deadlock_demo lab1_deadlock_demo.c
 *
 * [실행]
 *   ./lab1_deadlock_demo
 *   (프로그램이 멈추면 Ctrl+C로 종료)
 */

#include <stdio.h>      /* printf(), perror() */
#include <stdlib.h>     /* exit() */
#include <pthread.h>    /* pthread_mutex_t, pthread_create(), pthread_join() 등 */
#include <unistd.h>     /* usleep() */

/* 두 개의 mutex를 정적으로 초기화한다.
 * PTHREAD_MUTEX_INITIALIZER는 pthread_mutex_init() 호출 없이
 * 정적 할당된 mutex를 초기화하는 매크로이다. */
pthread_mutex_t mutex_A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_B = PTHREAD_MUTEX_INITIALIZER;

/*
 * thread1_func: Thread 1의 실행 함수
 *
 * 잠금 순서: mutex_A -> mutex_B
 * Thread 2와 반대 순서이므로 circular wait가 발생할 수 있다.
 */
void *thread1_func(void *arg)
{
    (void)arg;  /* 사용하지 않는 매개변수 경고 억제 */

    printf("[Thread 1] mutex_A 잠금 시도...\n");
    /* pthread_mutex_lock(): mutex를 잠근다.
     * 이미 다른 스레드가 잠근 상태이면 해제될 때까지 blocking한다.
     * 성공하면 0을 반환한다. */
    pthread_mutex_lock(&mutex_A);
    printf("[Thread 1] mutex_A 잠금 성공!\n");

    /* usleep(): 마이크로초 단위로 현재 스레드를 일시 정지한다.
     * 여기서는 의도적으로 100ms 대기하여 Thread 2가 mutex_B를
     * 먼저 잠글 시간을 준다. 이렇게 하면 deadlock이 거의 확실히 발생한다. */
    printf("[Thread 1] 0.1초 대기 (다른 스레드가 mutex_B를 잡도록)...\n");
    usleep(100000);  /* 100ms = 100,000 마이크로초 */

    /* 이 시점에서 Thread 2가 이미 mutex_B를 잠근 상태이다.
     * Thread 1은 mutex_A를 보유한 채로 mutex_B를 요청한다.
     * Thread 2는 mutex_B를 보유한 채로 mutex_A를 요청 중이다.
     * -> Circular Wait 성립 -> Deadlock 발생!
     *
     * 이 pthread_mutex_lock() 호출은 영원히 반환되지 않는다. */
    printf("[Thread 1] mutex_B 잠금 시도... (여기서 멈출 수 있음)\n");
    pthread_mutex_lock(&mutex_B);
    printf("[Thread 1] mutex_B 잠금 성공!\n");  /* 이 줄은 출력되지 않음 */

    /* 임계 영역 (critical section) - deadlock으로 인해 도달 불가 */
    printf("[Thread 1] 두 lock 모두 획득 - 작업 수행 중\n");

    /* pthread_mutex_unlock(): mutex를 해제한다.
     * 대기 중인 스레드가 있으면 그 중 하나가 깨어난다. */
    pthread_mutex_unlock(&mutex_B);
    pthread_mutex_unlock(&mutex_A);
    printf("[Thread 1] 완료\n");

    return NULL;
}

/*
 * thread2_func: Thread 2의 실행 함수
 *
 * 잠금 순서: mutex_B -> mutex_A  (Thread 1과 반대!)
 * 이 반대 순서가 circular wait를 만들어 deadlock을 유발한다.
 */
void *thread2_func(void *arg)
{
    (void)arg;  /* 사용하지 않는 매개변수 경고 억제 */

    printf("[Thread 2] mutex_B 잠금 시도...\n");
    /* pthread_mutex_lock(): mutex_B를 잠근다.
     * Thread 1이 아직 mutex_B를 잠그지 않았으므로 즉시 성공한다. */
    pthread_mutex_lock(&mutex_B);
    printf("[Thread 2] mutex_B 잠금 성공!\n");

    /* 100ms 대기하여 Thread 1이 mutex_A를 잠글 시간을 준다 */
    printf("[Thread 2] 0.1초 대기 (다른 스레드가 mutex_A를 잡도록)...\n");
    usleep(100000);  /* 100ms */

    /* 이 시점에서 Thread 1이 이미 mutex_A를 잠근 상태이다.
     * Hold and Wait: Thread 2는 mutex_B를 보유한 채 mutex_A를 요청.
     * Thread 1은 mutex_A를 보유한 채 mutex_B를 요청 중.
     * -> 두 스레드가 서로의 자원을 기다리며 영원히 대기 (deadlock) */
    printf("[Thread 2] mutex_A 잠금 시도... (여기서 멈출 수 있음)\n");
    pthread_mutex_lock(&mutex_A);
    printf("[Thread 2] mutex_A 잠금 성공!\n");  /* 이 줄은 출력되지 않음 */

    /* 임계 영역 (critical section) - deadlock으로 인해 도달 불가 */
    printf("[Thread 2] 두 lock 모두 획득 - 작업 수행 중\n");

    pthread_mutex_unlock(&mutex_A);
    pthread_mutex_unlock(&mutex_B);
    printf("[Thread 2] 완료\n");

    return NULL;
}

int main(void)
{
    pthread_t t1, t2;  /* 스레드 식별자 */

    printf("==============================================\n");
    printf("  Deadlock 발생 데모\n");
    printf("==============================================\n");
    printf("\n");
    printf("Thread 1: mutex_A -> mutex_B 순서로 잠금\n");
    printf("Thread 2: mutex_B -> mutex_A 순서로 잠금 (반대!)\n");
    printf("\n");
    printf("Deadlock이 발생하면 프로그램이 멈춥니다.\n");
    printf("Ctrl+C로 종료하세요.\n");
    printf("\n");
    printf("----------------------------------------------\n");

    /* pthread_create(): 새로운 스레드를 생성하고 실행을 시작한다.
     * 인자: (스레드 ID 포인터, 속성(NULL=기본), 실행 함수, 함수 인자)
     * 성공 시 0을 반환하고, 새 스레드가 즉시 실행을 시작한다. */
    if (pthread_create(&t1, NULL, thread1_func, NULL) != 0) {
        perror("pthread_create (t1)");  /* 에러 메시지 출력 */
        exit(1);
    }
    if (pthread_create(&t2, NULL, thread2_func, NULL) != 0) {
        perror("pthread_create (t2)");
        exit(1);
    }

    /* pthread_join(): 지정한 스레드가 종료될 때까지 호출 스레드(main)를 대기시킨다.
     * deadlock이 발생하면 t1, t2 모두 영원히 종료되지 않으므로
     * main 스레드도 여기서 영원히 대기한다. */
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    /* 아래 코드는 deadlock 발생 시 절대 실행되지 않는다 */
    printf("\n----------------------------------------------\n");
    printf("두 스레드 모두 정상 종료 (deadlock 미발생)\n");

    /* pthread_mutex_destroy(): mutex 자원을 해제한다.
     * 프로그램 종료 전에 호출하는 것이 좋은 습관이다. */
    pthread_mutex_destroy(&mutex_A);
    pthread_mutex_destroy(&mutex_B);

    return 0;
}
