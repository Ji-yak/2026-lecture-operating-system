# Week 5 Lab: Thread & Concurrency 2 - Advanced Synchronization

> In-class lab (~50 minutes)
>
> Textbook reference: xv6 textbook Ch 6.6-6.10 (Advanced Locking), Ch 9 (Concurrency Revisited)

---

## Learning Objectives

- Understand thread communication patterns using condition variables
- Correctly implement the Producer-Consumer problem
- Analyze the spinlock implementation in the xv6 kernel and explain the role of each function
- Explain what the lock in xv6's kalloc.c protects and why it is necessary

---

## Exercise 1: Implementing the Producer-Consumer Problem (15 min)

### Background

Producer-Consumer is a classic synchronization problem. With a shared buffer in between:
- **Producer**: Generates data and places it into the buffer
- **Consumer**: Retrieves data from the buffer and processes it

Key conditions:
- If the buffer is full, the producer must **wait**
- If the buffer is empty, the consumer must **wait**
- **Mutual exclusion** must be guaranteed when accessing the buffer

### Synchronization Tools Used

| Tool | Role |
|------|------|
| `pthread_mutex_t` | Mutual exclusion for shared data (buffer) |
| `pthread_cond_t not_full` | Notifies that space is available in the buffer |
| `pthread_cond_t not_empty` | Notifies that data has been placed in the buffer |

### Lab Walkthrough

1. Compile and run the example code:

```bash
cd examples/
gcc -Wall -pthread -o lab1_producer_consumer lab1_producer_consumer.c
./lab1_producer_consumer
```

2. Open `lab1_producer_consumer.c` and focus on the following sections:

#### (a) `buffer_put` Function - Producer Operation

```c
void buffer_put(bounded_buffer_t *buf, int item) {
    pthread_mutex_lock(&buf->mutex);

    // Why while instead of if?
    while (buf->count == BUFFER_SIZE) {
        pthread_cond_wait(&buf->not_full, &buf->mutex);
    }

    buf->data[buf->in] = item;
    buf->in = (buf->in + 1) % BUFFER_SIZE;
    buf->count++;

    pthread_cond_signal(&buf->not_empty);
    pthread_mutex_unlock(&buf->mutex);
}
```

#### (b) `buffer_get` Function - Consumer Operation

```c
int buffer_get(bounded_buffer_t *buf) {
    int item;
    pthread_mutex_lock(&buf->mutex);

    while (buf->count == 0) {
        pthread_cond_wait(&buf->not_empty, &buf->mutex);
    }

    item = buf->data[buf->out];
    buf->out = (buf->out + 1) % BUFFER_SIZE;
    buf->count--;

    pthread_cond_signal(&buf->not_full);
    pthread_mutex_unlock(&buf->mutex);
    return item;
}
```

### Points to Consider

- Why is `while` used instead of `if` when checking the condition in `pthread_cond_wait`?
  - Hint: **spurious wakeup**, Mesa semantics
- What happens to the mutex when `pthread_cond_wait` is called?
  - Hint: atomically unlock + sleep, automatically relock upon wakeup
- What is the difference between `signal` and `broadcast`?

---

## Exercise 2: Multi-Producer/Consumer Bounded Buffer (10 min)

### Lab Walkthrough

1. Compile and run:

```bash
gcc -Wall -pthread -o lab2_bounded_buffer lab2_bounded_buffer.c
./lab2_bounded_buffer
```

2. `lab2_bounded_buffer.c` has **3 Producers + 3 Consumers** operating concurrently. Observe the following:

- Since the buffer size is only 4, waiting occurs frequently
- Verify that all items are produced/consumed exactly once

3. **Experiment**: Modify the code and try the following:

| Change | Expected Result |
|------|-----------|
| Replace `while` with `if` | Possible race condition (assert failure) |
| Replace `signal` with `broadcast` | Works correctly, but unnecessary wakeups increase |
| Change `BUFFER_SIZE` to 1 | Alternating execution, reduced concurrency |
| Reduce number of Consumers to 1 | Consumption bottleneck, increased producer waiting |

---

## Exercise 3: xv6 kernel/spinlock.c Code Analysis (15 min)

> File location: `xv6-riscv/kernel/spinlock.c`

Let's read through xv6's spinlock implementation together and analyze the role of each function.

### 3-1. `acquire` Function

```c
void acquire(struct spinlock *lk)
{
    push_off();   // (1) Disable interrupts
    if(holding(lk))
        panic("acquire");

    // (2) Attempt to acquire lock via atomic swap
    while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
        ;

    // (3) Memory fence
    __sync_synchronize();

    // (4) Record debugging information
    lk->cpu = mycpu();
}
```

#### Analysis Points

| Step | Code | Question |
|------|------|------|
| (1) | `push_off()` | Why must interrupts be disabled before acquiring the lock? |
| (2) | `__sync_lock_test_and_set` | How does this atomic operation differ from test-and-set? |
| (3) | `__sync_synchronize` | What problems can arise without a memory fence? |
| (4) | `lk->cpu = mycpu()` | Where is this information used? |

**Why interrupts must be disabled (textbook Ch 6.6)**:
- An interrupt occurs while the CPU holds the lock
- The interrupt handler attempts to acquire the same lock
- It spins forever waiting for a lock held by itself -> **deadlock**

### 3-2. `release` Function

```c
void release(struct spinlock *lk)
{
    if(!holding(lk))
        panic("release");

    lk->cpu = 0;

    __sync_synchronize();              // (1) Memory fence
    __sync_lock_release(&lk->locked);  // (2) Atomic: locked = 0

    pop_off();                         // (3) Restore interrupts
}
```

**Question**: Why must `__sync_synchronize()` come **before** `__sync_lock_release`?
- Hint: Stores inside the critical section must not be reordered past the lock release

### 3-3. `push_off` / `pop_off`

```c
void push_off(void) {
    int old = intr_get();
    intr_off();
    if(mycpu()->noff == 0)
        mycpu()->intena = old;    // Save previous state on first push
    mycpu()->noff += 1;           // Increment nesting count
}

void pop_off(void) {
    struct cpu *c = mycpu();
    if(intr_get())
        panic("pop_off - interruptible");
    if(c->noff < 1)
        panic("pop_off");
    c->noff -= 1;
    if(c->noff == 0 && c->intena)
        intr_on();                // Restore only after all locks released
}
```

**Key Concept: Nestable Interrupt Disabling**

- When locks are acquired in a nested manner (multiple `push_off` calls), interrupts must not be re-enabled immediately
- Tracked using the `noff` counter so interrupts are only restored when the last lock is released
- `intena` remembers the interrupt state at the time of the first `push_off` call

**Question**: If lock A is acquired and then lock B is acquired, what is the value of `noff`? Are interrupts re-enabled when lock B is released?

---

## Exercise 4: Analyzing the Role of Locks in xv6 kernel/kalloc.c (10 min)

> File location: `xv6-riscv/kernel/kalloc.c`

### Structure Analysis

```c
struct run {
    struct run *next;
};

struct {
    struct spinlock lock;
    struct run *freelist;
} kmem;
```

- `kmem` is a global variable with only **one instance** in the entire system
- `freelist` is a linked list of available physical memory pages
- `lock` protects this linked list

### kfree Analysis

```c
void kfree(void *pa) {
    struct run *r;

    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");

    memset(pa, 1, PGSIZE);       // Fill with junk to detect dangling references

    r = (struct run*)pa;

    acquire(&kmem.lock);          // --- critical section start ---
    r->next = kmem.freelist;      // Insert at head of list
    kmem.freelist = r;
    release(&kmem.lock);          // --- critical section end ---
}
```

### kalloc Analysis

```c
void *kalloc(void) {
    struct run *r;

    acquire(&kmem.lock);          // --- critical section start ---
    r = kmem.freelist;
    if(r)
        kmem.freelist = r->next;  // Remove from head of list
    release(&kmem.lock);          // --- critical section end ---

    if(r)
        memset((char*)r, 5, PGSIZE);
    return (void*)r;
}
```

### Points to Consider

1. **What would happen without the lock?**
   - What if two CPUs call `kalloc` simultaneously?
   - The same page could be allocated twice (double allocation)

2. **Performance issue**:
   - All CPUs share **a single lock**
   - As the number of CPUs increases, **contention** occurs during `kalloc`/`kfree` calls
   - This is the motivation for this week's assignment: improve with **per-CPU free lists**!

3. **Placement of memset**:
   - In `kfree`, `memset(pa, 1, PGSIZE)` -- executed outside the lock (not yet on the freelist)
   - In `kalloc`, `memset((char*)r, 5, PGSIZE)` -- executed outside the lock (already removed from the freelist)
   - Performance is improved by minimizing the scope of the lock

---

## Summary

| Topic | Key Points |
|------|-----------|
| Condition Variable | Synchronize conditions between threads using `while` loop + `wait`/`signal` pattern |
| Bounded Buffer | mutex + 2 cond vars (`not_full`, `not_empty`) |
| xv6 spinlock | atomic swap + memory fence + interrupt disable |
| push_off/pop_off | Nestable interrupt management (`noff` counter) |
| kalloc lock | A single lock protects the free list; can be improved with per-CPU approach |

---

## References

- xv6 textbook Chapter 6: Locking (especially 6.6-6.10)
- xv6 textbook Chapter 9: Concurrency Revisited
- OSTEP Chapter 30: Condition Variables
- `man pthread_cond_wait`, `man pthread_mutex_lock`
