/*
 * producer_consumer.c - xv6 사용자 프로그램
 *
 * [개요]
 *   pipe를 이용한 Producer-Consumer 패턴 데모 프로그램이다.
 *   Producer(부모)가 여러 개의 메시지를 pipe에 쓰고,
 *   Consumer(자식)가 pipe에서 메시지를 읽어 출력한다.
 *
 * [핵심 개념]
 *   - Producer-Consumer 패턴: 한쪽이 데이터를 생산하고 다른 쪽이 소비하는 패턴.
 *     pipe의 내부 버퍼가 bounded buffer 역할을 한다.
 *   - pipe의 blocking 특성이 자동으로 동기화를 제공한다:
 *     (1) Consumer가 읽을 데이터가 없으면 sleep (Producer가 쓸 때까지 대기)
 *     (2) Producer가 버퍼를 가득 채우면 sleep (Consumer가 읽을 때까지 대기)
 *   - 길이-접두사(length-prefix) 프로토콜을 사용하여 메시지 경계를 구분한다.
 *     각 메시지 앞에 1바이트 길이 값을 보내어 수신 측이 정확히 읽을 수 있게 한다.
 *
 * [EOF 처리]
 *   Producer가 write end를 close()하면:
 *   - 커널에서 pipeclose() -> pi->writeopen = 0 -> wakeup(&pi->nread)
 *   - Consumer의 read()가 0을 반환 (EOF)
 *   - Consumer는 EOF를 감지하고 루프를 빠져나와 종료
 *
 * [빌드 방법 (xv6 환경)]
 *   1. 이 파일을 xv6-riscv/user/ 디렉토리에 복사
 *   2. Makefile의 UPROGS에 $U/_producer_consumer\ 추가
 *   3. make clean && make qemu
 */

#include "kernel/types.h"   /* xv6 기본 타입 정의 (uint, uint64 등) */
#include "kernel/stat.h"    /* 파일 상태 구조체 정의 */
#include "user/user.h"      /* xv6 사용자 시스템 콜 및 라이브러리 함수 선언 */

#define NUM_MESSAGES 5   /* Producer가 보낼 메시지 수 */
#define MSG_SIZE 64      /* 메시지 버퍼 최대 크기 */

/*
 * int_to_str: 정수를 문자열로 변환하는 헬퍼 함수
 *
 * xv6에는 sprintf()가 없으므로 직접 구현해야 한다.
 * 정수를 10으로 나누면서 각 자릿수를 추출하고 역순으로 문자열에 저장한다.
 */
static void
int_to_str(int n, char *buf)
{
  if(n == 0){
    buf[0] = '0';
    buf[1] = '\0';
    return;
  }

  char tmp[12];  /* int 최대 자릿수 + 부호 + null */
  int i = 0;
  int neg = 0;

  if(n < 0){
    neg = 1;
    n = -n;
  }

  /* 낮은 자릿수부터 추출 (역순으로 저장됨) */
  while(n > 0){
    tmp[i++] = '0' + (n % 10);
    n /= 10;
  }

  int j = 0;
  if(neg)
    buf[j++] = '-';

  /* 역순으로 저장된 숫자를 정순으로 복사 */
  while(i > 0)
    buf[j++] = tmp[--i];

  buf[j] = '\0';
}

/*
 * make_message: 시퀀스 번호를 포함한 메시지 문자열을 생성한다.
 *
 * 생성되는 형식: "msg N: hello from producer"
 * xv6에는 snprintf()가 없으므로 문자열 파트를 수동으로 이어 붙인다.
 *
 * 반환값: 생성된 메시지의 길이 (null 제외)
 */
static int
make_message(int seq, char *buf, int bufsize)
{
  char num[12];
  int_to_str(seq, num);

  /* 수동 문자열 조합: 각 파트를 순서대로 버퍼에 복사 */
  char *parts[] = {"msg ", num, ": hello from producer"};
  int nparts = 3;
  int pos = 0;

  for(int p = 0; p < nparts; p++){
    for(int k = 0; parts[p][k] != '\0' && pos < bufsize - 1; k++){
      buf[pos++] = parts[p][k];
    }
  }
  buf[pos] = '\0';
  return pos;  /* 메시지 길이 반환 */
}

/*
 * consumer: pipe에서 메시지를 읽어 출력하는 Consumer 함수
 *
 * 프로토콜:
 *   [1바이트 길이][메시지 본문] 형식으로 수신한다.
 *   1. 먼저 1바이트를 읽어 메시지 길이를 파악한다.
 *   2. 해당 길이만큼 메시지 본문을 읽는다.
 *   3. read()가 0을 반환하면 EOF이므로 종료한다.
 *
 * sleep/wakeup 동작:
 *   - read()에서 데이터가 없으면 piperead() 내부에서 sleep(&pi->nread, &pi->lock)
 *   - Producer가 write()하면 wakeup(&pi->nread)으로 Consumer를 깨움
 */
static void
consumer(int readfd)
{
  char buf[MSG_SIZE];
  int n;

  printf("[Consumer] Reading from pipe...\n");

  /* 메시지를 반복적으로 읽는 루프.
   * pipe에서 데이터를 읽을 때:
   *   - 데이터가 있으면 즉시 반환
   *   - 데이터가 없고 writer가 열려 있으면 sleep (blocking)
   *   - 데이터가 없고 writer가 닫혀 있으면 0 반환 (EOF) */
  while(1){
    /* 1단계: 메시지 길이(1 byte)를 읽는다.
     * read() 시스템 콜이 piperead()를 호출한다.
     * pipe가 비어 있으면 sleep하며 Producer가 쓸 때까지 대기한다. */
    char lenbuf;
    n = read(readfd, &lenbuf, 1);
    if(n <= 0)
      break;  /* EOF 또는 에러: 루프 종료 */

    int msglen = (int)lenbuf;
    if(msglen <= 0 || msglen >= MSG_SIZE)
      break;  /* 비정상적인 길이: 안전하게 종료 */

    /* 2단계: 메시지 본문을 읽는다.
     * pipe는 바이트 스트림이므로 한 번의 read()로
     * 요청한 바이트를 모두 받지 못할 수 있다.
     * (예: pipe 버퍼에 일부만 남아있는 경우)
     * 따라서 요청한 길이만큼 반복해서 읽어야 한다. */
    int total = 0;
    while(total < msglen){
      n = read(readfd, buf + total, msglen - total);
      if(n <= 0)
        break;  /* EOF 또는 에러 */
      total += n;
    }
    if(total < msglen)
      break;  /* 메시지가 불완전하면 종료 */

    buf[total] = '\0';  /* 문자열 종료 */
    printf("[Consumer] Received: %s\n", buf);
  }

  /* Producer가 write end를 close()하면 read()가 0을 반환한다.
   * 이것이 pipe를 통한 EOF 전달 메커니즘이다. */
  printf("[Consumer] Pipe closed by producer (read returned 0). Exiting.\n");
}

/*
 * producer: pipe에 메시지를 쓰는 Producer 함수
 *
 * 프로토콜:
 *   [1바이트 길이][메시지 본문] 형식으로 전송한다.
 *   이 길이-접두사 방식으로 Consumer가 메시지 경계를 정확히 파악할 수 있다.
 *
 * sleep/wakeup 동작:
 *   - write()에서 버퍼가 가득 차면 pipewrite() 내부에서 sleep(&pi->nwrite, &pi->lock)
 *   - Consumer가 read()하면 wakeup(&pi->nwrite)으로 Producer를 깨움
 */
static void
producer(int writefd)
{
  char buf[MSG_SIZE];

  printf("[Producer] Sending %d messages through pipe...\n", NUM_MESSAGES);

  for(int i = 0; i < NUM_MESSAGES; i++){
    /* 메시지 문자열 생성 */
    int msglen = make_message(i, buf, MSG_SIZE);

    /* 1단계: 메시지 길이(1 byte)를 먼저 pipe에 쓴다.
     * write() 시스템 콜이 pipewrite()를 호출한다.
     * 버퍼가 가득 차면 sleep하며 Consumer가 읽을 때까지 대기한다. */
    char lenbuf = (char)msglen;
    write(writefd, &lenbuf, 1);

    /* 2단계: 메시지 본문을 pipe에 쓴다.
     * pipewrite() 내부에서 1바이트씩 pi->data 원형 버퍼에 복사한다.
     * 버퍼가 가득 차면(pi->nwrite == pi->nread + PIPESIZE):
     *   -> wakeup(&pi->nread)  -- Consumer를 깨움
     *   -> sleep(&pi->nwrite)  -- Producer 자신은 잠듦
     * Consumer가 데이터를 읽으면 wakeup(&pi->nwrite)으로 깨워준다. */
    write(writefd, buf, msglen);

    printf("[Producer] Sent: %s\n", buf);
  }

  printf("[Producer] Done. Closing write end.\n");
}

/*
 * main: Producer-Consumer 프로그램의 진입점
 *
 * 실행 흐름:
 *   1. pipe()로 통신 채널 생성
 *   2. fork()로 프로세스 분리 (자식=Consumer, 부모=Producer)
 *   3. 각 프로세스에서 사용하지 않는 pipe end를 close()
 *   4. Producer가 메시지를 쓰고, Consumer가 읽음
 *   5. Producer가 write end를 close()하여 EOF 전달
 *   6. 부모가 wait()로 자식 종료 대기
 */
int
main(int argc, char *argv[])
{
  int fds[2];  /* fds[0] = read end, fds[1] = write end */
  int pid;

  /* pipe() 시스템 콜: 커널 내부에서 pipealloc()이 호출되어
   * struct pipe가 할당되고, 두 개의 file descriptor가 반환된다.
   * pipe 내부에는 512바이트 크기의 원형 버퍼(pi->data[PIPESIZE])가 있다. */
  if(pipe(fds) < 0){
    printf("pipe() failed\n");
    exit(1);
  }

  /* fork() 시스템 콜: 현재 프로세스를 복제하여 자식 프로세스를 생성한다.
   * 자식은 부모의 file descriptor 테이블을 복사받으므로
   * 같은 pipe의 양쪽 끝에 접근할 수 있다.
   * 반환값: 부모에게는 자식 pid, 자식에게는 0 */
  pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(1);
  }

  if(pid == 0){
    /* ---- 자식 프로세스 (Consumer) ----
     *
     * write end를 반드시 닫아야 하는 이유:
     *   자식도 write end의 file descriptor를 복사받았으므로,
     *   닫지 않으면 piperead()에서 pi->writeopen이 여전히 1이다.
     *   이 경우 Producer가 close()해도 Consumer는 EOF를 받지 못하고
     *   영원히 sleep 상태에 빠진다. (pi->writeopen > 0이므로 while 탈출 불가) */
    close(fds[1]);

    consumer(fds[0]);  /* pipe에서 메시지를 읽어 출력 */

    close(fds[0]);  /* read end 닫기 */
    exit(0);        /* 자식 프로세스 종료 */
  } else {
    /* ---- 부모 프로세스 (Producer) ----
     *
     * read end를 닫는다. 부모는 데이터를 쓰기만 하므로 read end가 불필요하다. */
    close(fds[0]);

    producer(fds[1]);  /* pipe에 메시지를 쓴다 */

    /* write end를 닫는다.
     * 커널 내부에서 pipeclose()가 호출되어:
     *   1. pi->writeopen = 0        -- write end가 닫혔음을 표시
     *   2. wakeup(&pi->nread)       -- sleep 중인 Consumer를 깨움
     * Consumer는 piperead()의 while 조건에서
     * pi->writeopen == 0이므로 루프를 빠져나오고,
     * 읽을 데이터가 없으면 read()가 0을 반환한다 (EOF). */
    close(fds[1]);

    /* wait() 시스템 콜: 자식 프로세스(Consumer)가 끝나길 기다린다.
     * 커널 내부에서 자식이 아직 살아있으면
     * sleep(p, &wait_lock)으로 부모가 잠든다.
     * 자식이 exit()하면 부모를 wakeup한다. */
    wait(0);
    exit(0);  /* 부모 프로세스 종료. xv6에서는 main에서도 exit() 사용 */
  }
}
