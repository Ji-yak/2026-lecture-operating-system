# Lab 1: Race Conditions

## Description

Multiple threads increment a shared counter (`counter++`) without any synchronization. Although `counter++` looks like a single operation, the CPU executes it in three steps (LOAD, ADD, STORE). When two threads LOAD the same value simultaneously, one increment is lost. The final counter value will be less than expected.

## Build & Run

```bash
gcc -Wall -pthread -o lab1_race_demo lab1_race_demo.c
./lab1_race_demo
```

Optional arguments: `./lab1_race_demo [num_threads] [increments_per_thread]`

```bash
# Try different thread counts
./lab1_race_demo 2 1000000
./lab1_race_demo 8 1000000

# Run multiple times to see non-deterministic results
for i in 1 2 3 4 5; do ./lab1_race_demo 4 1000000; echo "---"; done
```

## What to Observe

- The "Actual" counter is less than the "Expected" value
- The result differs on every run (non-deterministic behavior)
- More threads generally means more lost increments
- The MISMATCH message shows exactly how many increments were lost

## Try It Yourself

- Run with 1 thread and confirm no race condition occurs
- Increase the number of threads (e.g., 16) and observe whether lost increments increase
- Decrease `increments_per_thread` to a small value (e.g., 100) and check if the race still appears

## Discussion Questions

- Q1: Why does a race condition occur even though `counter++` is a single line of C code?
- Q2: Why is the result different on each run?
- Q3: Does a race condition occur when the number of threads is 1?
