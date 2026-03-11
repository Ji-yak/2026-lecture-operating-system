/*
 * deadlock_demo.c - Deadlock Demonstration Program
 *
 * [Key Concept]
 * A deadlock is a state where two or more threads wait forever for
 * resources held by each other, making no progress.
 *
 * The 4 necessary conditions for deadlock (Coffman conditions):
 *   1. Mutual Exclusion: A resource can be used by only one thread at a time
 *   2. Hold and Wait: A thread holds resources while waiting for others
 *   3. No Preemption: Resources cannot be forcibly taken from a thread
 *   4. Circular Wait: A circular chain of threads waiting for each other's resources
 *
 * Circular wait in this program:
 *   Thread A: holds lock1 -> waits for lock2
 *   Thread B: holds lock2 -> waits for lock1
 *   -> Each waits for the other's lock forever, unable to make progress!
 *
 * Solution: Enforce a consistent lock ordering.
 *   If all threads always acquire lock1 -> lock2 in the same order,
 *   deadlock cannot occur.
 *   This is called the "Lock Ordering Discipline".
 *
 * Compile: gcc -Wall -pthread -o deadlock_demo deadlock_demo.c
 * Run:     ./deadlock_demo
 *
 * Note: If the program hangs (deadlock occurred), terminate with Ctrl+C.
 *       Deadlock occurs probabilistically; if it doesn't happen, run again.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/*
 * Two mutexes declared globally
 * Statically initialized with PTHREAD_MUTEX_INITIALIZER
 */
pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;

/*
 * Thread A's execution function
 * Lock acquisition order: lock1 -> lock2
 */
void *thread_a(void *arg)
{
    /* Suppress unused parameter warning */
    (void)arg;

    /*
     * TODO: Implement Thread A's lock acquisition sequence to cause deadlock.
     *
     * Steps:
     *   1. Print: "[Thread A] Trying to acquire lock1...\n"
     *   2. Acquire lock1 using pthread_mutex_lock(&lock1)
     *   3. Print: "[Thread A] Acquired lock1.\n"
     *   4. Sleep for 100ms using usleep(100000) to increase deadlock probability
     *      (This gives Thread B time to acquire lock2)
     *   5. Print: "[Thread A] Trying to acquire lock2...\n"
     *   6. Acquire lock2 using pthread_mutex_lock(&lock2)
     *      (If Thread B holds lock2, Thread A blocks here forever)
     *   7. Print: "[Thread A] Acquired lock2.\n"
     */

    /* Critical section: safely perform work while holding both locks */
    printf("[Thread A] In critical section with both locks.\n");

    /*
     * Release locks: it is common practice to release in reverse order of acquisition
     * (Reverse order is not mandatory, but recommended for readability and consistency)
     */
    pthread_mutex_unlock(&lock2);
    pthread_mutex_unlock(&lock1);
    printf("[Thread A] Released both locks.\n");

    return NULL;
}

/*
 * Thread B's execution function
 * Lock acquisition order: lock2 -> lock1 (opposite order from Thread A!)
 *
 * Because locks are acquired in the opposite order from Thread A,
 * a circular wait is formed, causing deadlock.
 */
void *thread_b(void *arg)
{
    (void)arg;

    /*
     * TODO: Implement Thread B's lock acquisition sequence to cause deadlock.
     *
     * Steps (note the OPPOSITE order from Thread A):
     *   1. Print: "[Thread B] Trying to acquire lock2...\n"
     *   2. Acquire lock2 using pthread_mutex_lock(&lock2)
     *   3. Print: "[Thread B] Acquired lock2.\n"
     *   4. Sleep for 100ms using usleep(100000) to increase deadlock probability
     *   5. Print: "[Thread B] Trying to acquire lock1...\n"
     *   6. Acquire lock1 using pthread_mutex_lock(&lock1)
     *      (If Thread A holds lock1, Thread B blocks here forever)
     *   7. Print: "[Thread B] Acquired lock1.\n"
     *
     * The deadlock state at this point:
     *   Thread A: holds lock1, waiting for lock2 (blocked)
     *   Thread B: holds lock2, waiting for lock1 (blocked)
     *   -> Circular wait formed -> Deadlock!
     */

    /* Critical section */
    printf("[Thread B] In critical section with both locks.\n");

    /* Release locks */
    pthread_mutex_unlock(&lock1);
    pthread_mutex_unlock(&lock2);
    printf("[Thread B] Released both locks.\n");

    return NULL;
}

int main(void)
{
    printf("=== Deadlock Demo ===\n");
    printf("Thread A: lock1 -> lock2\n");
    printf("Thread B: lock2 -> lock1\n");
    printf("If deadlock occurs, the program will hang. Use Ctrl+C to exit.\n\n");

    pthread_t ta, tb;

    /*
     * pthread_create: Creates two threads.
     * Since both threads start nearly simultaneously, each acquires one lock
     * and then waits for the other's lock, creating the deadlock scenario.
     */
    pthread_create(&ta, NULL, thread_a, NULL);
    pthread_create(&tb, NULL, thread_b, NULL);

    /*
     * pthread_join: Wait for both threads to terminate.
     * If deadlock occurs, the threads never terminate,
     * so the main thread will also block here forever.
     */
    pthread_join(ta, NULL);
    pthread_join(tb, NULL);

    /* If this line is printed, deadlock did not occur */
    printf("\nNo deadlock occurred this time!\n");
    printf("Tip: Run multiple times to observe the deadlock.\n");

    /*
     * pthread_mutex_destroy: Releases mutex resources.
     * If deadlock occurs, this code is never reached.
     */
    pthread_mutex_destroy(&lock1);
    pthread_mutex_destroy(&lock2);
    return 0;
}
