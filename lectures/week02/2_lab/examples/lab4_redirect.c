/*
 * lab4_redirect.c - dup2()를 사용한 I/O 리다이렉션 예제
 *
 * 이 프로그램은 파일 디스크립터(fd)와 dup2() 시스템 콜을 사용하여
 * 표준 입출력을 파일로 리다이렉트하는 방법을 학습한다.
 * 이것은 셸이 >, <, >> 등의 리다이렉션을 구현하는 원리이다.
 *
 * 학습 목표:
 *   1. 파일 디스크립터(fd)의 개념과 표준 fd 번호(0, 1, 2)
 *   2. dup2(oldfd, newfd)의 동작 원리
 *   3. 출력 리다이렉션 (cmd > file) 구현 방법
 *   4. 입력 리다이렉션 (cmd < file) 구현 방법
 *   5. fork() + dup2() + exec() 조합으로 셸 리다이렉션 구현
 *
 * Compile: gcc -Wall -o redirect lab4_redirect.c
 * Run:     ./redirect
 */

#include <stdio.h>      /* printf, perror, fflush */
#include <stdlib.h>      /* exit */
#include <string.h>      /* strlen */
#include <unistd.h>      /* fork, dup, dup2, close, write, unlink */
#include <fcntl.h>       /* open, O_WRONLY, O_RDONLY, O_CREAT, O_TRUNC */
#include <sys/wait.h>    /* wait, WIFEXITED, WEXITSTATUS */

/*
 * 파일 디스크립터(fd) 기본 개념:
 *   프로세스가 열어둔 파일(또는 파이프, 소켓 등)을 가리키는 정수 번호이다.
 *   커널이 관리하는 "파일 테이블"의 인덱스라고 생각하면 된다.
 *
 *   번호  이름     설명
 *   ----  ------   -------------------------
 *   0     stdin    표준 입력 (키보드)
 *   1     stdout   표준 출력 (화면)
 *   2     stderr   표준 에러 (화면)
 *   3+    (사용자) open() 등으로 열면 할당됨
 *
 * dup2(oldfd, newfd) 시스템 콜:
 *   newfd가 oldfd와 같은 파일을 가리키도록 설정한다.
 *   만약 newfd가 이미 열린 fd라면, 먼저 닫은 후 복제한다.
 *
 *   예) dup2(fd, STDOUT_FILENO)
 *       --> fd 1(stdout)이 fd가 가리키는 파일을 가리키게 된다.
 *       --> 이후 printf()나 write(1, ...)의 출력이 해당 파일로 간다.
 *
 * 리다이렉션 원리:
 *   "cmd > file" = stdout(1)을 file로 연결   --> dup2(fd, 1)
 *   "cmd < file" = stdin(0)을 file로 연결    --> dup2(fd, 0)
 *   "cmd >> file" = stdout(1)을 file에 추가  --> open(..., O_APPEND) + dup2(fd, 1)
 */

/* ================================================================== */
/* Demo 1: 출력 리다이렉션 (stdout -> 파일)                            */
/* ================================================================== */
static void demo_output_redirect(void)
{
    printf("\n=== Demo 1: 출력 리다이렉션 (echo > output.txt) ===\n\n");

    const char *filename = "/tmp/redirect_demo_output.txt";

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /*
         * 자식 프로세스: stdout을 파일로 리다이렉트한 후 echo를 실행한다.
         * 이것이 셸에서 "echo ... > output.txt"를 처리하는 원리이다.
         */

        /*
         * open(): 파일을 열고 파일 디스크립터(fd)를 반환한다.
         *
         * 플래그 설명:
         *   O_WRONLY : 쓰기 전용으로 연다
         *   O_CREAT  : 파일이 없으면 새로 생성한다
         *   O_TRUNC  : 파일이 이미 있으면 내용을 비운다 (덮어쓰기)
         * 0644: 파일 권한 (소유자 읽기/쓰기, 그룹/기타 읽기)
         */
        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("open");
            exit(1);
        }

        /*
         * dup2(fd, STDOUT_FILENO):
         *   STDOUT_FILENO(=1, 즉 stdout)이 fd가 가리키는 파일을 가리키게 한다.
         *   이후 모든 stdout 출력(printf, echo 등)이 화면 대신 파일로 간다.
         */
        dup2(fd, STDOUT_FILENO);

        /*
         * 원본 fd를 닫는다.
         * dup2로 stdout(1)이 이미 같은 파일을 가리키고 있으므로
         * 원본 fd는 더 이상 필요 없다. 닫지 않으면 fd 누수가 발생한다.
         */
        close(fd);

        /*
         * execlp로 echo를 실행한다.
         * echo는 stdout에 출력하지만, stdout이 파일로 교체되었으므로
         * 출력 내용이 화면 대신 /tmp/redirect_demo_output.txt에 저장된다.
         */
        execlp("echo", "echo", "이 내용은 파일에 저장됩니다!", (char *)NULL);
        perror("exec");
        exit(1);
    }

    /* 부모: 자식(echo)이 끝날 때까지 대기 */
    wait(NULL);

    /* 파일에 저장된 내용을 cat으로 확인 */
    printf("[부모] %s 파일 내용:\n", filename);
    pid_t cat_pid = fork();
    if (cat_pid == 0) {
        execlp("cat", "cat", filename, (char *)NULL);
        exit(1);
    }
    wait(NULL);
    printf("\n");

    /* 임시 파일 정리 - unlink()는 파일을 삭제한다 */
    unlink(filename);
}

/* ================================================================== */
/* Demo 2: 입력 리다이렉션 (파일 -> stdin)                              */
/* ================================================================== */
static void demo_input_redirect(void)
{
    printf("\n=== Demo 2: 입력 리다이렉션 (wc < input.txt) ===\n\n");

    const char *filename = "/tmp/redirect_demo_input.txt";

    /*
     * 먼저 테스트용 입력 파일을 생성한다.
     * 5개의 과일 이름을 각 줄에 하나씩 쓴다.
     */
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return;
    }
    const char *content = "apple\nbanana\ncherry\ndate\nelderberry\n";
    write(fd, content, strlen(content));
    close(fd);

    printf("[부모] 입력 파일 내용:\n%s\n", content);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /*
         * 자식 프로세스: stdin을 파일로 리다이렉트한 후 wc를 실행한다.
         * 셸에서 "wc -l < input.txt"를 처리하는 원리이다.
         */

        /* 읽기 전용(O_RDONLY)으로 입력 파일을 연다 */
        int infd = open(filename, O_RDONLY);
        if (infd < 0) {
            perror("open");
            exit(1);
        }

        /*
         * dup2(infd, STDIN_FILENO):
         *   STDIN_FILENO(=0, 즉 stdin)이 infd가 가리키는 파일을 가리키게 한다.
         *   이후 프로그램이 stdin에서 읽으면 키보드 대신 파일 내용을 읽게 된다.
         */
        dup2(infd, STDIN_FILENO);
        close(infd);  /* 원본 fd 닫기 */

        /*
         * wc -l 실행: stdin에서 읽어 줄 수를 센다.
         * stdin이 파일로 교체되었으므로 파일의 줄 수(5)를 출력한다.
         *
         * 참고: printf는 stdout(fd 1)을 사용하고, stdin만 리다이렉트했으므로
         *       printf의 출력은 정상적으로 화면에 나타난다.
         */
        printf("[자식] wc -l 실행 결과 (줄 수): ");
        fflush(stdout);  /* printf 버퍼를 즉시 출력 (exec 전에 flush 필요) */
        execlp("wc", "wc", "-l", (char *)NULL);
        perror("exec");
        exit(1);
    }

    wait(NULL);

    /* 임시 파일 정리 */
    unlink(filename);
}

/* ================================================================== */
/* Demo 3: dup() 기본 동작 이해                                        */
/* ================================================================== */
static void demo_dup_basics(void)
{
    printf("\n=== Demo 3: dup() 기본 동작 이해 ===\n\n");

    const char *filename = "/tmp/redirect_demo_dup.txt";

    /* 쓰기용으로 파일을 연다 */
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return;
    }

    printf("원본 fd = %d\n", fd);

    /*
     * dup(fd): fd와 같은 파일을 가리키는 새로운 fd를 반환한다.
     *
     * dup()은 현재 사용 가능한 가장 작은 fd 번호를 할당한다.
     * (0, 1, 2는 이미 stdin/stdout/stderr가 사용 중이므로 보통 3 이상)
     *
     * dup()과 dup2()의 차이:
     *   dup(fd)           -> 커널이 알아서 빈 fd 번호를 배정
     *   dup2(fd, target)  -> 지정한 target 번호로 복제 (리다이렉션에 사용)
     *
     * 복제된 fd와 원본 fd는 같은 파일의 같은 위치(offset)를 공유한다.
     */
    int new_fd = dup(fd);
    printf("dup(%d) = %d (같은 파일을 가리킴)\n", fd, new_fd);

    /*
     * 두 fd 모두 같은 파일에 쓸 수 있다.
     * 같은 오프셋을 공유하므로, 두 번째 write는 첫 번째 이후부터 이어서 쓴다.
     * (데이터가 겹치거나 덮어써지지 않는다)
     */
    const char *msg1 = "Written via original fd\n";
    const char *msg2 = "Written via duplicated fd\n";
    write(fd, msg1, strlen(msg1));      /* 원본 fd로 쓰기 */
    write(new_fd, msg2, strlen(msg2));  /* 복제된 fd로 쓰기 */

    /* 양쪽 fd 모두 닫는다 */
    close(fd);
    close(new_fd);

    /* cat으로 파일 내용을 확인 - 두 메시지 모두 들어있음 */
    printf("\n파일 내용:\n");
    pid_t pid = fork();
    if (pid == 0) {
        execlp("cat", "cat", filename, (char *)NULL);
        exit(1);
    }
    wait(NULL);

    unlink(filename);
}

/* ================================================================== */
/* Demo 4: 셸의 리다이렉션 구현 패턴                                   */
/* ================================================================== */
static void demo_shell_redirect_pattern(void)
{
    printf("\n=== Demo 4: 셸이 리다이렉션을 구현하는 패턴 ===\n\n");

    /*
     * 셸에서 "sort < input > output" 을 실행하는 과정:
     *
     *   1. fork()로 자식 프로세스를 생성한다.
     *   2. 자식에서:
     *      a. input 파일을 열어 stdin(0)으로 dup2    (입력 리다이렉션)
     *      b. output 파일을 열어 stdout(1)으로 dup2  (출력 리다이렉션)
     *      c. exec("sort")를 호출한다.
     *         sort는 stdin에서 읽고 stdout에 쓰므로,
     *         input 파일을 읽어 정렬한 결과를 output 파일에 쓰게 된다.
     *   3. 부모에서 wait()으로 자식 종료를 기다린다.
     *
     * 핵심: dup2()는 exec() 전에 호출해야 한다.
     *       exec() 후에는 새 프로그램이 실행되므로 dup2를 호출할 수 없다.
     *       하지만 fd 테이블은 exec() 후에도 유지되므로
     *       리다이렉트된 stdin/stdout이 그대로 적용된다.
     */

    const char *infile = "/tmp/redirect_demo_sort_in.txt";
    const char *outfile = "/tmp/redirect_demo_sort_out.txt";

    /* 정렬할 입력 파일을 생성 (정렬되지 않은 상태) */
    int fd = open(infile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    const char *data = "cherry\napple\nbanana\ndate\n";
    write(fd, data, strlen(data));
    close(fd);

    printf("입력 파일 (%s):\n%s\n", infile, data);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /*
         * 자식 프로세스: 입력과 출력을 모두 리다이렉트한 후 sort를 실행한다.
         */

        /* --- 입력 리다이렉션: stdin <- infile --- */
        int in_fd = open(infile, O_RDONLY);
        if (in_fd < 0) { perror("open input"); exit(1); }
        dup2(in_fd, STDIN_FILENO);   /* stdin(0)을 입력 파일로 교체 */
        close(in_fd);                /* 원본 fd 닫기 */

        /* --- 출력 리다이렉션: stdout -> outfile --- */
        int out_fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) { perror("open output"); exit(1); }
        dup2(out_fd, STDOUT_FILENO); /* stdout(1)을 출력 파일로 교체 */
        close(out_fd);               /* 원본 fd 닫기 */

        /*
         * sort 실행:
         *   sort는 stdin에서 데이터를 읽어 알파벳 순으로 정렬한 후
         *   stdout에 출력한다. 하지만 stdin과 stdout이 파일로 교체되었으므로
         *   infile에서 읽어 outfile에 쓰게 된다.
         */
        execlp("sort", "sort", (char *)NULL);
        perror("exec");
        exit(1);
    }

    /* 부모: sort가 끝날 때까지 대기 */
    wait(NULL);

    /* 정렬된 출력 파일의 내용 확인 */
    printf("출력 파일 (%s):\n", outfile);
    pid_t cat_pid = fork();
    if (cat_pid == 0) {
        execlp("cat", "cat", outfile, (char *)NULL);
        exit(1);
    }
    wait(NULL);

    /* 임시 파일 정리 */
    unlink(infile);
    unlink(outfile);
}

int main(void)
{
    printf("=== dup2() I/O 리다이렉션 데모 ===\n");
    printf("핵심: dup2(oldfd, newfd) -> newfd가 oldfd와 같은 파일을 가리킴\n");

    demo_output_redirect();
    demo_input_redirect();
    demo_dup_basics();
    demo_shell_redirect_pattern();

    /* 셸 리다이렉션 문법과 그에 대응하는 시스템 콜 요약 */
    printf("\n=== 정리 ===\n");
    printf("  > file  : fd = open(file, O_WRONLY|O_CREAT|O_TRUNC); dup2(fd, 1);\n");
    printf("  < file  : fd = open(file, O_RDONLY);                  dup2(fd, 0);\n");
    printf("  >> file : fd = open(file, O_WRONLY|O_CREAT|O_APPEND); dup2(fd, 1);\n");
    printf("\n  셸 패턴: fork() -> [자식: dup2로 리다이렉트 -> exec()] -> [부모: wait()]\n");

    return 0;
}
