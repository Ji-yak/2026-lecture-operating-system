# Week 4 Lab: Thread & Concurrency 1 - Race Conditions and Locks

> In-class lab (~50 min)
> Textbook: xv6 Chapter 5-6 (Interrupts, Locking)

## Learning Objectives

Through this lab, you will understand:

- **Race conditions** that occur when multiple threads simultaneously access shared resources
- How to protect critical sections using **mutexes**
- The principles and implementation of **spinlocks** used in the xv6 kernel
- The conditions under which **deadlocks** occur and how to reproduce them

## Prerequisites

```bash
# Navigate to the lab directory
cd examples/

# Compile all examples at once
gcc -Wall -pthread -o race_demo race_demo.c
gcc -Wall -pthread -o mutex_fix mutex_fix.c
gcc -Wall -pthread -o spinlock_impl spinlock_impl.c
gcc -Wall -pthread -o deadlock_demo deadlock_demo.c
```

---

## Lab 1: Observing Race Conditions (~12 min)

### Background

When multiple threads simultaneously increment a shared variable (`counter`) without synchronization, the result differs from what is expected.

`counter++` appears to be a single line, but from the CPU's perspective it involves three steps:
```
1. LOAD  counter -> register    (read value from memory)
2. ADD   1 -> register          (add 1 in register)
3. STORE register -> counter    (write result to memory)
```

If two threads LOAD simultaneously, they read the same value, each adds 1, and stores the result.
As a result, what should have been an increase of 2 only increases by 1.

### Execution

```bash
# Default run: 4 threads, 1,000,000 increments each
./race_demo

# Try changing the number of threads
./race_demo 2 1000000
./race_demo 8 1000000

# Run multiple times to see if the result varies each time
for i in 1 2 3 4 5; do ./race_demo 4 1000000; echo "---"; done
```

### Checklist

- [ ] Did you observe the difference between Expected and Actual values?
- [ ] Does the result change on each run? (non-deterministic behavior)
- [ ] Do more lost increments occur when the number of threads increases?

### Questions

1. Why does a race condition occur even though `counter++` is a single line?
2. Why is the result different on each run?
3. Does a race condition occur when the number of threads is 1?

---

## Lab 2: Protecting with Mutex (~10 min)

### Background

Protecting the critical section with `pthread_mutex_t` ensures only one thread executes `counter++` at a time. The remaining threads wait (sleep) at `pthread_mutex_lock()`.

### Execution

```bash
# Run the mutex version
./mutex_fix

# Compare with the same parameters as race_demo
./mutex_fix 4 1000000
./mutex_fix 8 1000000
```

### Code Comparison

Open `race_demo.c` and `mutex_fix.c` side by side to compare the differences:

```bash
diff race_demo.c mutex_fix.c
```

Key difference:
```c
// race_demo.c (no protection)
counter++;

// mutex_fix.c (protected with mutex)
pthread_mutex_lock(&lock);
counter++;
pthread_mutex_unlock(&lock);
```

### Checklist

- [ ] Does the result always match the Expected value?
- [ ] Is the result always the same across multiple runs?
- [ ] Is the execution time slower than `race_demo`? Why?

### Questions

1. Why does using a mutex make it slower?
2. Why should the critical section be kept as small as possible?
3. Does a thread waiting at `pthread_mutex_lock()` consume CPU?

---

## Lab 3: Spinlock Implementation (~15 min)

### Background

The xv6 kernel uses **spinlocks** instead of `pthread_mutex`.
A spinlock busy-waits (repeatedly checks) on the CPU until the lock is acquired.

xv6's `kernel/spinlock.c` acquire function:
```c
void acquire(struct spinlock *lk)
{
    push_off(); // disable interrupts
    while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
        ;
    __sync_synchronize();
}
```

`__sync_lock_test_and_set(&lk->locked, 1)` atomically:
1. Reads the current value of `lk->locked`
2. Sets `lk->locked` to 1
3. Returns the previous value

If the previous value was 0, the lock has been acquired; if 1, another thread holds it, so it continues spinning.

### Execution

```bash
# Run the spinlock version
./spinlock_impl

# Compare with the mutex version
time ./mutex_fix 4 1000000
time ./spinlock_impl 4 1000000
```

### Code Analysis

Open `spinlock_impl.c` and analyze the following functions:

- `spinlock_init()`: Lock initialization
- `spinlock_acquire()`: Uses `__sync_lock_test_and_set`
- `spinlock_release()`: Uses `__sync_lock_release`

### Checklist

- [ ] Are the results correct with the spinlock as well?
- [ ] Did you measure the performance difference between mutex and spinlock?
- [ ] Can you explain how `__sync_lock_test_and_set` works?

### Questions

1. What are the differences between spinlocks and mutexes? (waiting mechanism, CPU usage)
2. Why does xv6 use spinlocks instead of mutexes?
3. Why is `__sync_synchronize()` (memory barrier) needed?
4. What problems arise from using spinlocks in user programs?

---

## Lab 4: Deadlock Scenario (~13 min)

### Background

A **deadlock** is a state where two or more threads are permanently stuck waiting for locks held by each other.

The 4 conditions for deadlock (Coffman conditions):
1. **Mutual exclusion**: A resource can be used by only one thread at a time
2. **Hold and wait**: Waiting for another lock while holding a lock
3. **No preemption**: Cannot forcibly take a lock from another thread
4. **Circular wait**: Circular dependency of the form A->B->A

In this example:
- Thread A: acquires `lock1` -> waits for `lock2`
- Thread B: acquires `lock2` -> waits for `lock1`

### Execution

```bash
# Deadlock demo (program may hang, terminate with Ctrl+C)
./deadlock_demo

# Run multiple times to observe deadlock occurrence
# (may not occur every time - depends on timing)
```

### Analysis

If the program hangs, a deadlock has occurred. Check from another terminal:
```bash
# macOS
ps aux | grep deadlock_demo

# Force kill
kill -9 $(pgrep deadlock_demo)
```

### Solution

The simplest way to prevent deadlock: **All threads acquire locks in the same order**

```c
// Thread A: lock1 -> lock2 (current)
// Thread B: lock2 -> lock1 (current - deadlock possible!)

// Fix: Both threads acquire locks in the order lock1 -> lock2
// Thread A: lock1 -> lock2
// Thread B: lock1 -> lock2  <-- unified ordering!
```

### Challenge (Optional)

Modify `deadlock_demo.c` to:
1. Change Thread B's lock acquisition order to match Thread A's
2. Verify that deadlock no longer occurs after the modification

### Checklist

- [ ] Did you observe the program hanging due to a deadlock?
- [ ] Can you identify all 4 Coffman conditions in this example?
- [ ] Did you verify that unifying the lock ordering resolves the deadlock?

### Questions

1. How does xv6 prevent deadlock? (Hint: lock ordering)
2. Why doesn't deadlock always occur?
3. How does removing `usleep(100000)` affect the probability of deadlock?

---

## Summary and Key Takeaways

| Concept | Description | xv6 Relevance |
|---------|-------------|---------------|
| Race condition | Non-deterministic results from unsynchronized access to shared resources | All shared data structures require locks |
| Mutex | Sleep-based lock, yields CPU while waiting | For user programs |
| Spinlock | Busy-wait-based lock, occupies CPU while waiting | Default lock in the xv6 kernel |
| Deadlock | All threads permanently stuck due to circular waiting | Prevented via lock ordering |

### Where to Find This in xv6 Code

- `kernel/spinlock.h`: Spinlock structure definition
- `kernel/spinlock.c`: `acquire()`, `release()` implementation
- `kernel/kalloc.c`: Example of spinlock usage in the memory allocator
- `kernel/bio.c`: Example of lock usage in the buffer cache
