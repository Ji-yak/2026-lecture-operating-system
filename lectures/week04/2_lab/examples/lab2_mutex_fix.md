# Lab 2: Mutex Protection

## Description

This is the same shared counter scenario as Lab 1, but the critical section (`counter++`) is protected with a `pthread_mutex_t`. Only one thread can execute the increment at a time; other threads sleep at `pthread_mutex_lock()` until the lock is available. The final result is always correct.

## Build & Run

```bash
gcc -Wall -pthread -o lab2_mutex_fix lab2_mutex_fix.c
./lab2_mutex_fix
```

Optional arguments: `./lab2_mutex_fix [num_threads] [increments_per_thread]`

```bash
# Compare with the same parameters used in Lab 1
./lab2_mutex_fix 4 1000000
./lab2_mutex_fix 8 1000000

# Compare execution time with race_demo
time ./lab1_race_demo 4 1000000
time ./lab2_mutex_fix 4 1000000
```

## What to Observe

- The "Actual" counter always matches the "Expected" value
- The result is deterministic across multiple runs
- Execution is noticeably slower than `lab1_race_demo` due to lock overhead
- The only code difference from Lab 1 is wrapping `counter++` with lock/unlock

## Try It Yourself

- Use `diff lab1_race_demo.c lab2_mutex_fix.c` to see the exact code differences
- Measure execution time with `time` and compare against Lab 1
- Try moving the lock/unlock outside the for loop (lock once, do all increments, unlock) and observe the performance difference

## Discussion Questions

- Q1: Why does using a mutex make the program slower?
- Q2: Why should the critical section be kept as small as possible?
- Q3: Does a thread waiting at `pthread_mutex_lock()` consume CPU?
