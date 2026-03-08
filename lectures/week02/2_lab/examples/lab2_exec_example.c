/*
 * lab2_exec_example.c - exec() 패밀리 함수들의 동작을 보여주는 예제
 *
 * 이 프로그램은 exec() 시스템 콜을 사용하여 현재 프로세스의 이미지를
 * 새로운 프로그램으로 교체하는 방법을 학습한다.
 *
 * 학습 목표:
 *   1. exec() 패밀리 함수의 종류와 이름 규칙 이해
 *   2. exec() 호출 후 원래 코드가 실행되지 않는 이유 이해
 *   3. fork() + exec() + wait() 패턴 숙달 (셸의 명령어 실행 원리)
 *   4. exec() 실패 시 에러 처리 방법
 *
 * Compile: gcc -Wall -o exec_example lab2_exec_example.c
 * Run:     ./exec_example
 */

#include <stdio.h>      /* printf, perror */
#include <stdlib.h>      /* exit */
#include <string.h>      /* 문자열 처리 함수 */
#include <unistd.h>      /* fork, exec 패밀리, getpid */
#include <sys/wait.h>    /* waitpid, WIFEXITED, WEXITSTATUS */

/*
 * exec() 함수는 현재 프로세스의 코드, 데이터, 힙, 스택 등
 * 모든 메모리 이미지를 새 프로그램으로 완전히 교체한다.
 * exec() 호출이 성공하면, 그 이후의 코드는 절대 실행되지 않는다.
 * (새 프로그램이 처음부터 실행되기 때문이다.)
 *
 * 단, PID는 변하지 않는다 - 같은 프로세스가 다른 프로그램을 실행하는 것이다.
 *
 * exec 패밀리 이름 규칙 (접미사의 의미):
 *   l = list   : 인자를 가변 인자로 하나씩 나열 (execl, execlp, execle)
 *   v = vector : 인자를 문자열 배열(char *argv[])로 전달 (execv, execvp, execvpe)
 *   p = PATH   : PATH 환경변수에서 실행 파일을 검색 (execlp, execvp)
 *                p가 없으면 전체 경로(예: /bin/ls)를 직접 지정해야 함
 *   e = env    : 환경변수를 직접 지정 (execle, execvpe)
 *                e가 없으면 현재 프로세스의 환경변수를 그대로 상속
 *
 * 주요 함수 시그니처:
 *   execl(path, arg0, arg1, ..., NULL)
 *   execlp(file, arg0, arg1, ..., NULL)
 *   execv(path, argv[])
 *   execvp(file, argv[])
 */

/*
 * 헬퍼 함수: fork 후 자식에서 데모 함수를 실행하고, 부모는 wait으로 대기.
 * 이 패턴을 통해 각 데모가 독립적으로 실행되며,
 * exec()이 성공해도 이 프로그램 전체가 끝나지 않도록 보호한다.
 *
 * title: 데모 제목 문자열
 * demo:  자식 프로세스에서 호출할 함수 포인터
 */
static void run_demo(const char *title, void (*demo)(void))
{
    printf("\n--- %s ---\n", title);

    /* fork()로 자식 생성 - exec()은 항상 자식에서 호출하는 것이 안전하다 */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    } else if (pid == 0) {
        /* 자식 프로세스: 데모 함수 실행 */
        demo();
        /*
         * exec()이 성공하면 아래 코드에 절대 도달하지 않는다.
         * exec()이 실패한 경우에만 아래 코드가 실행된다.
         */
        perror("exec failed");
        exit(1);
    } else {
        /*
         * 부모 프로세스: waitpid()로 특정 자식(pid)의 종료를 기다린다.
         * waitpid의 세 번째 인자 0은 블로킹 모드를 의미한다.
         * (WNOHANG을 주면 자식이 아직 실행 중이어도 즉시 반환)
         */
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("(자식 종료, exit code=%d)\n", WEXITSTATUS(status));
        }
    }
}

/* ------------------------------------------------------------------ */
/* Demo 1: execl - 전체 경로 + 인자 나열                               */
/* ------------------------------------------------------------------ */
static void demo_execl(void)
{
    printf("[자식 PID=%d] execl로 /bin/echo 실행\n", getpid());
    /*
     * execl(path, arg0, arg1, ..., NULL)
     *
     * path: 실행할 프로그램의 전체 경로 ("/bin/echo")
     * arg0: 관례상 프로그램 이름 (argv[0]에 해당)
     * arg1~: 프로그램에 전달할 인자들
     * 마지막에 반드시 (char *)NULL로 인자 목록의 끝을 표시해야 한다.
     *
     * 이 호출이 성공하면 현재 프로세스가 /bin/echo로 완전히 교체되어
     * "Hello from execl!"을 출력하고 종료한다.
     */
    execl("/bin/echo", "echo", "Hello", "from", "execl!", (char *)NULL);
    /* execl이 성공하면 이 줄은 절대 실행되지 않음 */
}

/* ------------------------------------------------------------------ */
/* Demo 2: execlp - PATH 검색 + 인자 나열                              */
/* ------------------------------------------------------------------ */
static void demo_execlp(void)
{
    printf("[자식 PID=%d] execlp로 ls 실행 (PATH 검색)\n", getpid());
    /*
     * execlp(file, arg0, arg1, ..., NULL)
     *
     * execl과 비슷하지만, 'p'가 붙었으므로 PATH 환경변수에서 프로그램을 검색한다.
     * 따라서 전체 경로("/bin/ls") 대신 이름만("ls") 지정해도 된다.
     * 셸에서 "ls -l -h /tmp"를 입력하는 것과 동일한 효과이다.
     */
    execlp("ls", "ls", "-l", "-h", "/tmp", (char *)NULL);
}

/* ------------------------------------------------------------------ */
/* Demo 3: execv - 전체 경로 + 인자 배열                               */
/* ------------------------------------------------------------------ */
static void demo_execv(void)
{
    printf("[자식 PID=%d] execv로 /bin/echo 실행 (배열 사용)\n", getpid());
    /*
     * execv(path, argv[])
     *
     * execl과 달리 인자를 가변 인자가 아닌 문자열 배열로 전달한다.
     * argv 배열은 반드시 NULL로 끝나야 한다.
     *
     * 배열 방식은 인자의 개수가 실행 시점에 결정될 때 유용하다.
     * (예: 사용자 입력을 파싱하여 인자를 동적으로 구성하는 경우)
     */
    char *args[] = {"echo", "Hello", "from", "execv!", NULL};
    execv("/bin/echo", args);
}

/* ------------------------------------------------------------------ */
/* Demo 4: execvp - PATH 검색 + 인자 배열 (가장 많이 사용)             */
/* ------------------------------------------------------------------ */
static void demo_execvp(void)
{
    printf("[자식 PID=%d] execvp로 wc 실행\n", getpid());
    /*
     * execvp(file, argv[])
     *
     * PATH 검색(p) + 배열 인자(v)의 조합이다.
     * 실무에서 가장 많이 사용되는 조합인데, 그 이유는:
     *   1. PATH 검색 덕분에 전체 경로를 알 필요가 없다.
     *   2. 배열 방식 덕분에 인자를 동적으로 구성할 수 있다.
     *
     * 셸 구현 시 사용자가 입력한 명령어를 실행할 때 주로 execvp를 사용한다.
     */
    char *args[] = {"echo", "execvp works!", NULL};
    execvp("echo", args);
}

/* ------------------------------------------------------------------ */
/* Demo 5: fork + exec + wait = 새 프로그램 실행의 기본 패턴            */
/* ------------------------------------------------------------------ */
static void demo_pattern(void)
{
    /*
     * 운영체제에서 새 프로그램을 실행하는 표준 패턴:
     *   1단계: fork()  - 자식 프로세스 생성
     *   2단계: exec()  - 자식에서 새 프로그램으로 교체
     *   3단계: wait()  - 부모에서 자식 종료 대기
     *
     * 이것이 Unix/Linux 셸이 명령어를 실행하는 기본 원리이다.
     * 이 데모 자체가 run_demo() 안에서 이미 fork+wait 안에 있으므로
     * 여기서는 exec만 호출하면 된다.
     */
    printf("[자식 PID=%d] fork+exec 패턴: date 명령어 실행\n", getpid());
    execlp("date", "date", (char *)NULL);
}

/* ------------------------------------------------------------------ */
/* Demo 6: exec 실패 처리                                              */
/* ------------------------------------------------------------------ */
static void demo_exec_fail(void)
{
    printf("[자식 PID=%d] 존재하지 않는 프로그램 실행 시도\n", getpid());

    /*
     * 존재하지 않는 프로그램을 exec하면 실패하고 -1을 반환한다.
     * exec이 "실패"한 경우에만 exec 이후의 코드가 실행된다.
     *
     * 실패 후 반드시 exit()을 호출해야 한다.
     * 그렇지 않으면 자식이 부모의 나머지 코드를 계속 실행하게 되어
     * 예상치 못한 동작이 발생할 수 있다.
     *
     * 관례상 exec 실패 시 종료 코드 127을 사용한다 (셸의 관례).
     */
    execlp("this_program_does_not_exist",
           "this_program_does_not_exist", (char *)NULL);

    /* exec 실패 시에만 여기에 도달한다 */
    perror("exec failed (예상된 에러)");
    exit(127);  /* 127: "command not found"를 의미하는 관례적 종료 코드 */
}

int main(void)
{
    printf("=== exec() 패밀리 데모 ===\n");
    printf("현재 프로세스 PID=%d\n", getpid());

    printf("\n핵심 개념:\n");
    printf("  - exec()는 현재 프로세스의 코드/데이터를 새 프로그램으로 교체\n");
    printf("  - exec() 성공 시 호출 이후 코드는 절대 실행되지 않음\n");
    printf("  - 따라서 보통 fork() 후 자식에서 exec()를 호출\n");

    /* 각 데모를 fork+wait으로 감싸서 순차적으로 실행 */
    run_demo("1. execl: 전체 경로 + 인자 나열", demo_execl);
    run_demo("2. execlp: PATH 검색 + 인자 나열", demo_execlp);
    run_demo("3. execv: 전체 경로 + 인자 배열", demo_execv);
    run_demo("4. execvp: PATH 검색 + 인자 배열 (가장 많이 사용)", demo_execvp);
    run_demo("5. fork+exec+wait 패턴: date 실행", demo_pattern);
    run_demo("6. exec 실패 처리", demo_exec_fail);

    /* exec 패밀리 요약 표 */
    printf("\n=== 정리 ===\n");
    printf("  execl(path, arg0, ..., NULL)  - 경로 직접 지정, 인자 나열\n");
    printf("  execlp(file, arg0, ..., NULL) - PATH 검색, 인자 나열\n");
    printf("  execv(path, argv[])           - 경로 직접 지정, 인자 배열\n");
    printf("  execvp(file, argv[])          - PATH 검색, 인자 배열\n");
    printf("\n셸에서 가장 많이 쓰는 패턴: fork() → 자식에서 execvp() → 부모에서 wait()\n");

    return 0;
}
