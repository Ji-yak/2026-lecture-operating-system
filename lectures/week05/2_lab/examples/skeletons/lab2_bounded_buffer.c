/*
 * lab2_bounded_buffer.c
 *
 * Bounded Buffer implementation using multiple Producer/Consumer threads
 * ================================================================
 *
 * [Overview]
 * In lab1, we covered a 1:1 Producer-Consumer structure.
 * In this example, we implement a more realistic scenario where multiple
 * Producers and Consumers simultaneously access a single shared buffer.
 *
 * [Additional considerations in a multi-threaded environment]
 * When multiple Producers/Consumers operate concurrently, the following
 * situations can occur:
 *   - Multiple Producers competing for a single empty slot
 *   - Multiple Consumers competing for a single item
 *   - A thread woken by signal may find another thread has already processed
 *     the item before it acquires the mutex
 *
 * [Mesa Semantics and the importance of the while loop]
 * Under Mesa semantics, pthread_cond_signal() is merely a "hint".
 * Between the time a thread is woken by a signal and the time it actually
 * acquires the mutex, another thread may grab the mutex first and change
 * the buffer state.
 *
 * Example: Both Consumer A and B are waiting on an empty buffer
 *   1) Producer inserts 1 item and sends signal
 *   2) Consumer A wakes up and attempts to acquire mutex
 *   3) Meanwhile, Consumer B acquires mutex first and takes the item
 *   4) When Consumer A acquires mutex, the buffer is empty again
 *   => Using if would cause an attempt to dequeue from an empty buffer, resulting in an error!
 *   => Using while re-checks the condition and waits again, ensuring safety!
 *
 * [Criteria for using signal vs broadcast]
 * - pthread_cond_signal: Wakes exactly one of the waiting threads.
 *   Used in put/get: Since one slot/item has become available, waking one is sufficient.
 *
 * - pthread_cond_broadcast: Wakes all waiting threads.
 *   Used at termination: All Consumers must be notified that "production is finished,
 *   please terminate", so broadcast is used. Using signal might leave some Consumers
 *   waiting forever.
 *
 * [Termination handling]
 * In a multi-Consumer environment, "when to terminate" is an important problem.
 * This example solves it by combining a done flag with broadcast:
 *   1) When all Producers finish, main sets done = 1
 *   2) broadcast wakes all waiting Consumers
 *   3) Consumers wake up, check the done flag, and terminate
 *
 * Compile: gcc -Wall -pthread -o lab2_bounded_buffer lab2_bounded_buffer.c
 * Run:     ./lab2_bounded_buffer
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

/* ============================================================
 * Configuration values
 *
 * BUFFER_SIZE is intentionally set small to frequently trigger situations
 * where the buffer becomes full or empty. This allows us to observe whether
 * the synchronization mechanism works correctly.
 * ============================================================ */
#define BUFFER_SIZE     4       // Intentionally small buffer (to frequently observe sync issues)
#define NUM_PRODUCERS   3       // Number of Producer threads
#define NUM_CONSUMERS   3       // Number of Consumer threads
#define ITEMS_PER_PROD  10      // Number of items each Producer will produce

/* ============================================================
 * Bounded Buffer structure
 *
 * Same circular buffer as lab1, with additional statistics fields and
 * a termination flag.
 * All fields must be protected by the lock mutex.
 * ============================================================ */
typedef struct {
    int     data[BUFFER_SIZE];  // Circular buffer array
    int     in;                 // Next insertion position
    int     out;                // Next removal position
    int     count;              // Current number of items

    pthread_mutex_t lock;       // Mutex: mutual exclusion for all fields
    pthread_cond_t  not_full;   // Condition variable: buffer has free space
    pthread_cond_t  not_empty;  // Condition variable: buffer has data

    /* Statistics and termination control */
    int     total_produced;     // Total items produced (for verification)
    int     total_consumed;     // Total items consumed (for verification)
    int     done;               // All production complete flag (1 means no more production)
} bbuf_t;

/* Global bounded buffer instance */
static bbuf_t bbuf;

/* ============================================================
 * Initialization
 *
 * Initialize the mutex, condition variables, and statistics counters.
 * ============================================================ */
void bbuf_init(bbuf_t *b) {
    b->in = 0;
    b->out = 0;
    b->count = 0;
    b->total_produced = 0;
    b->total_consumed = 0;
    b->done = 0;   // Indicates that production has not yet finished

    /* TODO: Initialize synchronization objects (NULL uses default attributes):
     *         - Initialize the mutex (b->lock) with pthread_mutex_init
     *         - Initialize the not_full condition variable with pthread_cond_init
     *         - Initialize the not_empty condition variable with pthread_cond_init
     */
}

/* ============================================================
 * Cleanup
 *
 * Release synchronization object resources before program termination.
 * ============================================================ */
void bbuf_destroy(bbuf_t *b) {
    /* TODO: Destroy all synchronization objects:
     *         - pthread_mutex_destroy(&b->lock)
     *         - pthread_cond_destroy(&b->not_full)
     *         - pthread_cond_destroy(&b->not_empty)
     */
}

/* ============================================================
 * put: Insert an item into the buffer (called by Producer)
 *
 * Operation sequence:
 *   1. Acquire mutex (pthread_mutex_lock)
 *   2. If buffer is full, wait in while loop (pthread_cond_wait)
 *   3. Insert item and update statistics
 *   4. Wake a waiting Consumer (pthread_cond_signal)
 *   5. Release mutex (pthread_mutex_unlock)
 * ============================================================ */
void bbuf_put(bbuf_t *b, int item) {
    /* TODO: Acquire the mutex (b->lock) to enter the critical section. */

    /* TODO: Wait while the buffer is full (b->count == BUFFER_SIZE).
     *
     *       Use a while loop (Mesa semantics). In a multi-Producer environment
     *       this is especially important because:
     *         - When a Consumer sends signal, multiple Producers may compete
     *         - If one Producer fills the slot first, the other must wait again
     *
     *       while (b->count == BUFFER_SIZE) {
     *           pthread_cond_wait(&b->not_full, &b->lock);
     *       }
     */

    /* Assert to check invariant:
     * If we passed the while loop, there must be free space in the buffer. */
    assert(b->count < BUFFER_SIZE);

    /* TODO: Insert data into the circular buffer and update counters:
     *         b->data[b->in] = item;
     *         b->in = (b->in + 1) % BUFFER_SIZE;
     *         b->count++;
     *         b->total_produced++;
     */

    /* TODO: Signal the not_empty condition variable to wake one waiting Consumer.
     *       Use pthread_cond_signal (not broadcast) since only 1 item was added. */

    /* TODO: Release the mutex to exit the critical section. */
}

/* ============================================================
 * get: Remove an item from the buffer (called by Consumer)
 *
 * Return value:
 *   - positive/0: item value dequeued from the buffer
 *   - -1: termination signal (all production finished and buffer is empty)
 *
 * Operation sequence:
 *   1. Acquire mutex
 *   2. If buffer is empty and production is not done, wait
 *   3. If production is done and buffer is also empty, return -1 (terminate)
 *   4. Remove item and update statistics
 *   5. Wake a waiting Producer
 *   6. Release mutex
 * ============================================================ */
int bbuf_get(bbuf_t *b) {
    int item;

    /* TODO: Acquire the mutex to enter the critical section. */

    /* TODO: Wait while the buffer is empty AND production is not done.
     *
     *       The wait condition must include the done flag so that Consumers
     *       don't wait forever after all Producers have finished:
     *
     *       while (b->count == 0 && !b->done) {
     *           pthread_cond_wait(&b->not_empty, &b->lock);
     *       }
     *
     *       Note: Consumers are woken either by a Producer's signal (new item)
     *       or by main's broadcast (production complete).
     */

    /* TODO: Check the termination condition.
     *       If production is done (b->done == 1) AND the buffer is empty
     *       (b->count == 0), there are no more items to consume.
     *       Release the mutex and return -1 to signal termination:
     *
     *       if (b->count == 0 && b->done) {
     *           pthread_mutex_unlock(&b->lock);
     *           return -1;
     *       }
     */

    /* Assert to check invariant: if we reached this point, there must be items */
    assert(b->count > 0);

    /* TODO: Remove data from the circular buffer and update counters:
     *         item = b->data[b->out];
     *         b->out = (b->out + 1) % BUFFER_SIZE;
     *         b->count--;
     *         b->total_consumed++;
     */

    /* TODO: Signal the not_full condition variable to wake one waiting Producer.
     *       Use pthread_cond_signal since only 1 slot was freed. */

    /* TODO: Release the mutex to exit the critical section. */

    return item;
}

/* ============================================================
 * Producer thread function
 *
 * Each Producer generates unique item numbers based on its ID
 * and produces ITEMS_PER_PROD items.
 * ============================================================ */
void *producer(void *arg) {
    int id = *(int *)arg;

    for (int i = 0; i < ITEMS_PER_PROD; i++) {
        int item = id * 100 + i;    // Unique item number (Producer ID * 100 + sequence)
        bbuf_put(&bbuf, item);      // Insert into buffer (synchronization handled internally)

        printf("  P%d: produced [%d] (buffer usage ~%d/%d)\n",
               id, item, bbuf.count, BUFFER_SIZE);
        /* Note: bbuf.count is read without mutex. This is for debugging output
         * purposes, so the value may not be exact, but it does not affect
         * program behavior. */

        /* Simulate production speed: wait for a random duration */
        usleep((unsigned)(rand() % 50000));
    }

    printf("  P%d: === production complete ===\n", id);
    return NULL;
}

/* ============================================================
 * Consumer thread function
 *
 * Continuously dequeues and consumes items from the buffer in an infinite loop.
 * Terminates when bbuf_get returns -1 (all production done and buffer empty).
 *
 * The exact number of items each Consumer will consume is unpredictable.
 * It varies depending on execution order and scheduling, which is a
 * characteristic of multi-Consumer systems.
 * ============================================================ */
void *consumer(void *arg) {
    int id = *(int *)arg;
    int consumed = 0;   // Number of items consumed by this Consumer

    while (1) {
        int item = bbuf_get(&bbuf);   // Dequeue item from buffer
        if (item == -1) {
            break;  // Termination signal: no more items to consume
        }

        consumed++;
        printf("  C%d: consumed [%d] (buffer usage ~%d/%d)\n",
               id, item, bbuf.count, BUFFER_SIZE);

        /* Simulate consumption speed: consumption is set slightly slower than production */
        usleep((unsigned)(rand() % 80000));
    }

    printf("  C%d: === consumption complete (total %d items) ===\n", id, consumed);
    return NULL;
}

/* ============================================================
 * main: Program entry point
 *
 * Overall flow:
 *   1. Initialize buffer
 *   2. Create N Producer threads
 *   3. Create M Consumer threads
 *   4. Wait for all Producers to finish (pthread_join)
 *   5. Set done flag + broadcast to wake all Consumers
 *   6. Wait for all Consumers to finish (pthread_join)
 *   7. Verify results and clean up resources
 * ============================================================ */
int main(void) {
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    int       prod_ids[NUM_PRODUCERS];
    int       cons_ids[NUM_CONSUMERS];

    int total_items = NUM_PRODUCERS * ITEMS_PER_PROD;

    printf("=== Bounded Buffer: Multiple Producer/Consumer ===\n");
    printf("Buffer size: %d\n", BUFFER_SIZE);
    printf("Producers %d x %d items = total %d items\n",
           NUM_PRODUCERS, ITEMS_PER_PROD, total_items);
    printf("Consumers: %d\n\n", NUM_CONSUMERS);

    srand((unsigned)time(NULL));   // Initialize random seed (time-based)
    bbuf_init(&bbuf);              // Initialize buffer and synchronization objects

    /* Create Producer threads:
     * Pass a unique ID as an argument to each thread.
     * Use the prod_ids array so each thread has its own ID address.
     * (Passing a local variable's address could cause the value to change,
     * so an array is used instead) */
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        prod_ids[i] = i;
        pthread_create(&producers[i], NULL, producer, &prod_ids[i]);
    }

    /* Create Consumer threads: same approach as Producers */
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        cons_ids[i] = i;
        pthread_create(&consumers[i], NULL, consumer, &cons_ids[i]);
    }

    /* Wait until all Producers finish:
     * pthread_join blocks until the target thread returns or calls pthread_exit. */
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }

    /* TODO: Termination handling - notify Consumers that all production is done.
     *
     *       This requires three steps, all inside the critical section:
     *         1. Acquire the mutex: pthread_mutex_lock(&bbuf.lock)
     *         2. Set the done flag: bbuf.done = 1
     *         3. Wake ALL waiting Consumers: pthread_cond_broadcast(&bbuf.not_empty)
     *            (Use broadcast, not signal! Multiple Consumers may be waiting.
     *             Using signal would wake only one, leaving others stuck forever.)
     *         4. Release the mutex: pthread_mutex_unlock(&bbuf.lock)
     */

    /* Wait until all Consumers finish */
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], NULL);
    }

    /* Verify results: the number of produced items and consumed items must match.
     * If synchronization is correctly implemented, no items should be lost
     * or consumed more than once. */
    printf("\n=== Results ===\n");
    printf("Total produced: %d\n", bbuf.total_produced);
    printf("Total consumed: %d\n", bbuf.total_consumed);

    if (bbuf.total_produced == total_items &&
        bbuf.total_consumed == total_items) {
        printf("Verification succeeded: all items were produced/consumed correctly.\n");
    } else {
        printf("Verification failed: item counts do not match!\n");
        return 1;
    }

    /* Clean up synchronization object resources */
    bbuf_destroy(&bbuf);

    printf("\n=== Program terminated normally ===\n");
    return 0;
}
