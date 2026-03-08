/*
 * cow_concept.c - Copy-On-Write (COW) 동작을 관찰하는 프로그램
 *
 * [개요]
 * COW fork의 핵심 개념을 Linux/macOS에서 시연합니다.
 * - 부모가 mmap으로 메모리 영역을 생성하고 데이터를 채운다
 * - fork() 후 부모와 자식이 같은 물리 페이지를 공유한다
 * - 한쪽이 쓰기를 하면 COW로 인해 페이지가 복사된다
 *
 * [COW Fork의 핵심 개념]
 *
 * 1) 참조 카운팅 (Reference Counting):
 *    - 각 물리 페이지에 대해 참조 카운트를 관리
 *    - fork() 시 자식은 부모의 PTE를 복사하고, 물리 페이지의 참조 카운트를 증가
 *    - 페이지 해제 시 참조 카운트를 감소시키고, 0이 되면 실제로 물리 메모리를 해제
 *    - xv6 COW 구현에서는 전역 배열로 관리: refcnt[pa / PGSIZE]
 *
 * 2) 페이지 폴트 핸들링 (Page Fault Handling):
 *    - fork() 시 부모와 자식 모두의 PTE에서 PTE_W(쓰기 권한)를 제거
 *    - 대신 PTE_COW 플래그(예: RSW 비트)를 설정하여 COW 페이지임을 표시
 *    - 어느 쪽이든 해당 페이지에 쓰기를 시도하면 page fault 발생
 *    - page fault 핸들러에서:
 *      a) 참조 카운트가 1이면: 이 프로세스만 사용 중이므로 PTE_W 복원만 하면 됨
 *      b) 참조 카운트가 2 이상이면: 새 물리 페이지를 할당하고, 원본 내용을 복사한 후,
 *         PTE를 새 페이지를 가리키도록 갱신 (PTE_W 설정, PTE_COW 제거)
 *      c) 원본 페이지의 참조 카운트를 감소
 *
 * 3) xv6 COW 구현 위치:
 *    - kernel/vm.c의 uvmcopy(): fork 시 PTE 복사 + PTE_W 제거 + PTE_COW 설정
 *    - kernel/trap.c의 usertrap(): scause == 15 (store page fault) 처리
 *    - kernel/vm.c 또는 kernel/kalloc.c: 참조 카운트 관리 함수들
 *
 * 컴파일: gcc -Wall -o lab2_cow_concept lab2_cow_concept.c
 * 실행:   ./lab2_cow_concept
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>

/* 실험에 사용할 메모리 크기 설정 */
#define NUM_PAGES    1024       /* 할당할 페이지 수 (1024 페이지) */
#define PAGE_SIZE    4096       /* 페이지 크기 (4KB, 일반적인 시스템 기준) */
#define REGION_SIZE  (NUM_PAGES * PAGE_SIZE)  /* 총 4MB (1024 * 4KB) */

/*
 * get_minor_faults - 현재 프로세스의 minor page fault 횟수를 반환한다.
 *
 * minor page fault: 디스크 I/O 없이 처리되는 page fault.
 * COW로 인한 페이지 복사는 minor page fault로 기록된다.
 */
static long get_minor_faults(void)
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_minflt;
}

/*
 * get_time_us - 현재 시간을 마이크로초 단위로 반환한다.
 */
static long long get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

/*
 * demo_cow_basic - COW의 기본 동작을 시연한다.
 *
 * fork() 후 자식이 읽기만 할 때와 쓰기를 할 때의
 * minor page fault 횟수 차이를 관찰한다.
 */
static void demo_cow_basic(void)
{
    char *region;
    int i;

    printf("=== 1. COW Fork 기본 동작 ===\n\n");

    /* 메모리 영역 할당
     * mmap 플래그 설명:
     *   PROT_READ | PROT_WRITE: 읽기/쓰기 권한 설정
     *   MAP_PRIVATE: 프로세스 전용 매핑 (fork 시 COW 적용 대상)
     *   MAP_ANONYMOUS: 파일이 아닌 익명 메모리 (0으로 초기화됨)
     * MAP_PRIVATE가 핵심: 이 플래그로 인해 fork 후 쓰기 시 COW 복사가 발생 */
    region = mmap(NULL, REGION_SIZE,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS,
                  -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    /* 모든 페이지에 데이터 기록 (물리 페이지 확보) */
    for (i = 0; i < NUM_PAGES; i++) {
        region[i * PAGE_SIZE] = (char)('A' + (i % 26));
    }
    printf("[부모] %d 페이지에 데이터 기록 완료 (%d KB)\n",
           NUM_PAGES, REGION_SIZE / 1024);

    /* fork 전 minor fault 횟수 기록
     * minor page fault: 디스크 I/O 없이 커널이 처리하는 page fault
     * COW 복사는 minor fault로 분류됨 (이미 메모리에 있는 페이지를 복사하므로) */
    long faults_before_fork = get_minor_faults();
    printf("[부모] fork 전 minor page faults: %ld\n\n", faults_before_fork);

    /* fork() 호출: 이 시점에서 커널은 부모의 모든 PTE를 자식에게 복사하되,
     * 물리 페이지는 복사하지 않음 (COW). 부모와 자식 모두 쓰기 권한을 제거하고,
     * COW 표시를 설정. 각 물리 페이지의 참조 카운트가 2로 증가 */
    pid_t pid = fork();

    if (pid == 0) {
        /* ---- 자식 프로세스 ---- */
        long faults_after_fork = get_minor_faults();
        printf("[자식] fork 직후 minor page faults: %ld\n", faults_after_fork);

        /* 읽기만 수행: COW 복사가 발생하지 않아야 함
         * 읽기는 쓰기 권한이 필요 없으므로 page fault가 발생하지 않음
         * 부모와 자식이 같은 물리 페이지를 공유한 상태로 읽기 가능 */
        long faults_before_read = get_minor_faults();
        volatile char sum = 0;
        for (i = 0; i < NUM_PAGES; i++) {
            sum += region[i * PAGE_SIZE];
        }
        (void)sum;
        long faults_after_read = get_minor_faults();
        printf("[자식] 읽기 후 minor page faults: %ld (읽기로 인한 추가 faults: %ld)\n",
               faults_after_read, faults_after_read - faults_before_read);

        /* 쓰기 수행: COW 복사가 발생해야 함
         * 각 페이지에 처음 쓰기 시:
         *   1. 쓰기 권한이 없으므로 store page fault 발생
         *   2. 커널의 page fault 핸들러가 COW 페이지임을 감지
         *   3. 새 물리 페이지를 할당하고 원본 내용을 복사
         *   4. PTE를 새 페이지를 가리키도록 갱신 (쓰기 권한 부여)
         *   5. 원본 페이지의 참조 카운트를 감소
         * 이 과정이 minor page fault로 기록됨 */
        long faults_before_write = get_minor_faults();
        for (i = 0; i < NUM_PAGES; i++) {
            region[i * PAGE_SIZE] = 'Z';  /* 각 페이지에 쓰기 -> COW 발생 */
        }
        long faults_after_write = get_minor_faults();
        printf("[자식] 쓰기 후 minor page faults: %ld (쓰기로 인한 추가 faults: %ld)\n",
               faults_after_write, faults_after_write - faults_before_write);
        long sys_pgsize = sysconf(_SC_PAGESIZE);
        long expected = (long)NUM_PAGES * PAGE_SIZE / sys_pgsize;
        printf("[자식] -> 시스템 페이지 크기 기준 약 %ld번의 COW fault 예상 (실제: %ld)\n",
               expected, faults_after_write - faults_before_write);
        printf("         (시스템 페이지 크기 = %ld bytes, 프로그램 PAGE_SIZE = %d bytes)\n\n",
               sys_pgsize, PAGE_SIZE);

        munmap(region, REGION_SIZE);
        exit(0);
    } else if (pid > 0) {
        /* ---- 부모 프로세스 ---- */
        wait(NULL);

        /* 부모의 데이터가 변경되지 않았는지 확인
         * COW의 핵심: 자식이 쓰기를 해도 부모의 데이터에는 영향 없음
         * 자식의 쓰기 시 새로운 물리 페이지가 할당되었으므로
         * 부모는 원본 물리 페이지를 그대로 유지 */
        int ok = 1;
        for (i = 0; i < NUM_PAGES; i++) {
            if (region[i * PAGE_SIZE] != (char)('A' + (i % 26))) {
                ok = 0;
                break;
            }
        }
        printf("[부모] 자식의 쓰기 후 부모 데이터 검증: %s\n",
               ok ? "변경 없음 (PASS)" : "변경됨! (FAIL)");
        printf("       -> COW 덕분에 자식의 쓰기가 부모에 영향을 주지 않음\n\n");

        munmap(region, REGION_SIZE);
    } else {
        perror("fork");
        exit(1);
    }
}

/*
 * demo_cow_performance - COW fork의 성능 이점을 시연한다.
 *
 * fork + exec 패턴에서 COW의 이점:
 * - 전통적인 eager copy fork: 부모의 모든 페이지를 물리적으로 복사
 *   -> 부모가 4MB를 사용하면 fork에 4MB 복사 비용 발생
 * - COW fork: PTE만 복사 (물리 페이지 복사 없음)
 *   -> fork 후 exec를 호출하면 자식의 주소 공간이 새로 교체되므로
 *      복사한 페이지를 즉시 버리게 됨 -> eager copy는 낭비
 *
 * 이 데모에서는 fork 자체의 속도를 측정하여 COW의 효율을 확인합니다.
 */
static void demo_cow_performance(void)
{
    char *region;
    int i;

    printf("=== 2. COW Fork 성능 비교 ===\n\n");

    /* 큰 메모리 영역 할당 */
    region = mmap(NULL, REGION_SIZE,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS,
                  -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    /* 모든 페이지에 데이터 기록 */
    for (i = 0; i < NUM_PAGES; i++) {
        memset(region + i * PAGE_SIZE, 'X', PAGE_SIZE);
    }

    /* fork 시간 측정 */
    long long t1 = get_time_us();
    pid_t pid = fork();
    long long t2 = get_time_us();

    if (pid == 0) {
        /* 자식: fork만 하고 즉시 종료 (exec 시뮬레이션) */
        printf("[자식] fork 소요 시간: %lld us\n", t2 - t1);
        printf("[자식] -> COW 덕분에 %d KB를 복사하지 않고 즉시 fork 완료\n",
               REGION_SIZE / 1024);
        printf("[자식] -> 만약 eager copy였다면 %d 페이지를 모두 복사해야 했음\n\n",
               NUM_PAGES);
        munmap(region, REGION_SIZE);
        exit(0);
    } else if (pid > 0) {
        wait(NULL);
        munmap(region, REGION_SIZE);
    } else {
        perror("fork");
        exit(1);
    }
}

/*
 * demo_cow_isolation - COW에서 프로세스 격리가 보장됨을 확인한다.
 *
 * COW는 성능 최적화 기법이지만, 프로세스 간 격리(isolation)는 완벽히 유지됩니다.
 * - fork 직후: 부모와 자식이 같은 물리 페이지를 공유하여 같은 값을 읽음
 * - 쓰기 후: COW 복사로 인해 각자 독립적인 물리 페이지를 갖게 됨
 * - 결과: 한쪽의 수정이 다른 쪽에 전혀 영향을 주지 않음
 *
 * 이것은 MAP_SHARED와 대조됨: MAP_SHARED는 쓰기도 공유하므로
 * 한쪽의 수정이 다른 쪽에서도 보임 (프로세스 간 통신에 사용).
 */
static void demo_cow_isolation(void)
{
    int *shared_val;

    printf("=== 3. COW 프로세스 격리 확인 ===\n\n");

    /* 정수값을 저장할 메모리 할당 */
    shared_val = mmap(NULL, PAGE_SIZE,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1, 0);
    if (shared_val == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    *shared_val = 42;
    printf("[부모] fork 전 값: %d\n", *shared_val);

    pid_t pid = fork();

    if (pid == 0) {
        /* 자식 프로세스 */
        printf("[자식] fork 직후 값: %d (부모와 같은 물리 페이지를 공유 중)\n",
               *shared_val);

        /* MAP_PRIVATE이므로 쓰기 시 COW 발생 */
        *shared_val = 100;
        printf("[자식] 값 변경 후: %d (COW로 인해 별도의 물리 페이지에 기록됨)\n",
               *shared_val);

        munmap(shared_val, PAGE_SIZE);
        exit(0);
    } else if (pid > 0) {
        wait(NULL);
        printf("[부모] 자식 종료 후 부모의 값: %d (변경되지 않음!)\n", *shared_val);
        printf("       -> COW가 프로세스 간 격리를 보장함\n\n");

        munmap(shared_val, PAGE_SIZE);
    } else {
        perror("fork");
        exit(1);
    }
}

int main(void)
{
    printf("=========================================\n");
    printf("  COW (Copy-On-Write) 동작 시연 프로그램\n");
    printf("=========================================\n\n");
    printf("시스템 페이지 크기: %d bytes\n", (int)sysconf(_SC_PAGESIZE));
    printf("할당 영역 크기: %d pages (%d KB)\n\n", NUM_PAGES, REGION_SIZE / 1024);

    demo_cow_basic();
    demo_cow_performance();
    demo_cow_isolation();

    printf("=========================================\n");
    printf("  요약\n");
    printf("=========================================\n");
    printf("1. fork() 후 읽기만 하면 COW 복사가 발생하지 않는다\n");
    printf("2. 쓰기를 하면 minor page fault가 발생하고 페이지가 복사된다\n");
    printf("3. COW 덕분에 부모와 자식의 데이터는 완전히 격리된다\n");
    printf("4. fork + exec 패턴에서 불필요한 메모리 복사를 피할 수 있다\n");

    return 0;
}
