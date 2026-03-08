# Lab 1: Producer-Consumer Problem

## Description

Implements the classic Producer-Consumer synchronization problem using a bounded
circular buffer. One producer thread generates items and one consumer thread
retrieves them, coordinated with `pthread_mutex` for mutual exclusion and two
`pthread_cond` variables (`not_full`, `not_empty`) for signaling buffer state
changes.

## Build & Run

```bash
gcc -Wall -pthread -o lab1_producer_consumer lab1_producer_consumer.c
./lab1_producer_consumer
```

## What to Observe

- The producer waits when the buffer is full (5 slots) and resumes after the consumer frees a slot.
- The consumer waits when the buffer is empty and resumes after the producer adds an item.
- The buffer occupancy counter rises and falls as items are produced and consumed at different random rates.
- All 20 items are produced and consumed exactly once, and the program terminates cleanly.

## Experiments

- **Change `while` to `if`** in `buffer_put` and `buffer_get`: With only one producer and one consumer this may still appear to work, but it is incorrect under Mesa semantics and will break once you add more threads (see Lab 2).
- **Replace `pthread_cond_signal` with `pthread_cond_broadcast`**: The program still works correctly, but broadcast wakes all waiters unnecessarily when there is only one thread on each side.
- **Change `BUFFER_SIZE` to 1**: Forces strict alternation between producer and consumer, eliminating any concurrent overlap.
- **Change `BUFFER_SIZE` to 100**: The producer runs far ahead before the consumer catches up; waiting messages almost disappear.
- **Remove the `usleep` calls**: Both threads run at full speed, making the interaction between waiting and signaling more visible.

## Discussion Questions

- Why must `while` be used instead of `if` when checking the condition before `pthread_cond_wait`? (Hint: spurious wakeup, Mesa semantics)
- What happens to the mutex when `pthread_cond_wait` is called? (Hint: it atomically unlocks the mutex and puts the thread to sleep; the mutex is automatically re-acquired upon wakeup)
- What is the difference between `pthread_cond_signal` and `pthread_cond_broadcast`, and when would you choose one over the other?
- What would happen if the producer called `pthread_cond_signal(&buf->not_full)` instead of `pthread_cond_signal(&buf->not_empty)` after inserting an item?
