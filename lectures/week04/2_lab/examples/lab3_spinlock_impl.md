# Lab 3: Spinlock (xv6 Model)

## Description

Implements a spinlock using `__sync_lock_test_and_set`, the same atomic primitive used in xv6's `acquire()` function (`kernel/spinlock.c`). Unlike a mutex, a spinlock busy-waits (spins) on the CPU rather than sleeping while waiting for the lock. This demonstrates how the xv6 kernel protects shared data structures.

## Build & Run

```bash
gcc -Wall -pthread -o lab3_spinlock_impl lab3_spinlock_impl.c
./lab3_spinlock_impl
```

Optional arguments: `./lab3_spinlock_impl [num_threads] [increments_per_thread]`

```bash
# Compare performance with the mutex version
time ./lab2_mutex_fix 4 1000000
time ./lab3_spinlock_impl 4 1000000
```

## What to Observe

- The counter result is always correct, just like the mutex version
- Performance may differ from the mutex version depending on contention level
- The spinlock functions (`spinlock_acquire`, `spinlock_release`) mirror xv6's `acquire()` and `release()`
- `__sync_lock_test_and_set` atomically reads, sets, and returns the old value in one instruction

## Try It Yourself

- Compare the `spinlock_acquire()` function with xv6's `acquire()` in `kernel/spinlock.c`
- Measure CPU usage (e.g., with `top` or `htop`) while running the spinlock version vs. the mutex version
- Try removing the `__sync_synchronize()` call and see if the program still works correctly

## Discussion Questions

- Q1: What are the differences between spinlocks and mutexes? (waiting mechanism, CPU usage)
- Q2: Why does xv6 use spinlocks instead of mutexes?
- Q3: Why is `__sync_synchronize()` (memory barrier) needed?
- Q4: What problems arise from using spinlocks in user programs?
