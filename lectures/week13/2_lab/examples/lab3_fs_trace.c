/*
 * fs_trace.c - xv6 파일 시스템 연산 추적 프로그램
 *
 * [개요]
 * 파일 생성, 쓰기, 닫기, 다시 열기, 읽기, 검증까지
 * 각 단계를 명확하게 출력하여 파일 시스템 동작을 이해합니다.
 * 이 프로그램은 xv6 사용자 프로그램으로, xv6 내부에서 실행됩니다.
 *
 * [xv6 파일 시스템 계층 구조]
 *
 *   사용자 프로그램   : open(), read(), write(), close(), mkdir(), unlink()
 *        |
 *   시스템 콜 계층    : sys_open(), sys_read(), sys_write(), sys_close() ...
 *        |
 *   파일 디스크립터   : file.c - struct file, fileread(), filewrite()
 *        |               fd -> struct file -> struct inode
 *   경로 해석 계층    : fs.c - namei(), namex(), dirlookup(), dirlink()
 *        |               경로 문자열을 inode로 변환 (예: "/testdir/inner.txt")
 *   inode 계층        : fs.c - ialloc(), iget(), ilock(), iput(), readi(), writei()
 *        |               디스크 inode와 메모리 inode 관리
 *   로그 계층         : log.c - begin_op(), end_op(), log_write(), commit()
 *        |               WAL(Write-Ahead Logging)로 crash consistency 보장
 *   버퍼 캐시 계층    : bio.c - bread(), bwrite(), brelse()
 *        |               디스크 블록을 메모리에 캐시, LRU 교체 정책
 *   디스크 드라이버   : virtio_disk.c - virtio_disk_rw()
 *                       VirtIO 인터페이스로 실제 디스크 I/O 수행
 *
 * [각 계층의 역할]
 *   - Buffer Cache (bio.c): 디스크 블록을 메모리에 캐시하여 I/O 횟수를 줄임
 *     bcache.lock(spinlock)으로 리스트를 보호하고, buf.lock(sleeplock)으로 데이터 보호
 *   - Log (log.c): 모든 디스크 수정을 트랜잭션으로 묶어 원자성 보장
 *     crash 후 복구 시 로그를 재생(replay)하여 일관성 유지
 *   - Inode (fs.c): 파일의 메타데이터(크기, 타입, 블록 포인터)를 관리
 *     메모리 inode(struct inode)는 참조 카운트(ref)로 수명 관리
 *   - Directory (fs.c): 이름 -> inode 번호 매핑을 디렉토리 엔트리로 관리
 *     각 엔트리는 struct dirent (inum + name[14])
 *
 * 빌드 방법:
 *   1. 이 파일을 xv6-riscv/user/fs_trace.c 에 복사
 *   2. Makefile의 UPROGS에 $U/_fs_trace 추가
 *   3. make qemu 로 빌드 및 실행
 *
 * xv6 쉘에서 실행:
 *   $ fs_trace
 */

/* xv6 헤더 파일 포함
 * kernel/types.h: uint, uint64 등 기본 타입 정의
 * kernel/stat.h : struct stat 정의 (type, ino, nlink, size)
 * user/user.h   : 시스템 콜 래퍼 함수 선언 (open, read, write, close, mkdir 등)
 * kernel/fcntl.h: 파일 열기 플래그 정의 (O_RDONLY, O_WRONLY, O_RDWR, O_CREATE) */
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

/* 구분선 출력 (시각적 구분용) */
void
print_separator(void)
{
  printf("--------------------------------------------------\n");
}

/*
 * print_stat - 파일의 메타데이터(inode 정보)를 출력한다.
 *
 * fstat() 시스템 콜은 열린 파일의 struct stat을 반환한다.
 * xv6의 struct stat (kernel/stat.h):
 *   - type  : 파일 유형 (T_DIR=1, T_FILE=2, T_DEVICE=3)
 *   - ino   : inode 번호 (디스크 inode 배열의 인덱스)
 *   - nlink : 하드 링크 수 (이 inode를 가리키는 디렉토리 엔트리 수)
 *             nlink가 0이 되면 inode와 데이터 블록이 해제됨
 *   - size  : 파일 크기 (bytes)
 */
void
print_stat(char *path)
{
  struct stat st;
  int fd;

  /* 파일을 열어서 fd를 얻은 후 fstat으로 메타데이터 조회
   * xv6 내부: sys_fstat() -> stati() -> inode 정보를 stat 구조체에 복사 */
  fd = open(path, O_RDONLY);
  if(fd < 0){
    printf("  [ERROR] open(\"%s\") 실패\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    printf("  [ERROR] fstat 실패\n");
    close(fd);
    return;
  }

  /* inode에 저장된 메타데이터 출력 */
  printf("  파일 정보:\n");
  printf("    - type  : %d ", st.type);
  if(st.type == 1) printf("(T_DIR)\n");       /* 디렉토리 */
  else if(st.type == 2) printf("(T_FILE)\n"); /* 일반 파일 */
  else if(st.type == 3) printf("(T_DEVICE)\n"); /* 장치 파일 (console 등) */
  else printf("(unknown)\n");
  printf("    - ino   : %d\n", st.ino);   /* inode 번호: 디스크 inode 배열의 인덱스 */
  printf("    - nlink : %d\n", st.nlink); /* 하드 링크 수 */
  printf("    - size  : %d bytes\n", st.size); /* 파일 크기 */

  close(fd);
}

int
main(int argc, char *argv[])
{
  int fd;                              /* 파일 디스크립터 (프로세스별 fd 테이블의 인덱스) */
  int n;                               /* 읽기/쓰기 바이트 수 */
  char buf[128];                       /* 읽기 버퍼 */
  char *testfile = "testfile.txt";     /* 테스트용 파일 이름 */
  char *testdir  = "testdir";          /* 테스트용 디렉토리 이름 */
  char *msg1 = "Hello, xv6 file system!\n";  /* 첫 번째 쓰기 데이터 */
  char *msg2 = "Second write to file.\n";    /* 두 번째 쓰기 데이터 */

  (void)argc;  /* 미사용 인자 경고 억제 */
  (void)argv;

  printf("==================================================\n");
  printf("  xv6 파일 시스템 연산 추적 프로그램\n");
  printf("==================================================\n\n");

  /* --------------------------------------------------------
   * 단계 1: 파일 생성
   * sys_open()에 O_CREATE 플래그가 있으면 create() 경로로 진입
   * create()는 새 inode를 할당하고 부모 디렉토리에 엔트리를 추가
   *
   * ialloc(): 디스크의 inode 블록 영역을 순회하며 type==0인
   *           빈 inode를 찾아 할당 (type을 T_FILE로 설정)
   * dirlink(): 부모 디렉토리의 데이터 블록에 새 dirent를 추가
   *            (inum + name[14] = 16바이트 엔트리)
   * -------------------------------------------------------- */
  printf("[단계 1] 파일 생성: open(\"%s\", O_CREATE|O_RDWR)\n", testfile);
  printf("  호출 경로:\n");
  printf("    sys_open() -> create() -> nameiparent()\n");
  printf("                            -> ialloc()  : 빈 inode 할당\n");
  printf("                            -> dirlink() : 디렉토리에 엔트리 추가\n");
  print_separator();

  fd = open(testfile, O_CREATE | O_RDWR);
  if(fd < 0){
    printf("  [ERROR] 파일 생성 실패!\n");
    exit(1);
  }

  printf("  성공! fd = %d\n", fd);
  print_stat(testfile);
  printf("\n");

  /* --------------------------------------------------------
   * 단계 2: 파일에 데이터 쓰기 (첫 번째)
   *
   * 쓰기의 핵심 경로: filewrite() -> writei() -> bmap() -> log_write()
   *
   * bmap(ip, bn): 논리 블록 번호(bn)를 물리 블록 번호로 매핑
   *   - bn < NDIRECT(12): 직접 블록 포인터 (ip->addrs[bn])
   *   - bn >= NDIRECT: 간접 블록을 통해 매핑
   *   - 아직 할당되지 않은 블록이면 balloc()으로 새 블록 할당
   *
   * log_write(): 수정된 블록을 즉시 디스크에 쓰지 않고 로그에 기록
   *   - 트랜잭션(begin_op/end_op) 안에서만 호출 가능
   *   - end_op()에서 commit() 시 로그 -> 실제 위치로 복사
   * -------------------------------------------------------- */
  printf("[단계 2] 첫 번째 쓰기: write(fd, \"%s\", %d)\n",
         "Hello, xv6 file system!\\n", strlen(msg1));
  printf("  호출 경로:\n");
  printf("    sys_write() -> filewrite()\n");
  printf("      -> begin_op()          : 트랜잭션 시작\n");
  printf("      -> ilock()             : inode 잠금\n");
  printf("      -> writei()\n");
  printf("         -> bmap()           : 논리 블록 -> 물리 블록 매핑\n");
  printf("            -> balloc()      : 새 데이터 블록 할당 (처음 쓰기)\n");
  printf("         -> bread()          : 블록 읽기\n");
  printf("         -> either_copyin()  : 사용자 데이터 -> 버퍼 복사\n");
  printf("         -> log_write()      : 로그에 기록 예약\n");
  printf("         -> iupdate()        : inode 크기 갱신\n");
  printf("      -> iunlock()           : inode 잠금 해제\n");
  printf("      -> end_op()            : 트랜잭션 종료 -> commit\n");
  print_separator();

  n = write(fd, msg1, strlen(msg1));
  if(n != strlen(msg1)){
    printf("  [ERROR] 쓰기 실패! (wrote %d bytes)\n", n);
    close(fd);
    exit(1);
  }

  printf("  성공! %d bytes 기록\n", n);
  print_stat(testfile);
  printf("\n");

  /* --------------------------------------------------------
   * 단계 3: 파일에 데이터 쓰기 (두 번째)
   * 첫 번째 쓰기로 데이터 블록이 이미 할당되었으므로,
   * bmap()은 balloc() 없이 기존 블록 번호를 즉시 반환한다.
   * 파일 오프셋(f->off)은 첫 번째 쓰기 후 자동으로 전진되어
   * 두 번째 쓰기는 같은 블록 내 다음 위치에 기록된다.
   * (xv6 블록 크기는 1024바이트이므로 두 메시지가 한 블록에 들어감)
   * -------------------------------------------------------- */
  printf("[단계 3] 두 번째 쓰기: write(fd, \"%s\", %d)\n",
         "Second write to file.\\n", strlen(msg2));
  printf("  (이번에는 bmap()이 이미 할당된 블록을 반환합니다)\n");
  print_separator();

  n = write(fd, msg2, strlen(msg2));
  if(n != strlen(msg2)){
    printf("  [ERROR] 쓰기 실패! (wrote %d bytes)\n", n);
    close(fd);
    exit(1);
  }

  printf("  성공! %d bytes 기록\n", n);
  print_stat(testfile);
  printf("\n");

  /* --------------------------------------------------------
   * 단계 4: 파일 닫기
   *
   * xv6 파일 관련 구조체 계층:
   *   프로세스: p->ofile[fd] -> struct file (전역 파일 테이블)
   *   struct file: { type, ref, readable, writable, ip, off }
   *     - ref: 이 파일 구조체를 참조하는 fd 수 (fork/dup으로 증가)
   *     - ip : 이 파일이 가리키는 inode
   *     - off: 현재 읽기/쓰기 위치
   *
   * close 시 f->ref를 감소시키고, 0이 되면:
   *   - iput(ip): inode의 참조 카운트(ref)를 감소
   *   - inode ref가 0이고 nlink가 0이면 itrunc()로 데이터 블록 해제
   * -------------------------------------------------------- */
  printf("[단계 4] 파일 닫기: close(fd=%d)\n", fd);
  printf("  호출 경로:\n");
  printf("    sys_close() -> fileclose()\n");
  printf("      -> f->ref 감소 (0이 되면):\n");
  printf("         -> begin_op()\n");
  printf("         -> iput(ip)    : inode 참조 감소\n");
  printf("         -> end_op()\n");
  print_separator();

  close(fd);
  printf("  성공! fd %d 닫힘\n\n", fd);

  /* --------------------------------------------------------
   * 단계 5: 파일 다시 열기 (O_CREATE 없이)
   *
   * O_CREATE 없이 open하면 create() 대신 namei() 경로로 진입:
   *   namei() -> namex(): 경로의 각 구성요소를 순회
   *     - 각 디렉토리에서 dirlookup()으로 이름 검색
   *     - dirlookup(): 디렉토리의 데이터 블록을 순회하며
   *       struct dirent의 name 필드와 비교
   *   ilock(): 메모리 inode가 디스크 내용을 아직 읽지 않았다면
   *     bread()로 디스크에서 읽어옴 (ip->valid == 0일 때)
   * -------------------------------------------------------- */
  printf("[단계 5] 파일 다시 열기: open(\"%s\", O_RDONLY)\n", testfile);
  printf("  호출 경로 (O_CREATE 없음):\n");
  printf("    sys_open() -> namei(\"%s\")\n", testfile);
  printf("      -> namex() -> dirlookup() : 디렉토리에서 이름 검색\n");
  printf("    -> ilock()                   : inode 잠금 + 디스크에서 읽기\n");
  printf("    -> filealloc() + fdalloc()   : 파일 테이블/fd 할당\n");
  print_separator();

  fd = open(testfile, O_RDONLY);
  if(fd < 0){
    printf("  [ERROR] 파일 열기 실패!\n");
    exit(1);
  }

  printf("  성공! fd = %d\n\n", fd);

  /* --------------------------------------------------------
   * 단계 6: 파일 읽기
   *
   * 읽기 경로의 핵심: readi() -> bmap() -> bread()
   *   bmap(): 파일의 논리 블록 번호를 디스크의 물리 블록 번호로 변환
   *   bread(): buffer cache에서 해당 블록을 찾거나,
   *            캐시 미스 시 디스크에서 읽어 캐시에 저장
   *
   * 읽기는 디스크를 수정하지 않으므로 트랜잭션(begin_op/end_op)이 불필요
   * buffer cache 덕분에 최근 읽은 블록은 디스크 I/O 없이 즉시 반환됨
   * -------------------------------------------------------- */
  printf("[단계 6] 파일 읽기: read(fd, buf, sizeof(buf))\n");
  printf("  호출 경로:\n");
  printf("    sys_read() -> fileread()\n");
  printf("      -> ilock()          : inode 잠금\n");
  printf("      -> readi()\n");
  printf("         -> bmap()        : 논리 블록 -> 물리 블록\n");
  printf("         -> bread()       : 블록 읽기 (buffer cache 확인)\n");
  printf("         -> either_copyout() : 버퍼 -> 사용자 공간 복사\n");
  printf("      -> f->off 갱신\n");
  printf("      -> iunlock()\n");
  printf("  (주의: read는 트랜잭션 불필요 - 디스크 수정 없음)\n");
  print_separator();

  memset(buf, 0, sizeof(buf));
  n = read(fd, buf, sizeof(buf) - 1);
  if(n < 0){
    printf("  [ERROR] 읽기 실패!\n");
    close(fd);
    exit(1);
  }

  buf[n] = '\0';
  printf("  성공! %d bytes 읽음\n", n);
  printf("  내용: \"%s\"\n\n", buf);

  /* --------------------------------------------------------
   * 단계 7: 데이터 검증
   * -------------------------------------------------------- */
  printf("[단계 7] 데이터 검증\n");
  print_separator();

  /* 전체 예상 데이터 구성 */
  char expected[128];
  char *p = expected;
  /* msg1 복사 */
  char *s = msg1;
  while(*s)
    *p++ = *s++;
  /* msg2 복사 */
  s = msg2;
  while(*s)
    *p++ = *s++;
  *p = '\0';

  int expected_len = strlen(expected);

  if(n == expected_len){
    /* 바이트 단위 비교 */
    int match = 1;
    int i;
    for(i = 0; i < n; i++){
      if(buf[i] != expected[i]){
        match = 0;
        break;
      }
    }
    if(match){
      printf("  검증 성공! 읽은 데이터가 쓴 데이터와 일치합니다.\n");
      printf("  (write -> log -> disk -> buffer cache -> read 경로 확인)\n");
    } else {
      printf("  검증 실패! 데이터 불일치 (offset %d)\n", i);
    }
  } else {
    printf("  검증 실패! 길이 불일치 (expected %d, got %d)\n",
           expected_len, n);
  }

  close(fd);
  printf("\n");

  /* --------------------------------------------------------
   * 단계 8: 디렉토리 생성
   *
   * 디렉토리는 특수한 파일: type=T_DIR인 inode + 디렉토리 엔트리 데이터
   * 새 디렉토리 생성 시 자동으로 두 개의 엔트리가 추가됨:
   *   "."  : 자기 자신을 가리키는 엔트리 (inum = 자신의 inode 번호)
   *   ".." : 부모 디렉토리를 가리키는 엔트리 (inum = 부모의 inode 번호)
   * 부모 디렉토리의 nlink도 1 증가 (".."이 부모를 가리키므로)
   *
   * sys_mkdir() -> create(path, T_DIR, 0, 0)
   *   -> ialloc + dirlink(".", ip) + dirlink("..", dp)
   * -------------------------------------------------------- */
  printf("[단계 8] 디렉토리 생성: mkdir(\"%s\")\n", testdir);
  printf("  호출 경로:\n");
  printf("    sys_mkdir() -> create(\"%s\", T_DIR)\n", testdir);
  printf("      -> ialloc()         : 새 inode 할당 (type=T_DIR)\n");
  printf("      -> dirlink(\".\")    : 자기 자신 엔트리\n");
  printf("      -> dirlink(\"..\")   : 부모 디렉토리 엔트리\n");
  printf("      -> dirlink(dp, \"%s\") : 부모에 새 디렉토리 등록\n", testdir);
  printf("      -> dp->nlink++      : 부모의 링크 수 증가 (\"..\" 때문)\n");
  print_separator();

  if(mkdir(testdir) < 0){
    printf("  [ERROR] 디렉토리 생성 실패!\n");
  } else {
    printf("  성공!\n");
    print_stat(testdir);
  }
  printf("\n");

  /* --------------------------------------------------------
   * 단계 9: 디렉토리 안에 파일 생성
   *
   * 경로 해석(path resolution) 과정:
   *   namex() 함수가 경로를 "/" 기준으로 분리하여 순회
   *   "testdir/inner.txt" 처리:
   *     1. skipelem()으로 "testdir" 추출
   *     2. 현재 디렉토리에서 dirlookup("testdir")으로 inode 찾기
   *     3. skipelem()으로 "inner.txt" 추출
   *     4. testdir 디렉토리에서 create("inner.txt", T_FILE) 수행
   * -------------------------------------------------------- */
  char *nested = "testdir/inner.txt";
  printf("[단계 9] 중첩 파일 생성: open(\"%s\", O_CREATE|O_RDWR)\n", nested);
  printf("  경로 해석 과정 (namex):\n");
  printf("    \"%s\" -> skipelem -> \"testdir\" -> dirlookup\n", nested);
  printf("                     -> skipelem -> \"inner.txt\" -> create\n");
  print_separator();

  fd = open(nested, O_CREATE | O_RDWR);
  if(fd < 0){
    printf("  [ERROR] 중첩 파일 생성 실패!\n");
  } else {
    char *inner_msg = "nested file content\n";
    write(fd, inner_msg, strlen(inner_msg));
    printf("  성공! fd = %d\n", fd);
    print_stat(nested);
    close(fd);
  }
  printf("\n");

  /* --------------------------------------------------------
   * 단계 10: 정리 (unlink)
   *
   * unlink는 디렉토리 엔트리를 삭제하고 nlink를 감소시킨다.
   * nlink가 0이 되면:
   *   - 즉시 삭제되는 것이 아님!
   *   - iput()에서 ref가 0이고 nlink가 0일 때 비로소:
   *     a) itrunc(): 파일의 모든 데이터 블록을 bfree()로 해제
   *     b) iupdate(): type=0으로 설정하여 inode를 빈 상태로 표시
   *   - 파일이 아직 열려 있다면(ref > 0) 닫힐 때까지 삭제가 지연됨
   *
   * sys_unlink() -> nameiparent() -> dirlookup() -> ip->nlink--
   * -------------------------------------------------------- */
  printf("[단계 10] 파일 삭제: unlink(\"%s\")\n", nested);
  printf("  호출 경로:\n");
  printf("    sys_unlink()\n");
  printf("      -> nameiparent()  : 부모 디렉토리 찾기\n");
  printf("      -> dirlookup()    : 대상 파일 찾기\n");
  printf("      -> writei()       : 디렉토리 엔트리 0으로 초기화\n");
  printf("      -> ip->nlink--    : 링크 수 감소\n");
  printf("      -> iupdate()      : 디스크에 반영\n");
  printf("  (nlink == 0 && ref == 0 이면 iput()에서 itrunc로 블록 해제)\n");
  print_separator();

  if(unlink(nested) < 0){
    printf("  [ERROR] 삭제 실패!\n");
  } else {
    printf("  성공! \"%s\" 삭제됨\n", nested);
  }
  printf("\n");

  /* 남은 테스트 파일/디렉토리 정리 */
  unlink(testdir);
  unlink(testfile);

  /* --------------------------------------------------------
   * 완료
   * -------------------------------------------------------- */
  printf("==================================================\n");
  printf("  모든 파일 시스템 연산 추적 완료!\n");
  printf("==================================================\n");
  printf("\n");
  printf("정리:\n");
  printf("  - 파일 생성: sys_open -> create -> ialloc + dirlink\n");
  printf("  - 파일 쓰기: sys_write -> filewrite -> writei -> log_write\n");
  printf("  - 파일 읽기: sys_read -> fileread -> readi -> bread\n");
  printf("  - 디렉토리 : sys_mkdir -> create(T_DIR) + \".\" + \"..\"\n");
  printf("  - 파일 삭제: sys_unlink -> nlink-- -> (itrunc if nlink==0)\n");
  printf("  - 모든 쓰기는 begin_op/end_op 트랜잭션 안에서 수행됨\n");

  exit(0);
}
