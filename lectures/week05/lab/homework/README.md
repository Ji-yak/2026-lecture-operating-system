# Week 5 Assignment: Advanced Synchronization

> Textbook reference: xv6 textbook Ch 6 (Locking), Ch 9 (Concurrency Revisited)
>
> Deadline: Before the next class

---

## Assignment 1: xv6 kalloc Per-CPU Free List Improvement (Required)

> Based on textbook Ch 6, Exercise 3

### Background

The current physical memory allocator in xv6 (`kernel/kalloc.c`) is implemented as follows:

```c
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;
```

- A **single free list** + **single lock** for the entire system
- All CPUs compete for the same lock when calling `kalloc()`/`kfree()`
- As the number of CPUs increases, **lock contention** intensifies -> performance degradation

### Goal

Change `kmem` to a **per-CPU free list array** to improve parallel memory allocation performance.

```
Before:                          After:
┌──────────────────┐            ┌──────────────────┐
│  kmem             │            │  kmem[0]          │  Dedicated to CPU 0
│  ├─ lock          │            │  ├─ lock          │
│  └─ freelist ─→...│            │  └─ freelist ─→...│
└──────────────────┘            ├──────────────────┤
    Shared by all CPUs           │  kmem[1]          │  Dedicated to CPU 1
                                │  ├─ lock          │
                                │  └─ freelist ─→...│
                                ├──────────────────┤
                                │  ...              │
                                ├──────────────────┤
                                │  kmem[NCPU-1]     │  Dedicated to CPU 7
                                │  ├─ lock          │
                                │  └─ freelist ─→...│
                                └──────────────────┘
```

### File to Modify

You only need to modify a single file: `kernel/kalloc.c`.

### Implementation Requirements

#### 1. Data Structure Change

```c
// Original
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// Modified
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];   // NCPU is defined as 8 in param.h
```

#### 2. Modify `kinit()`

- Initialize the lock for all CPUs (`initlock`)
- Call `freerange` the same way as before
- Since `kinit` runs only on CPU 0, all initial free pages go into CPU 0's freelist

#### 3. Modify `kfree()`

- Return the page to the current CPU's freelist
- To safely call `cpuid()`, interrupts must be disabled
- Use `push_off()` / `pop_off()` to manage interrupts

```
kfree flow:
  push_off()
  id = cpuid()
  acquire(&kmem[id].lock)
  Add page to freelist
  release(&kmem[id].lock)
  pop_off()
```

#### 4. Modify `kalloc()`

- **First**, attempt to allocate from the current CPU's freelist
- **If the local freelist is empty**, **steal** from another CPU's freelist
- If all CPU freelists are empty, return 0 (out of memory)

```
kalloc flow:
  push_off()
  id = cpuid()

  // 1. Try from own CPU
  acquire(&kmem[id].lock)
  r = remove one from kmem[id].freelist
  release(&kmem[id].lock)

  // 2. If failed, steal from other CPUs
  if(!r) {
    for each other CPU i:
      acquire(&kmem[i].lock)
      r = remove one from kmem[i].freelist
      release(&kmem[i].lock)
      if(r) break
  }

  pop_off()
  return r
```

### Important Notes

1. **Deadlock prevention**: When stealing, hold only one lock at a time. Holding two locks simultaneously risks deadlock.

2. **cpuid() and interrupts**: `cpuid()` reads the `tp` register to return the current CPU number. If called with interrupts enabled, a context switch could cause it to return an incorrect CPU number. It must be called with interrupts disabled.

3. **push_off inside acquire**: `acquire()` internally calls `push_off()`. However, since `cpuid()` must be called before `acquire()`, you need to call `push_off()` separately beforehand.

### Getting Started

A skeleton file is provided:

```bash
# Copy the skeleton file to the xv6 source
cp skeleton/kalloc_percpu.c ../../xv6-riscv/kernel/kalloc.c
```

Find the `TODO` comments in the skeleton file and complete the code.

### Testing

```bash
cd ../../xv6-riscv
make clean
make qemu
```

In the xv6 shell:
```
$ usertests
```

All tests must pass with `OK`. See `tests/test_kalloc.sh` for detailed testing instructions.

### Grading Criteria

| Item | Weight |
|------|------|
| Correctly change `kmem` to a per-CPU array | 20% |
| Initialize all CPU locks in `kinit()` | 10% |
| Return to current CPU freelist in `kfree()` | 20% |
| Prioritize own CPU allocation in `kalloc()` | 20% |
| Implement steal in `kalloc()` | 20% |
| Pass all `usertests` | 10% |

---

## Assignment 2: Barrier Implementation (Bonus, +20 points)

### Background

A **Barrier** is a synchronization technique where multiple threads wait at a specific point until all of them have arrived.

```
Thread 0: --- work ---|   wait   |--- next work ---
Thread 1: --- work ----|  wait  |--- next work ---
Thread 2: --- work ------| wait |--- next work ---
                          ^
                     barrier point
                  (when the last thread arrives,
                   all proceed simultaneously)
```

### Goal

Implement a barrier using only `pthread_mutex` and `pthread_cond`.

### Requirements

1. `barrier_init(b, n)`: Initialize the barrier for n threads
2. `barrier_wait(b)`: Wait at the barrier. All proceed when the last thread arrives
3. `barrier_destroy(b)`: Release resources
4. **Reusable**: The barrier must be usable across multiple rounds

### Key Point: Reusable Barrier

A naive implementation works only once and produces bugs when reused:

```
Problem scenario:
1. Round 0: All 5 threads arrive -> count reset, broadcast
2. Thread A quickly enters barrier_wait for round 1 (count=1)
3. Thread B is still waking up from round 0's wait
   -> When B checks count, it's 1, but it can't tell if this is round 0 or round 1!
```

Solution: Use a **round (generation) number** to distinguish rounds.

### Getting Started

```bash
cd skeleton/
gcc -Wall -pthread -o barrier barrier.c
./barrier
```

Find the `TODO` comments in the skeleton file and complete the code. Test code is already included and validates with 5 threads across 3 rounds.

### Grading Criteria

| Item | Weight |
|------|------|
| Correct initialization in `barrier_init` | 20% |
| Basic `barrier_wait` operation (1 round) | 30% |
| Reusable `barrier_wait` (multiple rounds) | 40% |
| Resource cleanup and code quality | 10% |

---

## Deliverables

```
week5/homework/
├── skeleton/
│   ├── kalloc_percpu.c     <- Complete the TODOs and submit (required)
│   └── barrier.c           <- Complete the TODOs and submit (bonus)
└── report.md               <- Brief implementation description (optional)
```

### report.md Writing Guide (Optional)

Bonus points will be awarded for answering the following questions:

1. In what situations does performance improve after changing to a per-CPU free list?
2. Can deadlock occur during the steal process? How did you prevent it?
3. (Bonus) What bugs occur if the round variable is absent in the barrier?

---

## References

- xv6 textbook Chapter 6: Locking
- xv6 source code: `kernel/kalloc.c`, `kernel/spinlock.c`, `kernel/param.h`
- OSTEP Chapter 28: Locks
- OSTEP Chapter 30: Condition Variables
- OSTEP Chapter 31: Semaphores (Barrier concept)
