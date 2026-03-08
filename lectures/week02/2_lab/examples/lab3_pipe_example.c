/*
 * lab3_pipe_example.c - pipe()를 사용한 프로세스 간 통신(IPC) 예제
 *
 * 이 프로그램은 pipe() 시스템 콜을 사용하여 프로세스 사이에
 * 데이터를 주고받는 방법을 학습한다.
 *
 * 학습 목표:
 *   1. pipe()로 단방향 통신 채널 생성하는 방법
 *   2. 사용하지 않는 파이프 끝(fd)을 반드시 닫아야 하는 이유
 *   3. 양방향 통신을 위해 파이프 2개를 사용하는 패턴
 *   4. dup2()와 pipe를 결합하여 셸의 파이프라인(cmd1 | cmd2)을 구현하는 원리
 *
 * Compile: gcc -Wall -o pipe_example lab3_pipe_example.c
 * Run:     ./pipe_example
 */

#include <stdio.h>      /* printf, perror */
#include <stdlib.h>      /* exit */
#include <string.h>      /* strlen */
#include <unistd.h>      /* pipe, fork, read, write, close, dup2 */
#include <sys/wait.h>    /* wait, waitpid */

/*
 * pipe()는 커널 내부에 단방향 통신 채널(버퍼)을 생성한다.
 *
 *   int fd[2];
 *   pipe(fd);
 *
 *   fd[0] = 읽기 끝 (read end)  - 이쪽에서 데이터를 읽는다
 *   fd[1] = 쓰기 끝 (write end) - 이쪽으로 데이터를 쓴다
 *
 *   데이터 흐름 방향 (단방향):
 *     쓰는 프로세스 ---> write(fd[1]) ===[커널 버퍼]=== read(fd[0]) ---> 읽는 프로세스
 *
 * 주의: pipe()는 fork() 전에 호출해야 한다.
 *       fork() 후 부모와 자식이 동일한 fd[0], fd[1]을 공유하게 되어
 *       서로 통신할 수 있게 된다.
 */

/* ================================================================== */
/* Demo 1: 부모 -> 자식 단방향 통신                                     */
/* ================================================================== */
static void demo_parent_to_child(void)
{
    printf("\n=== Demo 1: 부모 -> 자식 단방향 통신 ===\n\n");

    int fd[2];  /* fd[0]=읽기, fd[1]=쓰기 */

    /*
     * pipe(fd): 파이프를 생성한다.
     * 실패 시 -1을 반환 (시스템의 fd 제한 초과 등).
     */
    if (pipe(fd) < 0) {
        perror("pipe");
        exit(1);
    }

    /*
     * fork() 전에 pipe()를 호출했으므로,
     * fork() 후 부모와 자식 모두 fd[0]과 fd[1]을 갖는다.
     * 부모는 fd[1]에 쓰고, 자식은 fd[0]에서 읽는 방식으로 통신한다.
     */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /*
         * 자식 프로세스: 파이프에서 읽기만 한다.
         *
         * 중요: 사용하지 않는 쓰기 끝(fd[1])을 반드시 닫아야 한다!
         * 이유: 쓰기 끝이 열려있는 한, read()는 EOF를 받지 못하고
         *       데이터가 더 올 것으로 기대하며 영원히 블록된다.
         *       파이프의 모든 쓰기 끝이 닫혀야 read()가 0(EOF)을 반환한다.
         */
        close(fd[1]);  /* 자식은 쓰지 않으므로 쓰기 끝을 닫는다 */

        char buf[256];
        /*
         * read(fd[0], buf, size):
         *   파이프에서 데이터를 읽는다.
         *   데이터가 아직 없으면 쓰는 쪽이 write할 때까지 블록된다.
         *   반환값 n: 실제로 읽은 바이트 수 (0이면 EOF)
         */
        ssize_t n = read(fd[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';  /* read()는 널 종료를 추가하지 않으므로 직접 추가 */
            printf("[자식] 부모로부터 받은 메시지: \"%s\"\n", buf);
        }

        close(fd[0]);  /* 다 읽었으면 읽기 끝도 닫는다 */
        exit(0);
    } else {
        /*
         * 부모 프로세스: 파이프에 쓰기만 한다.
         * 사용하지 않는 읽기 끝(fd[0])을 닫는다.
         */
        close(fd[0]);  /* 부모는 읽지 않으므로 읽기 끝을 닫는다 */

        const char *msg = "안녕, 자식 프로세스!";
        printf("[부모] 자식에게 메시지 전송: \"%s\"\n", msg);

        /* write(fd[1], msg, len): 파이프의 쓰기 끝에 데이터를 쓴다 */
        write(fd[1], msg, strlen(msg));

        /*
         * 쓰기 끝을 닫아야 자식의 read()가 EOF(0)를 받을 수 있다.
         * 닫지 않으면 자식은 "아직 데이터가 더 올 수 있다"고 판단하여
         * read()에서 영원히 블록되어 프로그램이 멈춘다.
         */
        close(fd[1]);

        /* 자식이 종료될 때까지 대기 */
        wait(NULL);
    }
}

/* ================================================================== */
/* Demo 2: 양방향 통신 (pipe 2개 사용)                                  */
/* ================================================================== */
static void demo_bidirectional(void)
{
    printf("\n=== Demo 2: 양방향 통신 (pipe 2개) ===\n\n");

    /*
     * pipe는 단방향이므로, 양방향 통신을 하려면 파이프 2개가 필요하다.
     *
     *   부모 ---write---> parent_to_child[1] ===pipe=== parent_to_child[0] ---read---> 자식
     *   부모 ---read----> child_to_parent[0] ===pipe=== child_to_parent[1] ---write--> 자식
     */
    int parent_to_child[2];  /* 부모 -> 자식 방향 파이프 */
    int child_to_parent[2];  /* 자식 -> 부모 방향 파이프 */

    if (pipe(parent_to_child) < 0 || pipe(child_to_parent) < 0) {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /*
         * 자식 프로세스:
         *   - parent_to_child 파이프에서 읽기 (읽기 끝만 필요)
         *   - child_to_parent 파이프에 쓰기 (쓰기 끝만 필요)
         *   - 사용하지 않는 끝들은 모두 닫는다
         */
        close(parent_to_child[1]);  /* 부모->자식 파이프의 쓰기 끝: 자식은 안 씀 */
        close(child_to_parent[0]);  /* 자식->부모 파이프의 읽기 끝: 자식은 안 읽음 */

        /* 부모로부터 메시지 읽기 */
        char buf[256];
        ssize_t n = read(parent_to_child[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("[자식] 받은 메시지: \"%s\"\n", buf);
        }

        /* 부모에게 답장 보내기 */
        const char *reply = "잘 받았어요, 부모님!";
        printf("[자식] 답장 전송: \"%s\"\n", reply);
        write(child_to_parent[1], reply, strlen(reply));

        /* 사용한 fd들을 모두 닫고 종료 */
        close(parent_to_child[0]);
        close(child_to_parent[1]);
        exit(0);
    } else {
        /*
         * 부모 프로세스:
         *   - parent_to_child 파이프에 쓰기 (쓰기 끝만 필요)
         *   - child_to_parent 파이프에서 읽기 (읽기 끝만 필요)
         */
        close(parent_to_child[0]);  /* 부모->자식 파이프의 읽기 끝: 부모는 안 읽음 */
        close(child_to_parent[1]);  /* 자식->부모 파이프의 쓰기 끝: 부모는 안 씀 */

        /* 자식에게 메시지 보내기 */
        const char *msg = "안녕, 잘 지내니?";
        printf("[부모] 메시지 전송: \"%s\"\n", msg);
        write(parent_to_child[1], msg, strlen(msg));
        close(parent_to_child[1]);  /* 전송 완료 후 쓰기 끝을 닫아 EOF 전달 */

        /* 자식의 답장 읽기 */
        char buf[256];
        ssize_t n = read(child_to_parent[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("[부모] 자식의 답장: \"%s\"\n", buf);
        }

        close(child_to_parent[0]);
        wait(NULL);  /* 자식 종료 대기 */
    }
}

/* ================================================================== */
/* Demo 3: pipe로 두 명령어 연결 (셸의 cmd1 | cmd2 원리)               */
/* ================================================================== */
static void demo_pipe_commands(void)
{
    printf("\n=== Demo 3: pipe로 명령어 연결 (echo | wc) ===\n\n");

    /*
     * 셸에서 "echo hello world | wc -w" 를 실행하는 것과 같은 원리:
     *
     *   echo의 stdout ----> fd[1] ===pipe=== fd[0] ----> wc의 stdin
     *
     * 즉, 첫 번째 명령어(echo)의 표준 출력을 파이프를 통해
     * 두 번째 명령어(wc)의 표준 입력으로 연결한다.
     *
     * 이를 위해 dup2()를 사용하여:
     *   - echo의 stdout(fd 1)을 파이프의 쓰기 끝으로 교체
     *   - wc의 stdin(fd 0)을 파이프의 읽기 끝으로 교체
     */

    int fd[2];
    if (pipe(fd) < 0) {
        perror("pipe");
        exit(1);
    }

    /* ---- 첫 번째 자식: echo (stdout을 파이프의 쓰기 끝으로 연결) ---- */
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork");
        exit(1);
    }

    if (pid1 == 0) {
        /*
         * echo 자식: stdout을 파이프의 쓰기 끝(fd[1])으로 리다이렉트한다.
         *
         * 단계:
         *   1. 읽기 끝(fd[0])을 닫는다 (echo는 파이프에서 읽지 않음)
         *   2. dup2(fd[1], STDOUT_FILENO)로 stdout을 파이프 쓰기 끝으로 교체
         *   3. 원본 fd[1]을 닫는다 (dup2로 복제했으므로 원본은 불필요)
         *   4. exec("echo")를 호출하면, echo가 stdout에 쓸 때 파이프로 들어감
         */
        close(fd[0]);                      /* 1. 읽기 끝 닫기 */
        dup2(fd[1], STDOUT_FILENO);        /* 2. stdout -> 파이프 쓰기 끝 */
        close(fd[1]);                      /* 3. 원본 fd 닫기 (중복 방지) */

        /* echo 실행 - 출력이 화면 대신 파이프로 간다 */
        execlp("echo", "echo", "hello", "world", "from", "pipe", (char *)NULL);
        perror("exec echo");
        exit(1);
    }

    /* ---- 두 번째 자식: wc (stdin을 파이프의 읽기 끝으로 연결) ---- */
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork");
        exit(1);
    }

    if (pid2 == 0) {
        /*
         * wc 자식: stdin을 파이프의 읽기 끝(fd[0])으로 리다이렉트한다.
         *
         * 단계:
         *   1. 쓰기 끝(fd[1])을 닫는다 (wc는 파이프에 쓰지 않음)
         *      중요: 이것을 닫지 않으면 wc의 read()가 EOF를 받지 못한다!
         *   2. dup2(fd[0], STDIN_FILENO)로 stdin을 파이프 읽기 끝으로 교체
         *   3. 원본 fd[0]을 닫는다
         *   4. exec("wc")를 호출하면, wc가 stdin에서 읽을 때 파이프 데이터를 읽음
         */
        close(fd[1]);                      /* 1. 쓰기 끝 닫기 */
        dup2(fd[0], STDIN_FILENO);         /* 2. stdin -> 파이프 읽기 끝 */
        close(fd[0]);                      /* 3. 원본 fd 닫기 */

        /* wc -w 실행 - 단어 수를 센다 (입력은 파이프에서 온다) */
        execlp("wc", "wc", "-w", (char *)NULL);
        perror("exec wc");
        exit(1);
    }

    /*
     * 부모 프로세스: 파이프의 양쪽 끝을 모두 닫아야 한다!
     *
     * 부모가 fd[1](쓰기 끝)을 닫지 않으면, wc 자식이 파이프에서
     * 데이터를 기다리며 영원히 블록된다 (부모도 쓸 수 있다고 판단하므로).
     * 부모가 fd[0](읽기 끝)을 닫지 않으면, fd 누수(leak)가 발생한다.
     */
    close(fd[0]);
    close(fd[1]);

    /* 두 자식 모두 종료될 때까지 대기 */
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    printf("(위 숫자는 echo가 출력한 단어 수입니다)\n");
}

int main(void)
{
    printf("=== pipe() 프로세스 간 통신 데모 ===\n");
    printf("핵심: pipe(fd) -> fd[0]=읽기, fd[1]=쓰기\n");

    demo_parent_to_child();
    demo_bidirectional();
    demo_pipe_commands();

    printf("\n=== 주의사항 정리 ===\n");
    printf("1. 사용하지 않는 pipe fd는 반드시 close()할 것\n");
    printf("   - 쓰기 끝이 열려있으면 읽는 쪽의 read()가 EOF를 받지 못함\n");
    printf("2. pipe는 단방향 -> 양방향 통신에는 pipe 2개 필요\n");
    printf("3. pipe 버퍼가 가득 차면 write()가 블록됨\n");

    return 0;
}
