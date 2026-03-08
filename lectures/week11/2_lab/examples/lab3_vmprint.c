/*
 * vmprint() - 프로세스의 페이지 테이블 내용을 출력하는 함수
 *
 * [개요]
 * xv6의 Sv39 3단계 페이지 테이블을 재귀적으로 순회하면서
 * 유효한(PTE_V가 설정된) 모든 PTE를 출력합니다.
 * 페이지 테이블의 구조와 각 매핑의 권한(플래그)을 한눈에 파악할 수 있습니다.
 *
 * [Sv39 3단계 페이지 테이블 구조]
 *
 *   satp 레지스터 ----> L2 테이블 (512개 PTE, 각 8바이트 = 4KB)
 *                         |
 *                    VPN[2] 인덱스로 PTE 선택
 *                         |
 *                         v
 *                       L1 테이블 (512개 PTE)
 *                         |
 *                    VPN[1] 인덱스로 PTE 선택
 *                         |
 *                         v
 *                       L0 테이블 (512개 PTE)
 *                         |
 *                    VPN[0] 인덱스로 PTE 선택
 *                         |
 *                         v
 *                    물리 페이지 (4KB)
 *
 * [PTE 플래그 비트 (kernel/riscv.h)]
 *   PTE_V (bit 0): Valid - 이 PTE가 유효한지 여부
 *   PTE_R (bit 1): Read  - 읽기 허용
 *   PTE_W (bit 2): Write - 쓰기 허용
 *   PTE_X (bit 3): eXecute - 실행 허용
 *   PTE_U (bit 4): User - 사용자 모드에서 접근 허용
 *
 *   리프 PTE: R, W, X 중 하나 이상이 설정됨 -> 실제 물리 페이지를 가리킴
 *   비리프 PTE: R=0, W=0, X=0이고 V=1 -> 다음 레벨 페이지 테이블을 가리킴
 *
 * [xv6 주요 매크로 (kernel/riscv.h)]
 *   PTE2PA(pte): PTE에서 물리 주소(PA)를 추출
 *     -> ((pte) >> 10) << 12   (PPN 필드를 페이지 정렬된 주소로 변환)
 *   PA2PTE(pa):  물리 주소를 PTE의 PPN 필드로 변환
 *     -> ((pa) >> 12) << 10
 *
 * 이 코드를 kernel/vm.c 파일의 맨 끝에 추가하세요.
 *
 * === 추가 방법 ===
 *
 * 1단계: 이 파일의 두 함수를 kernel/vm.c 맨 끝에 복사하세요.
 *
 * 2단계: kernel/defs.h 파일에서 "// vm.c" 섹션을 찾아 아래 선언을 추가하세요:
 *
 *     void            vmprint(pagetable_t);
 *
 * 3단계: kernel/exec.c의 kexec() 함수에서 "return argc;" 직전에 아래 코드를 추가하세요:
 *
 *     if(p->pid == 1){
 *       printf("== pid 1 (init) page table ==\n");
 *       vmprint(p->pagetable);
 *     }
 *
 * 4단계: 빌드 및 실행
 *     $ make clean
 *     $ make qemu
 *
 * === 예상 출력 ===
 *
 * == pid 1 (init) page table ==
 * page table 0x0000000087f6b000
 *  ..0: pte 0x0000000021fd9c01 pa 0x0000000087f67000 [--]
 *  .. ..0: pte 0x0000000021fd9801 pa 0x0000000087f66000 [--]
 *  .. .. ..0: pte 0x0000000021fda41f pa 0x0000000087f69000 [RWXU]
 *  .. .. ..1: pte 0x0000000021fd9017 pa 0x0000000087f64000 [RWU]
 *  .. .. ..2: pte 0x0000000021fd8c07 pa 0x0000000087f63000 [RW]
 *  .. .. ..3: pte 0x0000000021fd8817 pa 0x0000000087f62000 [RWU]
 *  ..255: pte 0x0000000021fda801 pa 0x0000000087f6a000 [--]
 *  .. ..511: pte 0x0000000020001c0b pa 0x0000000080007000 [--]
 *  .. .. ..510: pte 0x0000000021fdcc17 pa 0x0000000087f73000 [RWU]
 *  .. .. ..511: pte 0x0000000020001c4b pa 0x0000000080007000 [RX]
 *
 * === 출력 해석 ===
 *
 * 1) L2 인덱스 0 -> 가상 주소 0x0000000000 ~ 0x003FFFFFFF 범위 (유저 코드/데이터 영역)
 *    - L1 인덱스 0 -> L0 테이블로 연결
 *      - L0 인덱스 0: 유저 코드 페이지 (RWXU) -- text 세그먼트 (읽기+쓰기+실행+유저)
 *      - L0 인덱스 1: 유저 데이터 페이지 (RWU) -- data 세그먼트 (읽기+쓰기+유저)
 *      - L0 인덱스 2: 가드 페이지 (RW, U 없음) -- exec에서 uvmclear()로 PTE_U 제거
 *                     유저가 접근하면 page fault 발생 -> 스택 오버플로 감지용
 *      - L0 인덱스 3: 유저 스택 페이지 (RWU) -- stack 세그먼트
 *
 * 2) L2 인덱스 255 -> 가상 주소의 최상위 영역 (TRAMPOLINE/TRAPFRAME)
 *    - L1 인덱스 511 -> L0 테이블로 연결
 *      - L0 인덱스 510: TRAPFRAME 페이지 (RWU) -- 트랩 시 레지스터 저장/복원
 *      - L0 인덱스 511: TRAMPOLINE 페이지 (RX, U 없음) -- uservec/userret 코드
 *                       커널과 유저 페이지 테이블 모두에 같은 VA로 매핑되어
 *                       페이지 테이블 전환 시에도 코드가 중단 없이 실행됨
 *
 * 참고: 실제 출력은 실행 환경에 따라 물리 주소가 다를 수 있습니다.
 *       PTE 값과 플래그 구조는 동일합니다.
 */

// ====================================================================
// 아래 코드를 kernel/vm.c 맨 끝에 추가하세요
// ====================================================================

/*
 * vmprint_recursive - 페이지 테이블의 유효한 엔트리를 재귀적으로 출력
 *
 * 인자:
 *   pagetable: 현재 레벨의 페이지 테이블 포인터 (pagetable_t = uint64*)
 *              xv6에서 각 페이지 테이블은 512개의 PTE를 포함하는 4KB 페이지
 *   level: 현재 레벨 (2 = L2 루트, 1 = L1 중간, 0 = L0 최하위)
 *
 * 동작:
 *   1. 512개의 PTE를 순회하며 유효한(PTE_V) 엔트리만 출력
 *   2. 비리프 PTE (R=0, W=0, X=0인 유효 PTE)를 만나면
 *      해당 PTE가 가리키는 다음 레벨 테이블로 재귀 호출
 *   3. 리프 PTE (R, W, X 중 하나 이상 설정)는 실제 매핑이므로 플래그 출력
 *
 * 이 함수는 xv6의 freewalk() 함수와 유사한 재귀 구조를 가집니다.
 * freewalk()은 페이지 테이블을 해제하고, vmprint_recursive()는 출력합니다.
 */
void
vmprint_recursive(pagetable_t pagetable, int level)
{
  // 페이지 테이블은 512개의 PTE로 구성 (인덱스 0~511)
  // 각 PTE는 8바이트(uint64) -> 512 * 8 = 4096 = 한 페이지
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];   // i번째 PTE 읽기 (pte_t = uint64)
    if(!(pte & PTE_V))
      continue;           // PTE_V(비트 0)가 0이면 유효하지 않은 엔트리 -> 건너뜀

    // 들여쓰기 출력: 레벨이 낮을수록 깊은 들여쓰기
    // level 2 (L2 루트):  " .."
    // level 1 (L1 중간):  " .. .."
    // level 0 (L0 최하위): " .. .. .."
    // 이를 통해 3단계 계층 구조를 시각적으로 표현
    for(int j = 2; j >= level; j--){
      printf(" ..");
    }

    // PTE2PA 매크로: PTE에서 물리 주소 추출
    // PTE의 비트 53~10이 PPN(Physical Page Number)
    // PTE2PA(pte) = ((pte >> 10) << 12) -> 페이지 정렬된 물리 주소
    uint64 pa = PTE2PA(pte);
    printf("%d: pte 0x%016lx pa 0x%016lx", i, (uint64)pte, pa);

    // PTE 플래그 비트를 사람이 읽기 쉬운 형태로 출력
    // R(Read), W(Write), X(eXecute), U(User) 플래그 표시
    // 비리프 PTE는 R=W=X=0이므로 "--"로 표시
    printf(" [");
    if(pte & PTE_R) printf("R");   // 비트 1: 읽기 허용
    if(pte & PTE_W) printf("W");   // 비트 2: 쓰기 허용
    if(pte & PTE_X) printf("X");   // 비트 3: 실행 허용
    if(pte & PTE_U) printf("U");   // 비트 4: 유저 모드 접근 허용
    if((pte & (PTE_R|PTE_W|PTE_X)) == 0) printf("--");  // 비리프 PTE
    printf("]");
    printf("\n");

    // 비리프 PTE이면 다음 레벨 페이지 테이블로 재귀
    // Sv39 스펙: R=0, W=0, X=0이고 V=1인 PTE는 다음 단계 테이블을 가리킴
    // 리프 PTE: R, W, X 중 하나 이상 설정 -> 실제 물리 페이지 매핑
    // 비리프 PTE: R=0, W=0, X=0 -> PPN이 다음 레벨 테이블의 물리 주소
    if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // pa를 pagetable_t로 캐스팅하여 다음 레벨 테이블 포인터로 사용
      // xv6 커널은 직접 매핑(VA==PA)이므로 PA를 그대로 포인터로 사용 가능
      vmprint_recursive((pagetable_t)pa, level - 1);
    }
  }
}

/*
 * vmprint - 프로세스의 전체 페이지 테이블을 출력
 *
 * 인자:
 *   pagetable: L2 (최상위, 루트) 페이지 테이블의 포인터
 *              xv6에서 프로세스의 페이지 테이블은 proc->pagetable에 저장됨
 *              이 값은 satp 레지스터에 로드되어 하드웨어 주소 변환에 사용됨
 *
 * 사용 예:
 *   vmprint(p->pagetable);     // 프로세스 p의 유저 페이지 테이블 출력
 *   vmprint(kernel_pagetable); // 커널 페이지 테이블 출력
 *
 * 출력 형식: 먼저 루트 테이블의 물리 주소를 출력한 후,
 *           vmprint_recursive()를 호출하여 모든 유효한 PTE를 재귀적으로 출력
 */
void
vmprint(pagetable_t pagetable)
{
  printf("page table 0x%016lx\n", (uint64)pagetable);
  vmprint_recursive(pagetable, 2);
}
