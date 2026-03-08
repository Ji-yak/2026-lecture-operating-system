/*
 * wakeup_demo.c - xv6 사용자 프로그램
 *
 * [개요]
 *   pipe의 blocking 동작을 통해 xv6 커널의 sleep/wakeup 메커니즘을
 *   실제로 관찰하는 프로그램이다.
 *
 * [핵심 개념]
 *   - sleep(chan, lock): 현재 프로세스를 chan(채널) 주소에서 잠들게 한다.
 *     프로세스 상태가 SLEEPING으로 바뀌고 CPU를 양보한다.
 *   - wakeup(chan): 해당 chan에서 잠든 모든 프로세스를 RUNNABLE로 깨운다.
 *   - pipe는 내부적으로 512바이트 크기의 원형 버퍼(circular buffer)를 사용한다.
 *   - pipe가 비어 있으면 reader가 sleep하고, 가득 차면 writer가 sleep한다.
 *   - writer가 pipe의 write end를 close하면 reader에게 EOF(0)가 전달된다.
 *
 * [IPC(프로세스 간 통신) 관점]
 *   pipe는 xv6에서 제공하는 가장 기본적인 IPC 메커니즘이다.
 *   단방향 바이트 스트림으로, fork()를 통해 부모-자식 간 데이터를 전달한다.
 *   pipe의 blocking 특성 자체가 sleep/wakeup을 활용한 동기화이다.
 *
 * [이 프로그램에서 보여주는 3가지 시나리오]
 *   Demo 1: reader가 빈 pipe에서 block(sleep) -> writer가 데이터를 써서 wakeup
 *   Demo 2: writer가 가득 찬 pipe에서 block(sleep) -> reader가 읽어서 wakeup
 *   Demo 3: writer가 pipe를 close -> reader에게 EOF 전달 (wakeup 후 0 반환)
 *
 * [빌드 방법 (xv6 환경)]
 *   1. 이 파일을 xv6-riscv/user/ 디렉토리에 복사
 *   2. Makefile의 UPROGS에 $U/_wakeup_demo\ 추가
 *   3. make clean && make qemu
 */

#include "kernel/types.h"   /* xv6 기본 타입 정의 (uint, uint64 등) */
#include "kernel/stat.h"    /* 파일 상태 구조체 정의 */
#include "user/user.h"      /* xv6 사용자 시스템 콜 및 라이브러리 함수 선언 */

/* ============================================================
 * Demo 1: Reader가 Writer를 기다리는 모습
 * ============================================================
 * 시나리오:
 *   Reader(자식)가 먼저 read()를 호출하면 pipe가 비어있으므로
 *   커널의 piperead()에서 sleep(&pi->nread, &pi->lock)이 호출된다.
 *   Writer(부모)가 pause() 후 데이터를 쓰면
 *   pipewrite() 안에서 wakeup(&pi->nread)이 호출되어
 *   Reader가 깨어난다.
 *
 * sleep/wakeup 채널:
 *   - &pi->nread: reader가 잠드는 채널 (데이터가 없을 때)
 *   - writer가 데이터를 쓴 후 wakeup(&pi->nread)으로 reader를 깨움
 * ============================================================ */
static void
demo_reader_waits(void)
{
  int fds[2];   /* fds[0]: read end, fds[1]: write end */
  int pid;

  printf("=== Demo 1: Reader blocks until Writer sends data ===\n");

  /* pipe() 시스템 콜: 커널에서 pipealloc()을 호출하여
   * struct pipe를 할당하고, read/write용 file descriptor 2개를 반환한다.
   * pipe 내부 버퍼 크기: PIPESIZE = 512 bytes */
  if(pipe(fds) < 0){
    printf("pipe() failed\n");
    exit(1);   /* xv6의 exit(): 프로세스를 종료하고 자원을 해제 */
  }

  /* fork() 시스템 콜: 현재 프로세스를 복제하여 자식 프로세스를 생성한다.
   * 자식은 부모의 파일 디스크립터 테이블을 그대로 복사받으므로
   * 같은 pipe의 양쪽 끝(read end, write end)에 접근할 수 있다.
   * 반환값: 부모에게는 자식의 pid, 자식에게는 0 */
  pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(1);
  }

  if(pid == 0){
    /* ---- 자식 프로세스 (Reader) ---- */

    /* close() 시스템 콜: 사용하지 않는 write end를 닫는다.
     * 자식이 write end를 닫지 않으면, 나중에 부모가 write end를 닫아도
     * pi->writeopen이 여전히 1로 남아 EOF를 받지 못할 수 있다. */
    close(fds[1]);

    printf("[Reader] Calling read()... (pipe is empty, will block)\n");

    /* 이 시점에서 pipe 버퍼는 비어있다.
     * 커널 내부 동작 (piperead 함수):
     *   1. acquire(&pi->lock)  -- pipe 구조체에 대한 스핀락 획득
     *   2. while(pi->nread == pi->nwrite && pi->writeopen)
     *        -> 조건 true (버퍼 비어있고, writer가 아직 열려있음)
     *        -> sleep(&pi->nread, &pi->lock) 호출:
     *             a. acquire(&p->lock)      -- 프로세스 락 획득
     *             b. release(&pi->lock)     -- pipe 락 해제 (writer가 진행 가능)
     *             c. p->chan = &pi->nread    -- 잠드는 채널 주소 설정
     *             d. p->state = SLEEPING    -- 프로세스 상태 변경
     *             e. sched()                -- CPU 양보, 스케줄러로 전환
     *
     * Reader는 여기서 멈추고 Writer가 깨워줄 때까지 대기한다. */

    char buf[32];
    /* read() 시스템 콜: pipe에서 데이터를 읽는다.
     * 데이터가 없으면 위 과정을 통해 sleep하고,
     * writer가 데이터를 쓰면 wakeup으로 깨어나 데이터를 복사한다. */
    int n = read(fds[0], buf, sizeof(buf) - 1);

    /* Writer가 write()를 호출하면 pipewrite()에서 wakeup(&pi->nread) 발생:
     *   1. wakeup()이 전체 프로세스 테이블(proc[NPROC])을 순회
     *   2. p->state == SLEEPING && p->chan == &pi->nread 인 프로세스 찾음
     *   3. p->state = RUNNABLE로 변경 (실행 가능 상태)
     *   4. 스케줄러가 Reader를 다시 실행
     *   5. sleep()에서 sched() 다음으로 복귀
     *   6. piperead()의 while 루프 재확인 -> 데이터 있으므로 루프 탈출
     *   7. for 루프에서 버퍼의 데이터를 사용자 공간으로 복사 */

    if(n > 0){
      buf[n] = '\0';
      printf("[Reader] Woke up! Received %d bytes: \"%s\"\n", n, buf);
    }

    close(fds[0]);  /* read end를 닫아 pipe 자원 해제 */
    exit(0);        /* 자식 프로세스 종료 */
  } else {
    /* ---- 부모 프로세스 (Writer) ---- */

    close(fds[0]);  /* 사용하지 않는 read end를 닫는다 */

    /* pause(n): xv6에서 n tick 동안 프로세스를 일시 정지시킨다.
     * 일부러 지연을 주어 Reader가 먼저 read()를 호출하고
     * sleep 상태에 들어가도록 유도한다. */
    printf("[Writer] Waiting 10 ticks before sending data...\n");
    pause(10);

    char *msg = "wakeup!";
    printf("[Writer] Writing \"%s\" to pipe...\n", msg);

    /* write() 시스템 콜 호출 시 커널 내부 동작 (pipewrite 함수):
     *   1. acquire(&pi->lock)           -- pipe 락 획득
     *   2. 1바이트씩 pi->data 원형 버퍼에 복사
     *   3. wakeup(&pi->nread)           -- Reader를 깨움!
     *   4. release(&pi->lock)           -- pipe 락 해제 */
    write(fds[1], msg, strlen(msg));

    printf("[Writer] Data sent. Reader should wake up now.\n");

    close(fds[1]);  /* write end를 닫는다 */

    /* wait() 시스템 콜: 자식 프로세스가 종료될 때까지 대기한다.
     * 자식이 아직 실행 중이면 부모는 sleep(&wait_lock)으로 잠든다.
     * 자식이 exit()하면 부모를 wakeup한다. */
    wait(0);
    printf("=== Demo 1 complete ===\n\n");
  }
}

/* ============================================================
 * Demo 2: 큰 데이터 전송 시 Writer blocking
 * ============================================================
 * 시나리오:
 *   pipe 버퍼 크기는 512 bytes (PIPESIZE)이다.
 *   Writer가 512 bytes 이상을 쓰려 하면 버퍼가 가득 차서
 *   pipewrite()에서 sleep(&pi->nwrite, &pi->lock)이 호출된다.
 *   Reader가 데이터를 읽으면 wakeup(&pi->nwrite)으로
 *   Writer가 깨어나서 나머지를 쓴다.
 *
 * sleep/wakeup 채널:
 *   - &pi->nwrite: writer가 잠드는 채널 (버퍼가 가득 찼을 때)
 *   - reader가 데이터를 읽은 후 wakeup(&pi->nwrite)으로 writer를 깨움
 * ============================================================ */
static void
demo_writer_blocks(void)
{
  int fds[2];
  int pid;

  printf("=== Demo 2: Writer blocks when pipe buffer is full ===\n");

  /* pipe() 시스템 콜로 새로운 pipe 생성 */
  if(pipe(fds) < 0){
    printf("pipe() failed\n");
    exit(1);
  }

  /* fork()로 자식(Reader)과 부모(Writer) 분리 */
  pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(1);
  }

  if(pid == 0){
    /* ---- 자식 프로세스 (Reader) - 일부러 천천히 읽는다 ---- */
    close(fds[1]);  /* 사용하지 않는 write end 닫기 */

    /* Reader가 먼저 10 tick 대기하여
     * Writer가 512바이트 버퍼를 가득 채우고 block(sleep)되게 한다. */
    printf("[Reader] Waiting 10 ticks before reading...\n");
    pause(10);

    char buf[128];
    int total = 0;
    int n;

    printf("[Reader] Starting to read (Writer may be blocked)...\n");

    /* read()를 반복 호출하여 pipe의 모든 데이터를 읽는다.
     * read()가 0을 반환하면 EOF이므로 루프를 빠져나온다.
     * Reader가 데이터를 읽을 때마다 커널 내부에서:
     *   piperead() -> pi->nread 증가 -> wakeup(&pi->nwrite)
     * 이로 인해 sleep 중이던 Writer가 RUNNABLE로 깨어난다. */
    while((n = read(fds[0], buf, sizeof(buf))) > 0){
      total += n;
    }

    printf("[Reader] Total bytes read: %d\n", total);

    close(fds[0]);  /* read end 닫기 */
    exit(0);
  } else {
    /* ---- 부모 프로세스 (Writer) - 큰 데이터를 한번에 쓴다 ---- */
    close(fds[0]);  /* 사용하지 않는 read end 닫기 */

    /* 600 bytes를 쓴다 (PIPESIZE=512보다 88바이트 더 큼)
     * 알파벳 문자로 채운 버퍼를 사용 */
    char bigbuf[600];
    for(int i = 0; i < 600; i++){
      bigbuf[i] = 'A' + (i % 26);
    }

    printf("[Writer] Trying to write 600 bytes (PIPESIZE=512)...\n");

    /* write() 시스템 콜 호출 시 커널 내부 동작 (pipewrite 함수):
     *   1. pipewrite() 진입, acquire(&pi->lock)
     *   2. 1 byte씩 while 루프에서 pi->data 원형 버퍼에 복사
     *   3. 512 bytes 쓴 후: pi->nwrite == pi->nread + PIPESIZE (버퍼 가득 참)
     *   4. wakeup(&pi->nread)              -- Reader를 깨움
     *   5. sleep(&pi->nwrite, &pi->lock)   -- Writer 자신은 잠듦
     *
     *   이후 Reader가 데이터를 읽으면:
     *   6. piperead() 안에서 pi->nread 증가 (버퍼 공간 확보)
     *   7. wakeup(&pi->nwrite)             -- Writer를 깨움!
     *   8. Writer가 깨어나서 나머지 88 bytes를 쓴다 */

    int written = write(fds[1], bigbuf, 600);
    printf("[Writer] Successfully wrote %d bytes\n", written);

    close(fds[1]);  /* write end를 닫아 Reader에게 EOF 신호 전달 */

    /* wait()로 자식(Reader)의 종료를 대기 */
    wait(0);
    printf("=== Demo 2 complete ===\n\n");
  }
}

/* ============================================================
 * Demo 3: Pipe close와 EOF
 * ============================================================
 * 시나리오:
 *   Writer가 write end를 닫으면 pipeclose()가 호출되어
 *   pi->writeopen = 0이 되고 wakeup(&pi->nread)이 호출된다.
 *   Reader는 piperead()의 while 조건에서 pi->writeopen == 0이므로
 *   루프를 빠져나오고, 버퍼가 비어있으면 0을 반환한다 (EOF).
 *
 * EOF 처리의 핵심:
 *   piperead()의 while 조건: (pi->nread == pi->nwrite && pi->writeopen)
 *   - 버퍼가 비어 있고(nread == nwrite) writer가 열려 있으면 -> sleep (대기)
 *   - 버퍼가 비어 있고 writer가 닫혀 있으면(writeopen == 0) -> 0 반환 (EOF)
 *   이렇게 write end의 close가 곧 EOF 신호가 된다.
 * ============================================================ */
static void
demo_pipe_close(void)
{
  int fds[2];
  int pid;

  printf("=== Demo 3: Pipe close triggers EOF for reader ===\n");

  /* pipe() 시스템 콜로 pipe 생성 */
  if(pipe(fds) < 0){
    printf("pipe() failed\n");
    exit(1);
  }

  /* fork()로 자식(Reader)과 부모(Writer) 분리 */
  pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(1);
  }

  if(pid == 0){
    /* ---- 자식 프로세스 (Reader) ---- */
    close(fds[1]);  /* 사용하지 않는 write end 닫기 (EOF 수신을 위해 필수!) */

    char buf[32];
    int n;

    printf("[Reader] Waiting for data...\n");

    /* 첫 번째 read(): 데이터가 올 때까지 block(sleep)한다.
     * Writer가 데이터를 쓰면 wakeup으로 깨어나 데이터를 수신한다. */
    n = read(fds[0], buf, sizeof(buf) - 1);
    if(n > 0){
      buf[n] = '\0';
      printf("[Reader] Got: \"%s\"\n", buf);
    }

    printf("[Reader] Calling read() again... waiting for more data or EOF\n");

    /* 두 번째 read(): Writer가 close()하면 EOF (0 반환)
     * 커널 내부 동작 (piperead 함수):
     *   1. acquire(&pi->lock)
     *   2. while(pi->nread == pi->nwrite && pi->writeopen)
     *      -> pi->writeopen == 0이므로 while 조건이 false
     *      -> 루프를 즉시 빠져나옴 (sleep하지 않음)
     *   3. for 루프: pi->nread == pi->nwrite이므로 복사할 데이터 없음
     *   4. return 0  -- 이것이 EOF 신호이다 */
    n = read(fds[0], buf, sizeof(buf) - 1);
    printf("[Reader] read() returned %d (0 = EOF)\n", n);

    close(fds[0]);  /* read end 닫기 */
    exit(0);
  } else {
    /* ---- 부모 프로세스 (Writer) ---- */
    close(fds[0]);  /* 사용하지 않는 read end 닫기 */

    /* 데이터 하나 보낸다 */
    char *msg = "last message";
    /* write() 시스템 콜: pipe에 메시지를 쓰고 reader를 wakeup */
    write(fds[1], msg, strlen(msg));
    printf("[Writer] Sent \"%s\"\n", msg);

    /* 잠시 대기하여 Reader가 첫 번째 메시지를 읽고
     * 두 번째 read()에서 다시 sleep에 들어갈 시간을 준다 */
    pause(10);
    printf("[Writer] Closing write end of pipe...\n");

    /* close() 시스템 콜 -> 커널 내부에서 pipeclose() 호출:
     *   1. acquire(&pi->lock)
     *   2. pi->writeopen = 0          -- write end가 닫혔음을 표시
     *   3. wakeup(&pi->nread)         -- sleep 중인 Reader를 깨움
     *   4. release(&pi->lock)
     *   Reader는 깨어나서 while 조건을 재확인하고,
     *   pi->writeopen == 0이므로 루프를 탈출하여 0(EOF)을 반환한다 */
    close(fds[1]);

    /* wait()로 자식(Reader)의 종료를 대기 */
    wait(0);
    printf("=== Demo 3 complete ===\n\n");
  }
}

/* main: 세 가지 데모를 순서대로 실행한다 */
int
main(int argc, char *argv[])
{
  printf("\n");
  printf("========================================\n");
  printf("  Sleep/Wakeup Demo via Pipe Blocking\n");
  printf("========================================\n\n");

  demo_reader_waits();   /* Demo 1: Reader가 빈 pipe에서 sleep -> Writer가 wakeup */
  demo_writer_blocks();  /* Demo 2: Writer가 가득 찬 pipe에서 sleep -> Reader가 wakeup */
  demo_pipe_close();     /* Demo 3: Writer close -> Reader에게 EOF 전달 */

  printf("All demos completed.\n");
  exit(0);  /* xv6에서는 main에서도 exit()을 사용한다 (return 불가) */
}
