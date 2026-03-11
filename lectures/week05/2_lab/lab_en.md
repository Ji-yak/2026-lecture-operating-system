---
theme: default
title: "Week 05 — Lab: Advanced Synchronization"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 5 — Advanced Synchronization

Korea University Sejong Campus, Department of Computer Science & Software

---

# Lab Overview

**Duration**: ~50 minutes · 4 exercises

```mermaid
graph LR
    E1["Ex 1<br/>Producer-Consumer"] --> E2["Ex 2<br/>Bounded Buffer"]
    E2 --> E3["Ex 3<br/>xv6 spinlock.c"]
    E3 --> E4["Ex 4<br/>xv6 kalloc.c"]
    style E1 fill:#e3f2fd
    style E2 fill:#fff3e0
    style E3 fill:#e8f5e9
    style E4 fill:#fce4ec
```

**Setup**:

```bash
cd examples/
gcc -Wall -pthread -o producer_consumer  producer_consumer.c
gcc -Wall -pthread -o bounded_buffer     bounded_buffer.c
```

---

# Exercise 1: Producer-Consumer

**Goal**: Coordinate producer and consumer with **condition variable** + **mutex**

<div class="grid grid-cols-2 gap-4">
<div>

```c
// Producer
pthread_mutex_lock(&buf->mutex);
while (buf->count == BUFFER_SIZE)
    pthread_cond_wait(
        &buf->not_full,
        &buf->mutex);
buf->data[buf->in] = item;
buf->in = (buf->in + 1) % BUFFER_SIZE;
buf->count++;
pthread_cond_signal(&buf->not_empty);
pthread_mutex_unlock(&buf->mutex);
```

</div>
<div>

```mermaid
graph TD
    P["Producer"] -->|"put item"| B["Bounded<br/>Buffer"]
    B -->|"get item"| C["Consumer"]
    P -->|"full?"| W1["⏳ wait(not_full)"]
    C -->|"empty?"| W2["⏳ wait(not_empty)"]
    W1 -.->|"signal"| C
    W2 -.->|"signal"| P
    style B fill:#fff3e0
    style W1 fill:#ffcdd2
    style W2 fill:#ffcdd2
```

</div>
</div>

**Why `while`, not `if`?** — POSIX uses **Mesa semantics**: spurious wakeups are possible. Always recheck.

<div class="text-right text-sm text-gray-400">📁 Skeleton: `examples/skeletons/lab1_producer_consumer.c`</div>
<div class="text-right text-sm text-gray-400">📁 Solution: `examples/solutions/lab1_producer_consumer.c`</div>

---

# Exercise 2: Bounded Buffer at Scale

**3 producers + 3 consumers**, `BUFFER_SIZE = 4`

```bash
./bounded_buffer   # every item produced is consumed exactly once
```

**Experiments**:

| Change | Expected Effect |
|---|---|
| Replace `while` with `if` | Assertion failures or wrong count |
| Replace `signal` with `broadcast` | Correct but more spurious wakeups |
| Set `BUFFER_SIZE = 1` | Strict alternation, low concurrency |
| Reduce consumers to 1 | Consumer bottleneck, producers wait more |

<div class="text-right text-sm text-gray-400">📁 Skeleton: `examples/skeletons/lab2_bounded_buffer.c`</div>
<div class="text-right text-sm text-gray-400">📁 Solution: `examples/solutions/lab2_bounded_buffer.c`</div>

```mermaid
graph LR
    P1["Producer 1"] --> B["Buffer<br/>(size 4)"]
    P2["Producer 2"] --> B
    P3["Producer 3"] --> B
    B --> C1["Consumer 1"]
    B --> C2["Consumer 2"]
    B --> C3["Consumer 3"]
    style B fill:#fff3e0
```

---

# Exercise 3: xv6 spinlock.c Analysis

**File**: `xv6-riscv/kernel/spinlock.c`

<div class="grid grid-cols-2 gap-4">
<div>

**`acquire` — four steps:**

```c
void acquire(struct spinlock *lk) {
  push_off();        // 1. disable interrupts
  if (holding(lk))
      panic("acquire"); // 2. re-entrancy check
  while (__sync_lock_test_and_set(
      &lk->locked, 1)) // 3. atomic spin
      ;
  __sync_synchronize(); // 4. memory barrier
  lk->cpu = mycpu();
}
```

</div>
<div>

**`push_off` / `pop_off`** — nestable interrupt disable:

```mermaid
graph TD
    A["acquire(lock_A)<br/>push_off: noff=1<br/>interrupts OFF"] --> B["acquire(lock_B)<br/>push_off: noff=2"]
    B --> C["release(lock_B)<br/>pop_off: noff=1<br/>still OFF"]
    C --> D["release(lock_A)<br/>pop_off: noff=0<br/>interrupts ON"]
    style A fill:#ffcdd2
    style D fill:#c8e6c9
```

Prevents self-deadlock: interrupt handler cannot spin on a lock held by the interrupted code.

</div>
</div>

---

# Exercise 4: xv6 kalloc.c Lock Analysis

**File**: `xv6-riscv/kernel/kalloc.c` — one global freelist + one lock

<div class="grid grid-cols-2 gap-4">
<div>

```c
struct {
    struct spinlock lock;
    struct run *freelist;
} kmem;  // ALL CPUs share this
```

```c
// kfree: memset BEFORE lock
memset(pa, 1, PGSIZE);
acquire(&kmem.lock);
r->next = kmem.freelist;
kmem.freelist = r;
release(&kmem.lock);

// kalloc: memset AFTER lock
acquire(&kmem.lock);
r = kmem.freelist;
if(r) kmem.freelist = r->next;
release(&kmem.lock);
memset((char*)r, 5, PGSIZE);
```

</div>
<div>

**Scalability problem:**

```mermaid
graph TD
    subgraph "Current: Global Lock"
        CPU0["CPU 0"] --> GL["🔒 kmem.lock"]
        CPU1["CPU 1"] --> GL
        CPU2["CPU 2"] --> GL
        GL --> FL["freelist"]
    end
    style GL fill:#ffcdd2
```

```mermaid
graph TD
    subgraph "Homework: Per-CPU"
        C0["CPU 0"] --> L0["🔒 lock_0"]
        C1["CPU 1"] --> L1["🔒 lock_1"]
        C2["CPU 2"] --> L2["🔒 lock_2"]
        L0 --> F0["freelist_0"]
        L1 --> F1["freelist_1"]
        L2 --> F2["freelist_2"]
    end
    style L0 fill:#c8e6c9
    style L1 fill:#c8e6c9
    style L2 fill:#c8e6c9
```

</div>
</div>

---

# Key Takeaways

| Concept | Key Insight |
|---|---|
| **Condition variables** | `mutex` + `cond_wait` in a `while` loop — always recheck |
| **xv6 spinlock** | disable interrupts + atomic spin + memory barrier |
| **push_off/pop_off** | nestable interrupt disable prevents self-deadlock |
| **Lock granularity** | global lock = simple but bottleneck; per-CPU = parallel |

```mermaid
graph LR
    CV["Condition<br/>Variables"] --> PC["Producer-<br/>Consumer"]
    SL["Spinlock"] --> XV["xv6 kernel<br/>locks"]
    XV --> KA["kalloc.c<br/>global → per-CPU"]
    style CV fill:#e3f2fd
    style SL fill:#fff3e0
    style KA fill:#e8f5e9
```

> **Minimize work inside critical sections** — `kalloc.c` deliberately places `memset` outside the lock.
