/*
 * spinlock_impl.c - Spinlock Implementation from Scratch
 *
 * [Key Concept]
 * A spinlock is a synchronization mechanism that repeatedly attempts
 * to acquire the lock in a loop (busy-wait). Unlike a mutex, it does
 * not put the thread to sleep and instead consumes CPU while waiting,
 * making it efficient when the lock is held for very short durations.
 *
 * This program implements a spinlock using __sync_lock_test_and_set,
 * the same atomic operation used in the xv6 kernel's acquire()/release()
 * functions.
 *
 * xv6 kernel code (kernel/spinlock.c):
 *   while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
 *     ;
 *
 * Spinlock vs Mutex comparison:
 *   - Spinlock: busy-wait (CPU consuming), suitable for short critical sections, mainly used in kernels
 *   - Mutex: sleep-wait (CPU yielding), suitable for long critical sections, mainly used in user space
 *
 * Compile: gcc -Wall -pthread -o spinlock_impl spinlock_impl.c
 * Run:     ./spinlock_impl [num_threads] [increments_per_thread]
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* Default number of threads */
#define DEFAULT_THREADS     4
/* Default number of increments each thread performs */
#define DEFAULT_INCREMENTS  1000000

/* ---- Spinlock implementation ---- */

struct spinlock {
    /*
     * locked field: variable representing the lock state
     *   - 0: unlocked state
     *   - 1: locked state
     * volatile: ensures the compiler does not optimize away accesses to this variable
     */
    volatile int locked;
};

/*
 * spinlock_init: Initializes the spinlock.
 * Sets it to the unlocked state (0).
 */
void spinlock_init(struct spinlock *lk)
{
    lk->locked = 0;
}

/*
 * spinlock_acquire: Acquires the spinlock (equivalent to xv6's acquire function).
 *
 * How __sync_lock_test_and_set(&lk->locked, 1) works:
 *   This function performs the following as a single atomic operation:
 *   1. Reads the current value of lk->locked (the old value)
 *   2. Sets lk->locked to 1
 *   3. Returns the old value
 *
 *   If the old value was 0: the lock was free, so acquisition succeeds
 *   If the old value was 1: another thread holds the lock, so keep spinning
 *
 *   Because this operation is atomic, it is impossible for two threads to
 *   acquire the lock simultaneously.
 *   (The hardware guarantees LOAD and STORE as a single instruction)
 */
void spinlock_acquire(struct spinlock *lk)
{
    /* Spin in an infinite loop until the lock is acquired (busy-wait, spin) */
    while (__sync_lock_test_and_set(&lk->locked, 1) != 0) {
        /*
         * Keep spinning (busy-waiting) here.
         * Although it consumes CPU resources, for short critical sections
         * where the lock is expected to be released soon, this is more
         * efficient than the cost of a context switch.
         *
         * In actual OS kernels (e.g., xv6), interrupts are disabled
         * before spinning to prevent preemption while holding the lock.
         */
    }
    /*
     * __sync_synchronize: Inserts a memory barrier (memory fence).
     *
     * CPUs and compilers may reorder memory accesses for performance
     * optimization. A memory barrier forces all memory operations before
     * this barrier to complete before any subsequent operations execute.
     *
     * This prevents reads/writes in the critical section from being
     * executed before the lock acquisition.
     *
     * Note: __sync_lock_test_and_set already includes an acquire barrier,
     * but __sync_synchronize makes the guarantee explicit.
     */
    __sync_synchronize();
}

/*
 * spinlock_release: Releases the spinlock (equivalent to xv6's release function).
 *
 * __sync_lock_release: Atomically sets lk->locked to 0 and performs
 * a release memory barrier.
 */
void spinlock_release(struct spinlock *lk)
{
    /*
     * __sync_synchronize: Ensures all memory operations in the critical
     * section complete before the lock is released.
     * If writes within the critical section are delayed past the lock
     * release, other threads may see incomplete data.
     */
    __sync_synchronize();

    /*
     * __sync_lock_release: Atomically sets locked to 0.
     * Unlike a simple assignment lk->locked = 0, this has release
     * semantics that correctly guarantee memory ordering.
     */
    __sync_lock_release(&lk->locked);
}

/* ---- Counter increment demo using spinlock ---- */

/* Shared counter - protected by spinlock */
int counter = 0;
/* Global spinlock instance */
struct spinlock lock;
/* Number of increments each thread performs */
int increments_per_thread;

/*
 * Function executed by each thread
 * Safely increments the counter under spinlock protection.
 */
void *increment(void *arg)
{
    int tid = *(int *)arg;
    for (int i = 0; i < increments_per_thread; i++) {
        /* Acquire spinlock: busy-wait until the lock is obtained */
        spinlock_acquire(&lock);

        /* [Critical section] Only one thread can execute this code at a time */
        counter++;

        /* Release spinlock: waiting threads can now enter */
        spinlock_release(&lock);
    }
    printf("[Thread %d] finished %d increments\n", tid, increments_per_thread);
    return NULL;
}

int main(int argc, char *argv[])
{
    /* Parse command-line arguments */
    int nthreads = (argc > 1) ? atoi(argv[1]) : DEFAULT_THREADS;
    increments_per_thread = (argc > 2) ? atoi(argv[2]) : DEFAULT_INCREMENTS;

    if (nthreads <= 0 || increments_per_thread <= 0) {
        fprintf(stderr, "Usage: %s [num_threads] [increments_per_thread]\n", argv[0]);
        return 1;
    }

    /* Calculate expected result */
    int expected = nthreads * increments_per_thread;

    printf("=== Spinlock Implementation Demo ===\n");
    printf("Threads: %d, Increments per thread: %d\n", nthreads, increments_per_thread);
    printf("Expected final counter: %d\n\n", expected);

    /* Initialize spinlock (locked = 0) */
    spinlock_init(&lock);

    /* Dynamically allocate arrays for thread handles and IDs */
    pthread_t *threads = malloc(sizeof(pthread_t) * nthreads);
    int *tids = malloc(sizeof(int) * nthreads);

    /* Create threads */
    for (int i = 0; i < nthreads; i++) {
        tids[i] = i;
        /*
         * pthread_create: Creates a new thread to execute the increment function in parallel.
         * All threads share the same global counter and lock.
         */
        pthread_create(&threads[i], NULL, increment, &tids[i]);
    }

    /* Wait for all threads to finish */
    for (int i = 0; i < nthreads; i++) {
        /* pthread_join: Blocks the main thread until each thread completes */
        pthread_join(threads[i], NULL);
    }

    /* Verify result: if the spinlock works correctly, it should always equal expected */
    printf("\nExpected: %d\n", expected);
    printf("Actual:   %d\n", counter);

    if (counter == expected) {
        printf("SUCCESS! Our spinlock works correctly.\n");
    } else {
        printf("ERROR: Spinlock implementation has a bug.\n");
    }

    /* Free dynamically allocated memory */
    free(threads);
    free(tids);
    return 0;
}
