/*
 * lazy_concept.c - Lazy Allocation (Demand Paging) 동작을 관찰하는 프로그램
 *
 * [개요]
 * mmap(MAP_ANONYMOUS)으로 대용량 메모리를 할당한 후,
 * 실제로 페이지에 접근할 때마다 물리 메모리가 할당되는 과정을 관찰합니다.
 *
 * [Lazy Allocation / Demand Paging의 핵심 개념]
 *
 * 1) 지연 할당 (Lazy Allocation):
 *    - 프로세스가 메모리를 요청(sbrk/mmap)하면, 가상 주소 공간만 확장하고
 *      실제 물리 페이지는 즉시 할당하지 않음
 *    - 가상 주소 범위만 기록하고, PTE는 생성하지 않음 (또는 Valid 비트 미설정)
 *    - 프로세스가 해당 주소에 실제로 접근할 때 page fault가 발생하면 그제서야
 *      물리 페이지를 할당하고 PTE를 설정함
 *
 * 2) xv6에서의 Lazy Allocation 구현 위치:
 *    - kernel/sysproc.c의 sys_sbrk(): 크기만 증가시키고 물리 페이지 할당을 생략
 *      (기존: growproc() 호출 -> 변경: p->sz만 증가)
 *    - kernel/trap.c의 usertrap(): scause == 13(load) 또는 15(store) page fault 처리
 *      a) page fault 주소(stval)가 유효한 범위(0 ~ p->sz)인지 확인
 *      b) kalloc()으로 새 물리 페이지 할당
 *      c) memset(mem, 0, PGSIZE)로 보안을 위해 0으로 초기화
 *      d) mappages()로 PTE 생성 (VA -> PA 매핑)
 *    - kernel/vm.c의 uvmunmap(): lazy로 인해 매핑되지 않은 페이지를 unmap할 때
 *      PTE가 없어도 panic하지 않도록 수정 필요
 *
 * 3) Lazy Allocation의 장점:
 *    - 실제로 사용하는 메모리만 물리 페이지를 소비 (메모리 효율)
 *    - sbrk/mmap 호출이 즉시 완료 (지연 없음)
 *    - 프로세스가 큰 메모리를 요청하되 일부만 사용하는 패턴에서 효과적
 *
 * 컴파일: gcc -Wall -o lab3_lazy_concept lab3_lazy_concept.c
 * 실행:   ./lab3_lazy_concept
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>

/* 실험에 사용할 메모리 크기 설정 */
#define PAGE_SIZE     4096                                /* 페이지 크기 (4KB) */
#define TOTAL_PAGES   8192                                /* 할당할 페이지 수 (8192 페이지) */
#define REGION_SIZE   ((size_t)TOTAL_PAGES * PAGE_SIZE)   /* 총 32MB (8192 * 4KB) */
#define TOUCH_STEP    512                                 /* 측정 간격: 512페이지마다 통계 출력 */

/*
 * get_rss_kb - 현재 프로세스의 RSS(Resident Set Size)를 KB 단위로 반환한다.
 *
 * RSS: 실제로 물리 메모리에 존재하는 페이지의 총 크기.
 * lazy allocation에서는 접근한 페이지만 RSS에 포함된다.
 *
 * Linux: /proc/self/statm에서 읽는다.
 * macOS: getrusage()의 ru_maxrss를 사용한다 (최대 RSS).
 */
static long get_rss_kb(void)
{
#ifdef __linux__
    FILE *f = fopen("/proc/self/statm", "r");
    if (f) {
        long total, rss;
        if (fscanf(f, "%ld %ld", &total, &rss) == 2) {
            fclose(f);
            return rss * (sysconf(_SC_PAGESIZE) / 1024);
        }
        fclose(f);
    }
    return -1;
#else
    /* macOS 등: getrusage()를 사용 */
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    /* macOS에서 ru_maxrss는 bytes 단위 */
    return usage.ru_maxrss / 1024;
#endif
}

/*
 * get_minor_faults - 현재 프로세스의 minor page fault 횟수를 반환한다.
 *
 * minor page fault: 디스크 I/O 없이 처리되는 page fault.
 * lazy allocation으로 인한 새 페이지 할당은 minor page fault로 기록된다.
 */
static long get_minor_faults(void)
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_minflt;
}

/*
 * demo_lazy_basic - Lazy Allocation의 기본 동작을 시연한다.
 *
 * 큰 메모리 영역을 mmap으로 할당하고, 점진적으로 접근하면서
 * minor page fault 횟수의 변화를 관찰한다.
 */
static void demo_lazy_basic(void)
{
    char *region;
    int i;

    printf("=== 1. Lazy Allocation 기본 동작 ===\n\n");

    long faults_before = get_minor_faults();
    long rss_before = get_rss_kb();

    /* mmap으로 큰 영역 할당 - 이 시점에서는 물리 메모리를 사용하지 않음
     * 커널은 가상 주소 공간의 범위만 기록 (VMA: Virtual Memory Area)
     * 실제 물리 페이지는 할당하지 않고, PTE도 생성하지 않음
     * 이것이 "lazy allocation"의 핵심: 할당 요청과 실제 할당을 분리
     *
     * xv6에서의 대응: sys_sbrk()에서 p->sz만 증가시키고
     * growproc()/uvmalloc() 호출을 생략하는 것과 동일한 원리 */
    region = mmap(NULL, REGION_SIZE,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS,
                  -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    long faults_after_mmap = get_minor_faults();
    long rss_after_mmap = get_rss_kb();

    printf("[mmap 호출 전]\n");
    printf("  minor page faults: %ld\n", faults_before);
    printf("  RSS: %ld KB\n\n", rss_before);

    printf("[mmap(%zu KB) 호출 후 - 아직 접근하지 않음]\n",
           REGION_SIZE / 1024);
    printf("  minor page faults: %ld (추가: %ld)\n",
           faults_after_mmap, faults_after_mmap - faults_before);
    printf("  RSS: %ld KB\n", rss_after_mmap);
    printf("  -> 가상 주소 공간만 할당됨. 물리 메모리 거의 사용하지 않음!\n\n");

    /* 점진적으로 페이지에 접근하면서 fault 횟수 관찰 */
    printf("[페이지 접근에 따른 minor page fault 변화]\n");
    printf("  %-20s  %-15s  %-15s\n",
           "접근한 페이지 수", "누적 faults", "구간 faults");
    printf("  %-20s  %-15s  %-15s\n",
           "--------------------", "---------------", "---------------");

    long prev_faults = get_minor_faults();

    for (i = 0; i < TOTAL_PAGES; i++) {
        /* 각 페이지의 첫 바이트에 쓰기 -> 이때 page fault 발생
         * page fault 처리 과정 (커널 내부):
         *   1. 하드웨어가 page fault 예외를 발생시킴
         *   2. 커널의 page fault 핸들러가 호출됨
         *   3. fault 주소가 유효한 VMA 범위 내인지 확인
         *   4. 물리 페이지를 할당 (kalloc에 해당)
         *   5. 0으로 초기화 (보안: 이전 프로세스 데이터 유출 방지)
         *   6. PTE를 생성하여 VA -> PA 매핑 설정
         *   7. 프로세스 재개 (쓰기 명령을 재시도) */
        region[(size_t)i * PAGE_SIZE] = (char)(i & 0xFF);

        if ((i + 1) % TOUCH_STEP == 0) {
            long cur_faults = get_minor_faults();
            printf("  %-20d  %-15ld  %-15ld\n",
                   i + 1,
                   cur_faults - faults_after_mmap,
                   cur_faults - prev_faults);
            prev_faults = cur_faults;
        }
    }

    long rss_after_touch = get_rss_kb();
    long faults_after_touch = get_minor_faults();

    printf("\n[모든 페이지 접근 완료]\n");
    printf("  총 minor page faults: %ld\n",
           faults_after_touch - faults_after_mmap);
    printf("  RSS: %ld KB\n", rss_after_touch);
    long sys_pgsize = sysconf(_SC_PAGESIZE);
    long expected = (long)TOTAL_PAGES * PAGE_SIZE / sys_pgsize;
    printf("  -> 시스템 페이지 크기(%ld bytes) 기준 약 %ld번의 page fault 예상\n",
           sys_pgsize, expected);
    printf("     (프로그램 PAGE_SIZE=%d, 실제 시스템 페이지=%ld bytes)\n\n",
           PAGE_SIZE, sys_pgsize);

    munmap(region, REGION_SIZE);
}

/*
 * demo_lazy_sparse - 희소(sparse) 접근 패턴에서의 이점을 시연한다.
 *
 * 큰 메모리를 할당하고 일부 페이지만 접근하여,
 * 실제로 접근한 페이지만 물리 메모리를 소비함을 관찰한다.
 *
 * 이것은 lazy allocation의 가장 큰 장점:
 * - 프로그램이 32MB를 요청했지만 실제로 10%만 사용하면
 *   물리 메모리는 약 3.2MB만 소비됨
 * - eager allocation이었다면 32MB 전체를 즉시 할당해야 했음
 * - 해시 테이블, 희소 배열 등에서 매우 효과적
 */
static void demo_lazy_sparse(void)
{
    char *region;
    int i;
    int accessed = 0;

    printf("=== 2. 희소 접근 패턴에서의 Lazy Allocation 이점 ===\n\n");

    region = mmap(NULL, REGION_SIZE,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS,
                  -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    long faults_before = get_minor_faults();

    /* 전체 페이지 중 10%만 접근 (매 10번째 페이지) */
    for (i = 0; i < TOTAL_PAGES; i += 10) {
        region[(size_t)i * PAGE_SIZE] = 'S';
        accessed++;
    }

    long faults_after = get_minor_faults();
    long rss = get_rss_kb();

    printf("  할당된 가상 메모리: %zu KB (%d 페이지)\n",
           REGION_SIZE / 1024, TOTAL_PAGES);
    printf("  실제 접근한 페이지: %d 페이지 (전체의 약 %d%%)\n",
           accessed, (accessed * 100) / TOTAL_PAGES);
    printf("  minor page faults: %ld\n", faults_after - faults_before);
    printf("  RSS: %ld KB\n", rss);
    printf("  -> 접근하지 않은 %d%%의 페이지는 물리 메모리를 사용하지 않음!\n",
           100 - (accessed * 100) / TOTAL_PAGES);
    printf("  -> Eager allocation이었다면 %zu KB를 모두 할당했을 것\n\n",
           REGION_SIZE / 1024);

    munmap(region, REGION_SIZE);
}

/*
 * demo_lazy_timing - 할당 시간 vs 접근 시간을 비교한다.
 *
 * 세 가지 시간을 비교하여 lazy allocation의 비용 구조를 이해:
 *   1. mmap 호출 시간: 가상 주소 공간 설정만 하므로 매우 빠름 (us 단위)
 *   2. 첫 접근 시간: 각 페이지마다 page fault -> 물리 할당 -> PTE 생성
 *      이 과정에서 page fault 처리 오버헤드가 발생
 *   3. 재접근 시간: 이미 PTE가 설정되어 있으므로 page fault 없이 바로 접근
 *      하드웨어 TLB에 캐시되면 더욱 빠름
 */
static void demo_lazy_timing(void)
{
    char *region;
    int i;
    struct timespec t1, t2;

    printf("=== 3. 할당 시간 vs 접근 시간 비교 ===\n\n");

    /* mmap 시간 측정 */
    clock_gettime(CLOCK_MONOTONIC, &t1);
    region = mmap(NULL, REGION_SIZE,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS,
                  -1, 0);
    clock_gettime(CLOCK_MONOTONIC, &t2);

    if (region == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    long long mmap_us = (t2.tv_sec - t1.tv_sec) * 1000000LL +
                        (t2.tv_nsec - t1.tv_nsec) / 1000;
    printf("  mmap(%zu KB) 소요 시간: %lld us\n", REGION_SIZE / 1024, mmap_us);
    printf("  -> 가상 주소 공간만 설정하므로 매우 빠름\n\n");

    /* 모든 페이지 접근 시간 측정 */
    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (i = 0; i < TOTAL_PAGES; i++) {
        region[(size_t)i * PAGE_SIZE] = (char)i;
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);

    long long touch_us = (t2.tv_sec - t1.tv_sec) * 1000000LL +
                         (t2.tv_nsec - t1.tv_nsec) / 1000;
    printf("  모든 페이지 접근 소요 시간: %lld us\n", touch_us);
    printf("  -> 각 페이지 접근 시 page fault 발생 -> 물리 페이지 할당\n\n");

    /* 이미 할당된 페이지 재접근 시간 측정 */
    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (i = 0; i < TOTAL_PAGES; i++) {
        region[(size_t)i * PAGE_SIZE] = (char)(i + 1);
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);

    long long retouch_us = (t2.tv_sec - t1.tv_sec) * 1000000LL +
                           (t2.tv_nsec - t1.tv_nsec) / 1000;
    printf("  이미 할당된 페이지 재접근 소요 시간: %lld us\n", retouch_us);
    printf("  -> page fault 없이 바로 접근 가능하므로 빠름\n\n");

    if (touch_us > 0) {
        printf("  비율: 첫 접근은 재접근보다 약 %.1fx 느림\n",
               (double)touch_us / (retouch_us > 0 ? retouch_us : 1));
        printf("  -> page fault 처리 비용이 포함되기 때문\n\n");
    }

    munmap(region, REGION_SIZE);
}

/*
 * demo_lazy_zero_fill - 새로 할당된 페이지가 0으로 초기화됨을 확인한다.
 *
 * 보안상의 이유로 OS는 새로 할당되는 물리 페이지를 0으로 초기화해야 한다.
 * 그렇지 않으면 이전 프로세스가 사용하던 메모리 내용(비밀번호, 개인정보 등)이
 * 새 프로세스에 노출될 수 있는 보안 취약점이 발생한다.
 *
 * xv6 구현: page fault 핸들러에서 kalloc() 후 memset(mem, 0, PGSIZE) 호출
 * Linux 구현: 시스템 전역의 "zero page"를 사용하거나, 할당 시 0으로 초기화
 */
static void demo_lazy_zero_fill(void)
{
    char *region;
    int i;
    int non_zero = 0;

    printf("=== 4. 새 페이지의 제로 초기화 확인 ===\n\n");

    region = mmap(NULL, REGION_SIZE,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS,
                  -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    /* 접근하지 않은 상태에서 읽기 -> 0이어야 함 */
    for (i = 0; i < TOTAL_PAGES; i++) {
        int j;
        for (j = 0; j < PAGE_SIZE; j++) {
            if (region[(size_t)i * PAGE_SIZE + j] != 0) {
                non_zero++;
                break;
            }
        }
    }

    printf("  %d 페이지 중 0이 아닌 데이터가 있는 페이지: %d\n",
           TOTAL_PAGES, non_zero);
    if (non_zero == 0) {
        printf("  -> 모든 새 페이지가 0으로 초기화됨 (보안 보장)\n");
    } else {
        printf("  -> 주의: 일부 페이지가 0으로 초기화되지 않음\n");
    }
    printf("  -> xv6의 vmfault()에서도 memset(mem, 0, PGSIZE)로 초기화함\n\n");

    munmap(region, REGION_SIZE);
}

int main(void)
{
    printf("=============================================\n");
    printf("  Lazy Allocation 동작 시연 프로그램\n");
    printf("=============================================\n\n");
    printf("시스템 페이지 크기: %d bytes\n", (int)sysconf(_SC_PAGESIZE));
    printf("할당 영역 크기: %d pages (%zu KB)\n\n",
           TOTAL_PAGES, REGION_SIZE / 1024);
#ifdef __linux__
    printf("플랫폼: Linux (/proc/self/statm으로 RSS 측정)\n\n");
#elif defined(__APPLE__)
    printf("플랫폼: macOS (getrusage()로 메모리 사용량 측정)\n");
    printf("참고: macOS의 ru_maxrss는 최대 RSS이므로 감소를 관찰할 수 없음.\n");
    printf("      minor page fault 횟수로 lazy allocation 동작을 확인.\n\n");
#else
    printf("플랫폼: 기타 (getrusage()로 측정)\n\n");
#endif

    demo_lazy_basic();
    demo_lazy_sparse();
    demo_lazy_timing();
    demo_lazy_zero_fill();

    printf("=============================================\n");
    printf("  요약\n");
    printf("=============================================\n");
    printf("1. mmap()은 가상 주소 공간만 할당하고, 물리 메모리는 접근 시 할당된다\n");
    printf("2. 접근하지 않은 페이지는 물리 메모리를 소비하지 않는다\n");
    printf("3. 첫 접근 시 page fault가 발생하여 물리 페이지가 할당된다\n");
    printf("4. 새로 할당된 페이지는 보안을 위해 0으로 초기화된다\n");
    printf("5. xv6의 vmfault()도 같은 원리: kalloc() + memset(0) + mappages()\n");

    return 0;
}
