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

    /* Initialize synchronization objects: NULL uses default attributes */
    pthread_mutex_init(&b->lock, NULL);       // Initialize mutex
    pthread_cond_init(&b->not_full, NULL);    // Initialize "free space available" condition variable
    pthread_cond_init(&b->not_empty, NULL);   // Initialize "data available" condition variable
}

/* ============================================================
 * Cleanup
 *
 * Release synchronization object resources before program termination.
 * ============================================================ */
void bbuf_destroy(bbuf_t *b) {
    pthread_mutex_destroy(&b->lock);          // Release mutex resources
    pthread_cond_destroy(&b->not_full);       // Release not_full condition variable resources
    pthread_cond_destroy(&b->not_empty);      // Release not_empty condition variable resources
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
    /* Acquire mutex: enter critical section. From this point, other threads
     * will block on lock until this mutex is released. */
    pthread_mutex_lock(&b->lock);

    /* [Key point] While loop conditional wait pattern per Mesa semantics
     *
     * Why while is especially important in a multi-Producer environment:
     *   - When a Consumer sends signal, multiple Producers may wake up
     *     (technically only one wakes, but another Producer may grab the mutex first)
     *   - If the Producer that grabbed mutex first fills the empty slot,
     *     the Producer that gets mutex later must wait again
     *   - The while loop automatically performs this re-check
     *
     * pthread_cond_wait behavior (performed atomically):
     *   (1) Releases the mutex
     *   (2) Puts this thread in the not_full wait queue and sleeps
     *   (3) When signal/broadcast is received, wakes up and re-acquires the mutex */
    while (b->count == BUFFER_SIZE) {
        pthread_cond_wait(&b->not_full, &b->lock);
    }

    /* Assert to check invariant:
     * If we passed the while loop, there must be free space in the buffer.
     * If this assertion fails, it means there is a bug in the synchronization logic. */
    assert(b->count < BUFFER_SIZE);

    /* Insert data into circular buffer */
    b->data[b->in] = item;
    b->in = (b->in + 1) % BUFFER_SIZE;   // Move to next insertion position (circular)
    b->count++;                            // Increment current item count
    b->total_produced++;                   // Increment total production counter (for statistics)

    /* pthread_cond_signal: Wake one of the waiting Consumers.
     *
     * Reason for using signal here:
     *   - Since 1 item was added, waking just 1 Consumer is sufficient
     *   - Using broadcast would also work, but unnecessarily waking multiple
     *     Consumers would cause most of them to go back to sleep in the while
     *     loop, which is inefficient
     *   - signal wakes only one from the wait queue, making it more efficient */
    pthread_cond_signal(&b->not_empty);

    /* Release mutex: exit critical section */
    pthread_mutex_unlock(&b->lock);
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

    /* Acquire mutex: enter critical section */
    pthread_mutex_lock(&b->lock);

    /* [Key point] Include done flag in wait condition
     *
     * If we only check count == 0, the Consumer would wait forever
     * after all Producers have terminated.
     * By adding the !b->done condition, we exit the wait when production is done.
     *
     * Reason for using while loop (Mesa semantics):
     *   - Multiple Consumers may be waiting on an empty buffer simultaneously
     *   - Even if woken by Producer's signal, another Consumer may take the item first
     *   - Re-checking the condition with while ensures safety in this race condition */
    while (b->count == 0 && !b->done) {
        /* pthread_cond_wait: Releases mutex and sleeps in the not_empty wait queue.
         * Wakes when Producer sends signal or main sends broadcast. */
        pthread_cond_wait(&b->not_empty, &b->lock);
    }

    /* Check termination condition: if all production is done (done==1) and
     * buffer is empty (count==0), there are no more items to consume,
     * so return -1 to signal termination. */
    if (b->count == 0 && b->done) {
        pthread_mutex_unlock(&b->lock);   // Must release mutex before returning!
        return -1;
    }

    /* Assert to check invariant: if we reached this point, there must be items */
    assert(b->count > 0);

    /* Remove data from circular buffer */
    item = b->data[b->out];
    b->out = (b->out + 1) % BUFFER_SIZE;  // Move to next removal position (circular)
    b->count--;                            // Decrement current item count
    b->total_consumed++;                   // Increment total consumption counter (for statistics)

    /* pthread_cond_signal: Wake one of the waiting Producers.
     * Since 1 slot has become available, waking just 1 Producer is sufficient. */
    pthread_cond_signal(&b->not_full);

    /* Release mutex: exit critical section */
    pthread_mutex_unlock(&b->lock);

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

    /* [Key point] Termination handling: notify Consumers that all production is done
     *
     * Reason for using mutex in this process:
     *   - Setting the done flag is also shared resource access, requiring mutex protection
     *   - Consumers may be waiting in the while(count==0 && !done) loop, so
     *     setting done and broadcasting must be done atomically
     *
     * Reason for using broadcast:
     *   - Multiple Consumers may be waiting on not_empty simultaneously
     *   - Using signal would wake only one Consumer, leaving the rest waiting forever
     *   - broadcast wakes all Consumers so each can check the done flag and terminate
     *   - This is a good example demonstrating the critical difference between
     *     signal and broadcast */
    pthread_mutex_lock(&bbuf.lock);
    bbuf.done = 1;  // Indicate that there are no more items to produce
    pthread_cond_broadcast(&bbuf.not_empty);  // Wake all waiting Consumers
    pthread_mutex_unlock(&bbuf.lock);

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
