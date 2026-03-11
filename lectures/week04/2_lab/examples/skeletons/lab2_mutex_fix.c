/*
 * mutex_fix.c - Fixing the Race Condition Using a Mutex
 *
 * [Key Concept]
 * A mutex (Mutual Exclusion) is a synchronization tool that implements
 * mutual exclusion. It prevents race conditions by ensuring that only
 * one thread can enter the critical section at a time.
 *
 * This program adds a mutex to the same scenario as race_demo.c to
 * safely protect the shared counter. The result is always correct.
 *
 * How it works:
 *   lock()   -> If another thread holds the lock, wait (blocking)
 *   counter++ -> Critical section: only one thread can execute
 *   unlock() -> Release the lock, allowing a waiting thread to enter
 *
 * Compile: gcc -Wall -pthread -o mutex_fix mutex_fix.c
 * Run:     ./mutex_fix [num_threads] [increments_per_thread]
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* Default number of threads */
#define DEFAULT_THREADS     4
/* Default number of increments each thread performs */
#define DEFAULT_INCREMENTS  1000000

/* Shared counter - protected by a mutex (volatile unnecessary: mutex guarantees memory visibility) */
int counter = 0;

/*
 * Mutex protecting the counter
 * PTHREAD_MUTEX_INITIALIZER: static initialization macro
 *   - Initializes the mutex without calling pthread_mutex_init().
 *   - Can only be used for global/static variables.
 */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* Number of increments each thread performs */
int increments_per_thread;

/*
 * Function executed by each thread
 * arg: pointer to an int containing the thread ID
 */
void *increment(void *arg)
{
    int tid = *(int *)arg;
    for (int i = 0; i < increments_per_thread; i++) {
        /*
         * TODO: Protect the critical section using the mutex.
         *
         * Steps:
         *   1. Acquire the mutex lock using pthread_mutex_lock(&lock)
         *      - If the lock is free: acquire it immediately and proceed
         *      - If another thread holds it: block (wait) until it is released
         *
         *   2. Perform the critical section operation: counter++
         *      - Only one thread can execute this at a time
         *      - The mutex protects the entire LOAD-ADD-STORE sequence
         *
         *   3. Release the mutex lock using pthread_mutex_unlock(&lock)
         *      - If other threads are waiting, one of them wakes up and acquires the lock
         *      - The lock must be released by the same thread that acquired it
         */
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

    printf("=== Mutex Fix Demo ===\n");
    printf("Threads: %d, Increments per thread: %d\n", nthreads, increments_per_thread);
    printf("Expected final counter: %d\n\n", expected);

    /* Dynamically allocate arrays for thread handles and IDs */
    pthread_t *threads = malloc(sizeof(pthread_t) * nthreads);
    int *tids = malloc(sizeof(int) * nthreads);

    /* Create threads: each thread runs the increment function */
    for (int i = 0; i < nthreads; i++) {
        tids[i] = i;
        /*
         * pthread_create: Creates a new thread to execute the increment function.
         * The created thread begins execution immediately.
         */
        pthread_create(&threads[i], NULL, increment, &tids[i]);
    }

    /* Wait for all threads to finish */
    for (int i = 0; i < nthreads; i++) {
        /*
         * pthread_join: Blocks the main thread until the specified thread completes.
         * The result can only be checked after all threads have terminated.
         */
        pthread_join(threads[i], NULL);
    }

    /* Verify result: thanks to the mutex, it should always equal expected */
    printf("\nExpected: %d\n", expected);
    printf("Actual:   %d\n", counter);

    if (counter == expected) {
        printf("SUCCESS! Mutex prevented the race condition.\n");
    } else {
        printf("ERROR: This should not happen with correct mutex usage.\n");
    }

    /*
     * pthread_mutex_destroy: Releases mutex resources.
     *   - A mutex that is no longer used must be destroyed.
     *   - Must be called only when the lock is in the unlocked state.
     *   - Recommended even for mutexes initialized with PTHREAD_MUTEX_INITIALIZER.
     */
    pthread_mutex_destroy(&lock);

    /* Free dynamically allocated memory */
    free(threads);
    free(tids);
    return 0;
}
