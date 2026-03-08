# Week 4 Homework: Parallel Hash Table

> Textbook: xv6 Chapter 6 (Locking) - Exercise 4
> Submission file: `hashtable.c` (modified file)

## Overview

In this assignment, you will implement a **parallel hash table** using pthreads.
You will implement two locking strategies to ensure safe operation in a multithreaded environment and compare their performance.

- **Phase 1**: Coarse-grained locking (single global mutex)
- **Phase 2**: Fine-grained locking (per-bucket mutex)

## Learning Objectives

- Understand the concepts of critical sections and mutual exclusion
- Analyze the trade-offs of coarse-grained vs. fine-grained locking
- Experiment with the impact of lock granularity on parallel performance
- Compare with xv6 kernel locking strategies (e.g., the freelist lock in `kalloc.c`)

## File Structure

```
homework/
  skeleton/          <- Working directory (code here)
    hashtable.h      - Structure definitions (no modification needed)
    hashtable.c      - Implementation file with TODOs (modify this file)
    main.c           - Benchmark code (no modification needed)
    Makefile         - Build configuration
  solution/          - Reference solution (available after submission)
  tests/             - Test scripts
    test_correctness.sh  - Correctness test
    test_performance.sh  - Performance measurement
```

## Build and Run

```bash
cd skeleton/
make

# Run: ./hashtable_bench [none|coarse|fine] [threads] [keys]
./hashtable_bench none   4 100000   # No locking (race condition occurs)
./hashtable_bench coarse 4 100000   # Coarse-grained
./hashtable_bench fine   4 100000   # Fine-grained
```

## Assignment Details

### Phase 1: Coarse-grained Locking

Modify `hashtable.c` to protect the hash table with a **single global mutex**.

Implementation location: Find the `TODO` comments in `hashtable.c` and modify them

**What to implement:**

1. `hashtable_init()`: Initialize `ht->global_lock` (`pthread_mutex_init`)
2. `hashtable_destroy()`: Destroy `ht->global_lock` (`pthread_mutex_destroy`)
3. `hashtable_insert()`: Acquire the global lock at the start of the function, release before return
4. `hashtable_lookup()`: Same
5. `hashtable_delete()`: Same

**Key idea:**
```c
void hashtable_insert(struct hashtable *ht, int key, int value)
{
    // Lock only when strategy is LOCK_COARSE
    if (ht->strategy == LOCK_COARSE)
        pthread_mutex_lock(&ht->global_lock);

    // ... existing code ...

    if (ht->strategy == LOCK_COARSE)
        pthread_mutex_unlock(&ht->global_lock);
}
```

**Verification:**
```bash
make
./hashtable_bench coarse 4 100000
# "All keys found correctly." should be printed
```

### Phase 2: Fine-grained Locking

Use **per-bucket mutexes** so that threads accessing different buckets can execute concurrently.

**What to implement:**

1. `hashtable_init()`: Initialize each `ht->buckets[i].lock`
2. `hashtable_destroy()`: Destroy each `ht->buckets[i].lock`
3. `hashtable_insert()`: Acquire/release only the relevant bucket's lock
4. `hashtable_lookup()`: Same
5. `hashtable_delete()`: Same

**Key idea:**
```c
void hashtable_insert(struct hashtable *ht, int key, int value)
{
    int idx = hash(key);

    if (ht->strategy == LOCK_FINE)
        pthread_mutex_lock(&ht->buckets[idx].lock);

    // ... existing code (accesses only bucket idx) ...

    if (ht->strategy == LOCK_FINE)
        pthread_mutex_unlock(&ht->buckets[idx].lock);
}
```

**Verification:**
```bash
./hashtable_bench fine 4 100000
# "All keys found correctly." should be printed
```

### Performance Comparison Report

Compare the performance of coarse-grained and fine-grained locking with 1, 2, and 4 threads.

```bash
# Use the test script
cd ../tests/
./test_performance.sh ../skeleton/hashtable_bench
```

Or run manually:
```bash
# Coarse-grained
./hashtable_bench coarse 1 200000
./hashtable_bench coarse 2 200000
./hashtable_bench coarse 4 200000

# Fine-grained
./hashtable_bench fine 1 200000
./hashtable_bench fine 2 200000
./hashtable_bench fine 4 200000
```

## Testing

```bash
# Correctness test (tests both coarse and fine)
cd tests/
./test_correctness.sh ../skeleton/hashtable_bench

# Performance test
./test_performance.sh ../skeleton/hashtable_bench
```

## Deliverables

1. **`hashtable.c`**: Source code with locking implemented
2. **Report** (written as short text or comments):
   - Performance measurement results table (strategy x thread count)
   - Answers to the following questions:

### Report Questions

1. What problems occur when using 4 threads in `none` mode?
   Run `./hashtable_bench none 4 100000` and explain the results.

2. In coarse-grained locking, how does performance change when increasing the number of threads from 1 to 4?
   Why is that?

3. Is fine-grained locking faster than coarse-grained? Why?

4. How does the performance of fine-grained locking change when you change the number of buckets (`NBUCKETS`) from 13 to 101?
   (Hint: Modify `NBUCKETS` in `hashtable.h` and rebuild)

5. What locking strategy does xv6's `kalloc.c` use?
   What advantages would switching to fine-grained locking provide?

## Grading Criteria

| Item | Score |
|------|-------|
| Phase 1: Coarse-grained correctness | 30% |
| Phase 2: Fine-grained correctness | 30% |
| Performance measurement and comparison | 20% |
| Report question answers | 20% |

## Hints

- Find and implement all `TODO` comments in `hashtable.c`.
- Check `ht->strategy` to use the appropriate lock.
- If you acquire a lock, you **must** release it on **every return path**. (Especially the mid-function return in `hashtable_insert`)
- You only need to use `pthread_mutex_init`, `pthread_mutex_lock`, `pthread_mutex_unlock`, and `pthread_mutex_destroy`.
- Creating helper functions makes the code cleaner (refer to the solution).

## References

- xv6 textbook Chapter 6: Locking
- `kernel/spinlock.c`: xv6's spinlock implementation
- `kernel/kalloc.c`: xv6 memory allocator (example of coarse-grained lock usage)
- `pthread_mutex(3)` man page
