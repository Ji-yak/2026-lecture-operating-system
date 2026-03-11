/*
 * race_demo.c - Race Condition Demonstration Program
 *
 * [Key Concept]
 * A race condition is a phenomenon where the result varies depending on
 * the execution order when two or more threads access a shared resource
 * simultaneously.
 *
 * In this program, multiple threads increment a shared counter without
 * any synchronization. Since the counter++ operation is not atomic,
 * the final result will be less than the expected value.
 *
 * counter++ is actually executed in the following 3 steps:
 *   1. Read the counter value from memory into a register (LOAD)
 *   2. Add 1 to the register value (ADD)
 *   3. Store the register value back to memory (STORE)
 * If another thread intervenes between these 3 steps, the increment is lost.
 *
 * Compile: gcc -Wall -pthread -o race_demo race_demo.c
 * Run:     ./race_demo [num_threads] [increments_per_thread]
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* Default number of threads */
#define DEFAULT_THREADS     4
/* Default number of increments each thread performs */
#define DEFAULT_INCREMENTS  1000000

/*
 * Shared counter - not protected by any synchronization mechanism
 * The volatile keyword prevents the compiler from optimizing away
 * memory accesses, but does not guarantee atomicity.
 */
volatile int counter = 0;

/* Number of increments each thread performs (shared by all threads) */
int increments_per_thread;

/*
 * Function executed by each thread
 * arg: pointer to an int containing the thread ID
 */
void *increment(void *arg)
{
    /* Cast the void pointer to an int pointer and dereference to get the thread ID */
    int tid = *(int *)arg;

    /*
     * TODO: Write a for loop that increments the shared counter
     *       increments_per_thread times.
     *
     * Hint:
     *   - Loop from i = 0 to increments_per_thread
     *   - Inside the loop, do counter++
     *   - Note: counter++ is NOT atomic. This is the source of the race condition.
     *     At the assembly level, it roughly translates to:
     *       1. LOAD  counter -> register   (read value from memory)
     *       2. ADD   1       -> register   (register value +1)
     *       3. STORE register -> counter   (write value to memory)
     */

    printf("[Thread %d] finished %d increments\n", tid, increments_per_thread);
    return NULL;
}

int main(int argc, char *argv[])
{
    /* Set thread count and increment count from command-line arguments (use defaults if absent) */
    int nthreads = (argc > 1) ? atoi(argv[1]) : DEFAULT_THREADS;
    increments_per_thread = (argc > 2) ? atoi(argv[2]) : DEFAULT_INCREMENTS;

    /* Validate input values */
    if (nthreads <= 0 || increments_per_thread <= 0) {
        fprintf(stderr, "Usage: %s [num_threads] [increments_per_thread]\n", argv[0]);
        return 1;
    }

    /* Expected result = number of threads x increments per thread */
    int expected = nthreads * increments_per_thread;

    printf("=== Race Condition Demo ===\n");
    printf("Threads: %d, Increments per thread: %d\n", nthreads, increments_per_thread);
    printf("Expected final counter: %d\n\n", expected);

    /* Dynamically allocate arrays for thread handles and IDs */
    pthread_t *threads = malloc(sizeof(pthread_t) * nthreads);
    int *tids = malloc(sizeof(int) * nthreads);

    /* Thread creation loop */
    for (int i = 0; i < nthreads; i++) {
        tids[i] = i;
        /*
         * pthread_create: Creates a new thread.
         *   - &threads[i]: location to store the handle of the created thread
         *   - NULL: use default thread attributes
         *   - increment: function the thread will execute
         *   - &tids[i]: argument to pass to the thread function (thread ID)
         */
        pthread_create(&threads[i], NULL, increment, &tids[i]);
    }

    /* Wait for all threads to finish */
    for (int i = 0; i < nthreads; i++) {
        /*
         * pthread_join: Blocks the main thread until the specified thread terminates.
         *   - threads[i]: handle of the thread to wait for
         *   - NULL: do not receive the thread's return value
         */
        pthread_join(threads[i], NULL);
    }

    /* Compare results: verify that actual < expected due to the race condition */
    printf("\nExpected: %d\n", expected);
    printf("Actual:   %d\n", counter);

    if (counter != expected) {
        /* Print the number of increments lost due to the race condition */
        printf("MISMATCH! Lost %d increments due to race condition.\n",
               expected - counter);
    } else {
        printf("No race detected this run. Try increasing increments or threads.\n");
    }

    /* Free dynamically allocated memory */
    free(threads);
    free(tids);
    return 0;
}
