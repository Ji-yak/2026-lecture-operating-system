# Lab 4: Deadlock Scenario

## Description

Two threads acquire two locks in opposite order, causing a classic deadlock (circular wait). Thread A acquires `lock1` then waits for `lock2`, while Thread B acquires `lock2` then waits for `lock1`. Neither can proceed because each holds the lock the other needs. The program will hang when deadlock occurs.

## Build & Run

```bash
gcc -Wall -pthread -o lab4_deadlock_demo lab4_deadlock_demo.c
./lab4_deadlock_demo
```

If the program hangs, deadlock has occurred. Use `Ctrl+C` to terminate it.

```bash
# Run multiple times to observe that deadlock is probabilistic
./lab4_deadlock_demo
./lab4_deadlock_demo
./lab4_deadlock_demo
```

To check or kill a hanging process from another terminal:

```bash
ps aux | grep deadlock_demo
kill -9 $(pgrep deadlock_demo)
```

## What to Observe

- The program usually hangs after both threads acquire their first lock
- The last output lines will show each thread waiting for the other's lock
- Deadlock does not occur every time -- it depends on thread scheduling
- A `usleep(100000)` call between the two lock acquisitions increases deadlock probability

## Try It Yourself

- Modify Thread B's lock order to match Thread A's (`lock1` then `lock2`) and verify deadlock no longer occurs
- Remove the `usleep(100000)` calls and observe how it affects deadlock probability
- Identify all 4 Coffman conditions (mutual exclusion, hold and wait, no preemption, circular wait) in the code

## Discussion Questions

- Q1: How does xv6 prevent deadlock? (Hint: lock ordering)
- Q2: Why doesn't deadlock always occur?
- Q3: How does removing `usleep(100000)` affect the probability of deadlock?
