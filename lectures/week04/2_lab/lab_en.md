---
theme: default
title: "Week 04 — Lab: Race Conditions and Locks"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 4 — Race Conditions and Locks

Korea University Sejong Campus, Department of Computer Science & Software

---

# Lab Overview

**Duration**: ~50 minutes · 4 labs

```mermaid
graph LR
    L1["Lab 1<br/>Race Condition"] --> L2["Lab 2<br/>Mutex Fix"]
    L2 --> L3["Lab 3<br/>Spinlock"]
    L3 --> L4["Lab 4<br/>Deadlock"]
    style L1 fill:#ffcdd2
    style L2 fill:#c8e6c9
    style L3 fill:#fff3e0
    style L4 fill:#e3f2fd
```

**Setup**:

```bash
cd examples/
gcc -Wall -pthread -o race_demo      race_demo.c
gcc -Wall -pthread -o mutex_fix      mutex_fix.c
gcc -Wall -pthread -o spinlock_impl  spinlock_impl.c
gcc -Wall -pthread -o deadlock_demo  deadlock_demo.c
```

---

# Lab 1: Race Conditions

**Goal**: Observe that `counter++` is **not atomic**

```bash
./race_demo          # 4 threads × 1,000,000 increments
# Expected: 4,000,000  →  Actual: less, different every run!
```

**Why?** `counter++` compiles to **three** CPU instructions:

```mermaid
sequenceDiagram
    participant T1 as Thread A
    participant Mem as counter (= 0)
    participant T2 as Thread B
    T1->>Mem: LOAD reg ← 0
    T2->>Mem: LOAD reg ← 0
    T1->>Mem: STORE reg+1 → 1
    T2->>Mem: STORE reg+1 → 1
    Note over Mem: Expected 2, got 1!<br/>Lost update
```

**Experiment**: Try `./race_demo 2 1000000` vs `./race_demo 8 1000000`
- More threads → more lost updates. Output is **non-deterministic**.

<div class="text-right text-sm text-gray-400 pt-2">

📁 Skeleton: `examples/skeletons/lab1_race_demo.c`

</div>

<div class="text-right text-sm text-gray-400">

📁 Solution: `examples/solutions/lab1_race_demo.c`

</div>

---

# Lab 2: Mutex Protection

**Goal**: Eliminate the race with `pthread_mutex`

<div class="grid grid-cols-2 gap-4">
<div>

```c
pthread_mutex_lock(&lock);
counter++;                // critical section
pthread_mutex_unlock(&lock);
```

```bash
./mutex_fix   # always prints 4,000,000 ✓
```

</div>
<div>

```mermaid
sequenceDiagram
    participant T1 as Thread A
    participant L as Mutex
    participant T2 as Thread B
    T1->>L: lock() ✅
    Note over T1: counter++
    T2->>L: lock() ⏳ blocked
    T1->>L: unlock()
    T2->>L: lock() ✅
    Note over T2: counter++
    T2->>L: unlock()
```

</div>
</div>

**Performance trade-off**: `time ./race_demo` (fast, wrong) vs `time ./mutex_fix` (correct, slower)

> Rule: keep the critical section **as small as possible** to minimize contention.

<div class="text-right text-sm text-gray-400 pt-2">

📁 Skeleton: `examples/skeletons/lab2_mutex_fix.c`

</div>

<div class="text-right text-sm text-gray-400">

📁 Solution: `examples/solutions/lab2_mutex_fix.c`

</div>

---

# Lab 3: Spinlock (xv6 Model)

**Spinlock** = busy-wait using atomic test-and-set (no sleeping)

<div class="grid grid-cols-2 gap-4">
<div>

**User-space spinlock core:**

```c
// acquire
while (__sync_lock_test_and_set(
    &lock->locked, 1) != 0)
    ;   // spin — burn CPU

// release
__sync_lock_release(&lock->locked);
```

**xv6 adds** (`kernel/spinlock.c`):

```c
push_off();   // disable interrupts
while (__sync_lock_test_and_set(...))
    ;
__sync_synchronize(); // memory barrier
lk->cpu = mycpu();
```

</div>
<div>

**Mutex vs Spinlock**:

```mermaid
graph TD
    subgraph "Mutex"
        M1["Thread blocked"] --> M2["💤 Sleep<br/>no CPU used"]
        M2 --> M3["Wake on unlock"]
    end
    subgraph "Spinlock"
        S1["Thread blocked"] --> S2["🔄 Spin<br/>burns CPU!"]
        S2 --> S3["Proceed on unlock"]
    end
    style M2 fill:#c8e6c9
    style S2 fill:#ffcdd2
```

| | Mutex | Spinlock |
|---|---|---|
| Wait | Sleep (no CPU) | Spin (burns CPU) |
| Best for | Long sections | Very short kernel sections |

</div>
</div>

<div class="text-right text-sm text-gray-400 pt-2">

📁 Skeleton: `examples/skeletons/lab3_spinlock_impl.c`

</div>

<div class="text-right text-sm text-gray-400">

📁 Solution: `examples/solutions/lab3_spinlock_impl.c`

</div>

---

# Lab 4: Deadlock Scenario

**Two threads, two locks** — classic circular wait:

```mermaid
graph LR
    TA["Thread A<br/>holds lock1"] -->|"wants"| LB["lock2"]
    TB["Thread B<br/>holds lock2"] -->|"wants"| LA["lock1"]
    LA -.->|held by| TA
    LB -.->|held by| TB
    style TA fill:#ffcdd2
    style TB fill:#ffcdd2
    style LA fill:#fff3e0
    style LB fill:#fff3e0
```

```bash
./deadlock_demo   # hangs! kill with: Ctrl-C
```

**Four Coffman conditions** (ALL must hold for deadlock):

| Condition | Description |
|---|---|
| **Mutual exclusion** | Only one thread holds a lock at a time |
| **Hold and wait** | Hold one lock while requesting another |
| **No preemption** | Locks cannot be forcibly taken away |
| **Circular wait** | A cycle in the dependency graph |

**Fix**: enforce global lock ordering — always acquire `lock1` before `lock2`.

<div class="text-right text-sm text-gray-400 pt-2">

📁 Skeleton: `examples/skeletons/lab4_deadlock_demo.c`

</div>

<div class="text-right text-sm text-gray-400">

📁 Solution: `examples/solutions/lab4_deadlock_demo.c`

</div>

---

# Key Takeaways

```mermaid
graph TD
    RC["🔴 Race Condition<br/>unsynchronized shared state"] -->|fix with| MU["🟢 Mutex<br/>sleep-based lock"]
    RC -->|fix with| SP["🟡 Spinlock<br/>busy-wait lock"]
    MU --> DL["🔵 Deadlock<br/>circular wait"]
    SP --> DL
    DL -->|prevent with| LO["✅ Lock Ordering<br/>break circular wait"]
    style RC fill:#ffcdd2
    style MU fill:#c8e6c9
    style SP fill:#fff3e0
    style DL fill:#bbdefb
    style LO fill:#c8e6c9
```

**You will see these in xv6**:
- `kernel/spinlock.c` — `acquire` / `release` with interrupt disable
- `kernel/kalloc.c` — memory allocator guarded by a spinlock
- `kernel/proc.c` — process table locked per-entry
