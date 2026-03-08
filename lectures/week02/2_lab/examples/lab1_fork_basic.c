/*
 * lab1_fork_basic.c - fork()와 wait()의 기본 동작을 보여주는 예제
 *
 * 이 프로그램은 프로세스 생성의 핵심인 fork() 시스템 콜과,
 * 부모가 자식의 종료를 기다리는 wait() 시스템 콜의 사용법을 학습한다.
 *
 * 학습 목표:
 *   1. fork()의 반환값에 따른 부모/자식 구분 방법
 *   2. wait()를 통한 자식 프로세스 종료 대기
 *   3. WIFEXITED / WEXITSTATUS 매크로로 종료 상태 확인
 *   4. 여러 자식 프로세스 생성 및 관리
 *
 * Compile: gcc -Wall -o fork_basic lab1_fork_basic.c
 * Run:     ./fork_basic
 */

#include <stdio.h>      /* printf, perror */
#include <stdlib.h>      /* exit */
#include <unistd.h>      /* fork, getpid, getppid, sleep, usleep */
#include <sys/wait.h>    /* wait, waitpid, WIFEXITED, WEXITSTATUS */

int main(void)
{
    printf("=== fork() + wait() 기본 예제 ===\n\n");

    /*
     * fork()는 현재 프로세스를 복제하여 자식 프로세스를 만든다.
     * 호출 한 번에 반환이 두 번 일어나는 독특한 시스템 콜이다:
     *   - 부모 프로세스에서는 자식의 PID를 반환 (양수)
     *   - 자식 프로세스에서는 0을 반환
     *   - 실패하면 -1을 반환 (자식은 생성되지 않음)
     *
     * fork() 직후, 부모와 자식은 동일한 코드를 실행하지만
     * 반환값이 다르므로 if-else로 분기하여 서로 다른 작업을 수행한다.
     */
    printf("[부모 PID=%d] fork() 호출 전\n", getpid());

    /* fork() 호출 - 이 시점에서 프로세스가 두 개로 갈라진다 */
    pid_t pid = fork();

    if (pid < 0) {
        /* fork 실패: 시스템 자원 부족 등의 이유로 자식 생성에 실패한 경우 */
        perror("fork");  /* errno에 저장된 에러 메시지를 출력 */
        exit(1);
    } else if (pid == 0) {
        /*
         * 자식 프로세스 영역
         * fork()가 0을 반환했으므로, 여기는 자식 프로세스에서만 실행된다.
         *
         * getpid()  - 자기 자신의 PID를 반환
         * getppid() - 부모 프로세스의 PID를 반환
         */
        printf("[자식 PID=%d] 나는 자식 프로세스! 부모 PID=%d\n",
               getpid(), getppid());
        printf("[자식 PID=%d] 작업 수행 중...\n", getpid());

        /* sleep(1): 1초 동안 대기하여 실제 작업 수행을 시뮬레이션 */
        sleep(1);

        /*
         * exit(42): 종료 코드 42로 자식 프로세스를 종료한다.
         * 이 값은 부모가 wait()를 통해 WEXITSTATUS()로 읽을 수 있다.
         * 종료 코드는 0~255 범위이며, 0은 보통 정상 종료를 의미한다.
         */
        printf("[자식 PID=%d] 작업 완료, exit(42)로 종료\n", getpid());
        exit(42);
    } else {
        /*
         * 부모 프로세스 영역
         * fork()가 자식의 PID(양수)를 반환했으므로, 여기는 부모에서만 실행된다.
         * pid 변수에는 방금 생성한 자식의 PID가 들어있다.
         */
        printf("[부모 PID=%d] fork() 성공! 자식 PID=%d\n", getpid(), pid);

        /*
         * wait(&status): 자식 프로세스 중 하나가 종료될 때까지 부모를 블록(대기)시킨다.
         *   - 자식이 종료되면, 종료된 자식의 PID를 반환한다.
         *   - status 변수에 자식의 종료 상태 정보가 저장된다.
         *   - 자식이 없으면 -1을 반환한다.
         *
         * wait()가 없으면 부모가 먼저 끝나 자식이 고아(orphan) 프로세스가 될 수 있다.
         */
        int status;
        pid_t terminated = wait(&status);

        if (terminated < 0) {
            perror("wait");
            exit(1);
        }

        printf("[부모 PID=%d] 자식(PID=%d)이 종료됨\n", getpid(), terminated);

        /*
         * WIFEXITED(status): 자식이 정상적으로 exit()을 호출해 종료되었는지 확인.
         *   - true이면 WEXITSTATUS(status)로 종료 코드를 꺼낼 수 있다.
         *   - 시그널에 의해 비정상 종료된 경우 WIFSIGNALED()를 사용한다.
         */
        if (WIFEXITED(status)) {
            printf("[부모 PID=%d] 자식의 종료 코드: %d\n",
                   getpid(), WEXITSTATUS(status));
        }
    }

    /* ------------------------------------------------------------ */
    /* 추가 실험: fork()를 반복문에서 호출하여 여러 자식 만들기      */
    /* ------------------------------------------------------------ */
    printf("\n=== 추가 실험: fork()로 여러 자식 만들기 ===\n\n");

    int num_children = 3;  /* 생성할 자식 프로세스 수 */

    for (int i = 0; i < num_children; i++) {
        pid_t child = fork();
        if (child < 0) {
            perror("fork");
            exit(1);
        } else if (child == 0) {
            /*
             * 자식 프로세스: 각 자식은 서로 다른 시간만큼 대기한다.
             * usleep()은 마이크로초(1/1,000,000초) 단위로 대기한다.
             * 역순으로 종료되도록 i가 작을수록 더 오래 대기한다.
             */
            printf("  자식 #%d (PID=%d) 시작\n", i, getpid());
            usleep((unsigned int)(100000 * (num_children - i)));  /* 역순으로 종료 */
            printf("  자식 #%d (PID=%d) 종료\n", i, getpid());

            /*
             * 중요: 자식에서 반드시 exit()을 호출해야 한다.
             * exit()이 없으면 자식도 for 루프를 계속 돌며 또 fork()를 호출하게 되어
             * 프로세스가 기하급수적으로 늘어나는 "fork bomb"이 된다.
             */
            exit(i);
        }
        /* 부모는 for 루프를 계속 진행하여 다음 자식을 fork() */
    }

    /*
     * 부모: 모든 자식이 종료될 때까지 wait()를 반복 호출한다.
     * wait()는 종료된 자식이 있으면 즉시 반환하고,
     * 아직 종료된 자식이 없으면 하나가 종료될 때까지 블록된다.
     */
    for (int i = 0; i < num_children; i++) {
        int status;
        pid_t done = wait(&status);
        if (WIFEXITED(status)) {
            printf("[부모] 자식 PID=%d 종료 (exit code=%d)\n",
                   done, WEXITSTATUS(status));
        }
    }

    /*
     * 자식들의 종료 순서는 생성 순서와 다를 수 있다.
     * 운영체제의 스케줄러가 어떤 프로세스를 먼저 실행할지 결정하기 때문이다.
     * 이 예제에서는 usleep()으로 의도적으로 역순 종료를 유도했다.
     */
    printf("\n주목: 자식들의 종료 순서가 생성 순서와 다를 수 있습니다!\n");

    return 0;
}
