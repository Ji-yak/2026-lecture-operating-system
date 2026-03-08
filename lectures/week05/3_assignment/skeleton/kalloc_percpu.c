// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.
//
// [과제] Per-CPU free list를 사용하도록 수정
// 원본: xv6-riscv/kernel/kalloc.c

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

// TODO: kmem을 per-CPU 배열로 변경하세요.
// 힌트: NCPU는 param.h에 8로 정의되어 있습니다.
// 각 CPU가 자신만의 lock과 freelist를 가져야 합니다.
//
// 기존 코드:
//   struct {
//     struct spinlock lock;
//     struct run *freelist;
//   } kmem;
//
// 변경 후 (아래를 완성하세요):
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  // TODO: 각 CPU의 lock을 초기화하세요.
  // 힌트: 모든 NCPU개의 lock을 초기화해야 합니다.
  // initlock(&kmem[i].lock, "kmem");
  //
  // 초기에 모든 free page는 CPU 0의 freelist에 넣습니다.
  // (나중에 다른 CPU가 steal하게 됩니다)

  // --- 여기에 코드를 작성하세요 ---

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // TODO: 현재 CPU의 freelist에 페이지를 반환하세요.
  // 힌트:
  //   1. push_off()로 인터럽트를 비활성화해야 cpuid()를 안전하게 호출할 수 있습니다.
  //   2. cpuid()로 현재 CPU 번호를 얻습니다.
  //   3. 해당 CPU의 kmem[id].lock을 acquire합니다.
  //   4. freelist에 페이지를 추가합니다.
  //   5. release 후 pop_off()를 호출합니다.
  //
  // 주의: push_off/pop_off와 acquire/release의 순서에 주의하세요.
  //       acquire 내부에서 이미 push_off를 호출하므로,
  //       cpuid()를 안전하게 사용하려면 그 전에 push_off가 필요합니다.

  // --- 여기에 코드를 작성하세요 ---
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  // TODO: 먼저 현재 CPU의 freelist에서 할당을 시도하세요.
  // 자신의 freelist가 비어 있으면 다른 CPU에서 steal합니다.
  //
  // 구현 단계:
  //   1. push_off()로 인터럽트 비활성화
  //   2. cpuid()로 현재 CPU 번호 획득
  //   3. 현재 CPU의 freelist에서 할당 시도
  //   4. 실패하면 다른 CPU의 freelist에서 steal 시도
  //      - 다른 CPU의 lock을 acquire하여 하나의 페이지를 가져옴
  //   5. pop_off()로 인터럽트 복원
  //
  // 주의사항:
  //   - steal 시 deadlock을 방지하려면 한 번에 하나의 lock만 보유하세요.
  //   - 모든 CPU를 순회했는데도 없으면 0 반환 (out of memory)

  // --- 여기에 코드를 작성하세요 ---

  r = 0; // placeholder - 이 줄을 수정하세요

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
