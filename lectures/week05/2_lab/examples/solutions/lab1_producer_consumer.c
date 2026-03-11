/*
 * lab1_producer_consumer.c
 *
 * Producer-Consumer Pattern
 * ============================================
 *
 * [Overview]
 * Producer-Consumer is one of the most classic synchronization problems in operating systems.
 * The Producer creates data and places it into a shared buffer,
 * and the Consumer retrieves data from the buffer and uses it.
 *
 * [Bounded Buffer]
 * Because the buffer size is finite, synchronization is needed in two situations:
 *   (1) When the buffer is full - the Producer must wait until space becomes available
 *   (2) When the buffer is empty - the Consumer must wait until data arrives
 *
 * [Synchronization Tools Used]
 *   - pthread_mutex_t : Ensures mutual exclusion so that only one thread
 *                       can access the shared resource at a time.
 *   - pthread_cond_t  : A condition variable that puts a thread to sleep (wait)
 *                       until a specific condition is met, then wakes it up (signal).
 *
 * [Mesa Semantics]
 * pthread condition variables follow Mesa semantics.
 * In Mesa semantics, a thread woken by a signal does not execute immediately;
 * it must re-acquire the mutex before resuming execution. In the meantime,
 * another thread may acquire the mutex first and change the condition.
 *
 * Therefore, you must always use a while loop when checking conditions:
 *   - if(condition) wait  -> Incorrect! (Only safe under Hoare semantics)
 *   - while(condition) wait -> Correct! (Safe under Mesa semantics)
 *
 * Using while ensures the condition is re-checked after waking up,
 * so the code works safely even if another thread changed the condition first.
 * It also guards against "spurious wakeups" where the OS or library
 * wakes a thread for no reason.
 *
 * [signal vs broadcast]
 *   - pthread_cond_signal   : Wakes only one of the waiting threads.
 *                             Efficient in 1:1 relationships.
 *   - pthread_cond_broadcast : Wakes all waiting threads.
 *                              Used when multiple threads may be waiting.
 *   In this example, there is 1 Producer and 1 Consumer, so signal is sufficient.
 *
 * Compile: gcc -Wall -pthread -o lab1_producer_consumer lab1_producer_consumer.c
 * Run:     ./lab1_producer_consumer
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/* ============================================================
 * Bounded Buffer Definition
 *
 * Implemented as a circular buffer.
 * The in and out indices wrap around using modulo BUFFER_SIZE,
 * so when they reach the end of the array, they loop back to the beginning.
 * ============================================================ */
#define BUFFER_SIZE 5       // Buffer size (number of slots)
#define NUM_ITEMS   20      // Total number of items to produce/consume

typedef struct {
    int     data[BUFFER_SIZE];  // Circular buffer array (actual data storage)
    int     in;                 // Next insertion position (Producer inserts here)
    int     out;                // Next removal position (Consumer removes from here)
    int     count;              // Current number of items in the buffer

    pthread_mutex_t mutex;      // Mutex: ensures mutual exclusion for buffer access
    pthread_cond_t  not_full;   // Condition variable: represents "buffer is not full"
    pthread_cond_t  not_empty;  // Condition variable: represents "buffer is not empty"
} bounded_buffer_t;

/* Global bounded buffer instance */
bounded_buffer_t buffer;

/* ============================================================
 * Buffer Initialization
 *
 * Initializes the mutex and condition variables.
 * When the second argument to pthread_mutex_init/pthread_cond_init is NULL,
 * default attributes are used.
 * ============================================================ */
void buffer_init(bounded_buffer_t *buf) {
    buf->in    = 0;
    buf->out   = 0;
    buf->count = 0;

    /* Initialize mutex: create a lock to protect shared resources */
    pthread_mutex_init(&buf->mutex, NULL);

    /* Initialize condition variables: create wait queues for "not full" and "not empty" */
    pthread_cond_init(&buf->not_full, NULL);
    pthread_cond_init(&buf->not_empty, NULL);
}

/* ============================================================
 * Buffer Insertion (Called by Producer)
 *
 * Operation sequence:
 *   1. pthread_mutex_lock   - Acquire mutex (enter critical section)
 *   2. while + cond_wait    - Wait if the buffer is full
 *   3. Insert data          - Add item to circular buffer
 *   4. pthread_cond_signal  - Wake up waiting Consumer
 *   5. pthread_mutex_unlock - Release mutex (exit critical section)
 * ============================================================ */
void buffer_put(bounded_buffer_t *buf, int item) {
    /* Acquire mutex: from this point, no other thread can hold this mutex.
     * Shared resources (buf->count, buf->data, etc.) can be accessed safely. */
    pthread_mutex_lock(&buf->mutex);

    /* [Key Point] While loop pattern following Mesa semantics
     *
     * Why while instead of if?
     *   - Waking up from pthread_cond_wait() does not guarantee the condition is true.
     *   - Between waking up and re-acquiring the mutex, another thread may run first
     *     and make the condition false again. (This is the essence of Mesa semantics)
     *   - A "spurious wakeup" may also occur.
     *   - The while loop re-checks the condition after waking, making it safe in all cases.
     */
    while (buf->count == BUFFER_SIZE) {
        printf("  [Producer] Buffer full, waiting...\n");
        /* pthread_cond_wait operation (performed atomically):
         *   (1) Releases the mutex (allowing other threads to enter the critical section)
         *   (2) Places this thread in the not_full condition variable's wait queue and sleeps
         *   (3) Upon receiving signal/broadcast, wakes up and re-acquires the mutex */
        pthread_cond_wait(&buf->not_full, &buf->mutex);
        /* At this point, the mutex has been re-acquired.
         * The while loop condition is re-checked to verify space is truly available. */
    }

    /* Insert data into circular buffer:
     * Place data at the in position, then advance in to the next position.
     * The % BUFFER_SIZE operation wraps around from the end to the beginning. */
    buf->data[buf->in] = item;
    buf->in = (buf->in + 1) % BUFFER_SIZE;
    buf->count++;

    printf("  [Producer] Produced: %d (buffer: %d/%d)\n",
           item, buf->count, BUFFER_SIZE);

    /* pthread_cond_signal: Wake up one of the waiting Consumer threads.
     *
     * A Consumer may be waiting on the not_empty condition variable.
     * Since an item has been added, the "buffer is not empty" condition is now true,
     * so we wake a waiting Consumer to let it consume.
     *
     * signal wakes only one thread, so in this example with a single Consumer,
     * signal is sufficient instead of broadcast. */
    pthread_cond_signal(&buf->not_empty);

    /* Release mutex: leave the critical section so other threads can access it. */
    pthread_mutex_unlock(&buf->mutex);
}

/* ============================================================
 * Remove from Buffer (Called by Consumer)
 *
 * Operation sequence:
 *   1. pthread_mutex_lock   - Acquire mutex (enter critical section)
 *   2. while + cond_wait    - Wait if the buffer is empty
 *   3. Remove data          - Retrieve item from circular buffer
 *   4. pthread_cond_signal  - Wake up waiting Producer
 *   5. pthread_mutex_unlock - Release mutex (exit critical section)
 * ============================================================ */
int buffer_get(bounded_buffer_t *buf) {
    int item;

    /* Acquire mutex: lock to safely access shared resources */
    pthread_mutex_lock(&buf->mutex);

    /* [Key Point] While loop pattern following Mesa semantics
     *
     * Wait until data arrives if the buffer is empty.
     * The reason for using while instead of if is the same as in buffer_put():
     *   - Another Consumer may have taken the data first after waking up
     *   - A spurious wakeup may occur
     *   - Re-checking with while ensures safety in all these situations */
    while (buf->count == 0) {
        printf("  [Consumer] Buffer empty, waiting...\n");
        /* pthread_cond_wait: releases mutex and sleeps on the not_empty wait queue.
         * Wakes up and re-acquires mutex when Producer sends a signal. */
        pthread_cond_wait(&buf->not_empty, &buf->mutex);
    }

    /* Remove data from circular buffer:
     * Retrieve data from the out position, then advance out to the next position. */
    item = buf->data[buf->out];
    buf->out = (buf->out + 1) % BUFFER_SIZE;
    buf->count--;

    printf("  [Consumer] Consumed: %d (buffer: %d/%d)\n",
           item, buf->count, BUFFER_SIZE);

    /* pthread_cond_signal: Wake up one of the waiting Producer threads.
     *
     * A Producer may be waiting on the not_full condition variable.
     * Since an item was removed and space is now available, the "buffer is not full"
     * condition is now true, so we wake a waiting Producer to let it produce. */
    pthread_cond_signal(&buf->not_full);

    /* Release mutex: leave the critical section */
    pthread_mutex_unlock(&buf->mutex);

    return item;
}

/* ============================================================
 * Buffer Cleanup
 *
 * Releases resources for the mutex and condition variables after use.
 * pthread_*_destroy cleans up resources for initialized synchronization objects.
 * ============================================================ */
void buffer_destroy(bounded_buffer_t *buf) {
    pthread_mutex_destroy(&buf->mutex);       // Release mutex resources
    pthread_cond_destroy(&buf->not_full);     // Release not_full condition variable resources
    pthread_cond_destroy(&buf->not_empty);    // Release not_empty condition variable resources
}

/* ============================================================
 * Producer Thread Function
 *
 * Generates NUM_ITEMS items and places them into the buffer.
 * After producing each item, waits a random amount of time to simulate real work.
 * ============================================================ */
void *producer(void *arg) {
    int id = *(int *)arg;

    for (int i = 0; i < NUM_ITEMS; i++) {
        int item = id * 1000 + i;   // Generate unique item number (Producer ID * 1000 + sequence)
        buffer_put(&buffer, item);  // Insert into buffer (synchronization handled internally)
        usleep(rand() % 100000);    // Random delay 0~100ms (simulates production time)
    }

    printf("  [Producer %d] Production complete\n", id);
    return NULL;
}

/* ============================================================
 * Consumer Thread Function
 *
 * Retrieves and consumes NUM_ITEMS items from the buffer.
 * After consuming each item, waits a random amount of time to simulate processing.
 * ============================================================ */
void *consumer(void *arg) {
    int id = *(int *)arg;

    for (int i = 0; i < NUM_ITEMS; i++) {
        int item = buffer_get(&buffer);  // Retrieve item from buffer (synchronization handled internally)
        (void)item;                      // Suppress unused variable warning (prevent compiler warning)
        usleep(rand() % 150000);         // Random delay 0~150ms (simulates consumption time)
    }

    printf("  [Consumer %d] Consumption complete\n", id);
    return NULL;
}

/* ============================================================
 * main: Program Entry Point
 *
 * 1. Initialize the buffer
 * 2. Create one Producer and one Consumer thread
 * 3. Wait for both threads to finish (pthread_join)
 * 4. Clean up resources and exit
 * ============================================================ */
int main(void) {
    pthread_t prod_thread, cons_thread;
    int prod_id = 1, cons_id = 1;

    printf("=== Producer-Consumer Problem ===\n");
    printf("Buffer size: %d, Items to produce/consume: %d\n\n", BUFFER_SIZE, NUM_ITEMS);

    /* Initialize buffer: create mutex and condition variables */
    buffer_init(&buffer);

    /* pthread_create: Creates a new thread.
     * Arguments: (thread handle, attributes (NULL=default), thread function, argument to pass)
     * Producer and Consumer run concurrently and share the buffer. */
    pthread_create(&prod_thread, NULL, producer, &prod_id);
    pthread_create(&cons_thread, NULL, consumer, &cons_id);

    /* pthread_join: Main thread waits until the specified thread terminates.
     * Without join, main may finish first and the process could terminate. */
    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, NULL);

    /* Clean up synchronization object resources */
    buffer_destroy(&buffer);

    printf("\n=== Program terminated normally ===\n");
    return 0;
}
