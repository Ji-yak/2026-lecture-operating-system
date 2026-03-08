/*
 * deadlock_demo.c - 데드락(Deadlock, 교착 상태) 시연 프로그램
 *
 * [핵심 개념]
 * 데드락(Deadlock)이란 두 개 이상의 스레드가 서로 상대방이 보유한
 * 자원을 기다리며 영원히 진행하지 못하는 상태이다.
 *
 * 데드락 발생의 4가지 필요 조건 (Coffman 조건):
 *   1. 상호 배제(Mutual Exclusion): 자원은 한 번에 하나의 스레드만 사용 가능
 *   2. 점유 대기(Hold and Wait): 자원을 보유한 채 다른 자원을 대기
 *   3. 비선점(No Preemption): 보유 중인 자원을 강제로 빼앗을 수 없음
 *   4. 순환 대기(Circular Wait): 스레드 간 자원 대기가 순환 구조
 *
 * 이 프로그램에서의 순환 대기:
 *   Thread A: lock1 보유 -> lock2 대기
 *   Thread B: lock2 보유 -> lock1 대기
 *   -> 서로 상대방의 잠금을 기다리며 영원히 진행 불가!
 *
 * 해결 방법: 잠금 순서(Lock Ordering)를 통일한다.
 *   모든 스레드가 항상 lock1 -> lock2 순서로 획득하면 데드락이 발생하지 않는다.
 *   이를 "잠금 순서 규칙(Lock Ordering Discipline)"이라 한다.
 *
 * 컴파일: gcc -Wall -pthread -o deadlock_demo deadlock_demo.c
 * 실행:   ./deadlock_demo
 *
 * 주의: 프로그램이 멈추면(데드락 발생) Ctrl+C로 종료하세요.
 *       데드락은 확률적으로 발생하므로, 발생하지 않으면 다시 실행하세요.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/*
 * 두 개의 뮤텍스를 전역으로 선언
 * PTHREAD_MUTEX_INITIALIZER로 정적 초기화
 */
pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;

/*
 * Thread A의 실행 함수
 * 잠금 획득 순서: lock1 -> lock2
 */
void *thread_a(void *arg)
{
    /* 사용하지 않는 매개변수 경고 방지 */
    (void)arg;

    /* 1단계: lock1 획득 시도 */
    printf("[Thread A] Trying to acquire lock1...\n");
    /*
     * pthread_mutex_lock(&lock1): lock1 잠금을 획득한다.
     * 이 시점에서 Thread A는 lock1을 보유하게 된다.
     */
    pthread_mutex_lock(&lock1);
    printf("[Thread A] Acquired lock1.\n");

    /*
     * usleep(100000): 100밀리초(0.1초) 동안 대기
     *
     * 이 대기 시간은 데드락 발생 확률을 높이기 위한 것이다.
     * Thread A가 lock1을 획득한 후 잠시 쉬는 동안,
     * Thread B가 lock2를 획득할 시간을 확보한다.
     * 이 대기가 없으면 한 스레드가 두 잠금을 모두 획득하고
     * 해제한 후에야 다른 스레드가 시작할 수 있어
     * 데드락이 발생하지 않을 수 있다.
     */
    usleep(100000);

    /* 2단계: lock2 획득 시도 - 데드락 발생 가능 지점! */
    printf("[Thread A] Trying to acquire lock2...\n");
    /*
     * pthread_mutex_lock(&lock2): lock2 잠금을 획득하려 시도한다.
     * 만약 Thread B가 이미 lock2를 보유하고 있다면,
     * Thread A는 여기서 영원히 블로킹된다.
     * (Thread B도 lock1을 기다리며 블로킹 중이므로 lock2를 해제할 수 없다)
     */
    pthread_mutex_lock(&lock2);
    printf("[Thread A] Acquired lock2.\n");

    /* 임계 영역: 두 잠금을 모두 보유한 상태에서 안전하게 작업 수행 */
    printf("[Thread A] In critical section with both locks.\n");

    /*
     * 잠금 해제: 획득의 역순으로 해제하는 것이 일반적인 관례
     * (역순이 필수는 아니지만, 코드 가독성과 일관성을 위해 권장)
     */
    pthread_mutex_unlock(&lock2);
    pthread_mutex_unlock(&lock1);
    printf("[Thread A] Released both locks.\n");

    return NULL;
}

/*
 * Thread B의 실행 함수
 * 잠금 획득 순서: lock2 -> lock1 (Thread A와 반대 순서!)
 *
 * Thread A와 반대 순서로 잠금을 획득하기 때문에
 * 순환 대기(circular wait)가 형성되어 데드락이 발생한다.
 */
void *thread_b(void *arg)
{
    (void)arg;

    /* 1단계: lock2 획득 시도 (Thread A는 lock1부터 시작한다는 점에 주목) */
    printf("[Thread B] Trying to acquire lock2...\n");
    /*
     * pthread_mutex_lock(&lock2): lock2 잠금을 획득한다.
     * 이 시점에서 Thread B는 lock2를 보유하게 된다.
     */
    pthread_mutex_lock(&lock2);
    printf("[Thread B] Acquired lock2.\n");

    /* 데드락 확률을 높이기 위한 대기 (위의 Thread A와 동일한 이유) */
    usleep(100000);

    /* 2단계: lock1 획득 시도 - 데드락 발생 가능 지점! */
    printf("[Thread B] Trying to acquire lock1...\n");
    /*
     * pthread_mutex_lock(&lock1): lock1 잠금을 획득하려 시도한다.
     * 만약 Thread A가 이미 lock1을 보유하고 있다면,
     * Thread B는 여기서 영원히 블로킹된다.
     *
     * 이 시점의 상태:
     *   Thread A: lock1 보유, lock2 대기 (블로킹)
     *   Thread B: lock2 보유, lock1 대기 (블로킹)
     *   -> 순환 대기 형성 -> 데드락!
     */
    pthread_mutex_lock(&lock1);
    printf("[Thread B] Acquired lock1.\n");

    /* 임계 영역 */
    printf("[Thread B] In critical section with both locks.\n");

    /* 잠금 해제 */
    pthread_mutex_unlock(&lock1);
    pthread_mutex_unlock(&lock2);
    printf("[Thread B] Released both locks.\n");

    return NULL;
}

int main(void)
{
    printf("=== Deadlock Demo ===\n");
    printf("Thread A: lock1 -> lock2\n");
    printf("Thread B: lock2 -> lock1\n");
    printf("If deadlock occurs, the program will hang. Use Ctrl+C to exit.\n\n");

    pthread_t ta, tb;

    /*
     * pthread_create: 두 스레드를 생성한다.
     * 두 스레드가 거의 동시에 시작되므로, 각각 하나의 잠금을
     * 획득한 후 상대방의 잠금을 기다리는 상황이 만들어진다.
     */
    pthread_create(&ta, NULL, thread_a, NULL);
    pthread_create(&tb, NULL, thread_b, NULL);

    /*
     * pthread_join: 두 스레드가 모두 종료될 때까지 대기한다.
     * 데드락이 발생하면 스레드가 종료되지 않으므로
     * 메인 스레드도 여기서 영원히 블로킹된다.
     */
    pthread_join(ta, NULL);
    pthread_join(tb, NULL);

    /* 이 줄이 출력되면 데드락이 발생하지 않은 것이다 */
    printf("\nNo deadlock occurred this time!\n");
    printf("Tip: Run multiple times to observe the deadlock.\n");

    /*
     * pthread_mutex_destroy: 뮤텍스 자원을 해제한다.
     * 데드락이 발생하면 이 코드에 도달하지 못한다.
     */
    pthread_mutex_destroy(&lock1);
    pthread_mutex_destroy(&lock2);
    return 0;
}
