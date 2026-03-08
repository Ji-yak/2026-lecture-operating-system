# Lab 2: Bounded Buffer at Scale

## Description

Extends the Producer-Consumer pattern to multiple concurrent threads: 3
producers and 3 consumers sharing a small bounded buffer (size 4). The program
uses `pthread_mutex` and two `pthread_cond` variables for synchronization, adds
`assert` checks for invariants, tracks production/consumption statistics, and
uses a `done` flag with `pthread_cond_broadcast` to cleanly shut down consumers
after all producers finish.

## Build & Run

```bash
gcc -Wall -pthread -o lab2_bounded_buffer lab2_bounded_buffer.c
./lab2_bounded_buffer
```

## What to Observe

- With a buffer of only 4 slots and 3 producers, waiting occurs frequently -- producers block when the buffer fills up.
- Consumer threads share the work; each consumes a different number of items depending on scheduling.
- The final summary verifies that `total_produced == total_consumed == 30` (3 producers x 10 items each).
- The `done` flag combined with `pthread_cond_broadcast` ensures all consumers wake up and exit gracefully when production is complete.

## Experiments

- **Replace `while` with `if`** in `bbuf_put` and `bbuf_get`: With multiple producers and consumers, spurious or stolen wakeups can cause the `assert` to fire, demonstrating why `while` is essential under Mesa semantics.
- **Replace `pthread_cond_signal` with `pthread_cond_broadcast`**: The program still works correctly but generates unnecessary wakeups. Measure the difference in runtime or CPU usage.
- **Change `BUFFER_SIZE` to 1**: Forces near-strict alternation; concurrency drops and total runtime increases.
- **Change `BUFFER_SIZE` to 100**: The buffer rarely fills, so producers almost never wait. Observe how this affects throughput.
- **Reduce `NUM_CONSUMERS` to 1**: Creates a consumption bottleneck; producers spend more time waiting for buffer space.
- **Increase `NUM_PRODUCERS` to 10**: Amplifies contention on the buffer lock; observe how the system behaves under heavy load.
- **Remove the `done` flag and broadcast**: Observe that consumers may hang indefinitely after all producers finish, because no one signals `not_empty` anymore.

## Discussion Questions

- Why does the program use `pthread_cond_broadcast` (not `signal`) when setting the `done` flag? What would happen if only one consumer were woken?
- In `bbuf_get`, the condition is `while (b->count == 0 && !b->done)`. Why must both conditions be checked together?
- The `printf` after `bbuf_put` reads `bbuf.count` without holding the lock. Is this a bug? What are the consequences?
- Why are the `assert` statements useful even though they are not part of the synchronization logic?
- How could you redesign the shutdown mechanism to avoid the `done` flag entirely? (Hint: consider sentinel values in the buffer.)
- What is the performance implication of all producers and consumers sharing a single mutex? How might you reduce contention?
