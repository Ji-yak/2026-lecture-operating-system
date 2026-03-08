/*
 * main.c - Benchmark for Parallel Hash Table
 *
 * Inserts and looks up keys in parallel using multiple threads.
 * Measures throughput for different locking strategies.
 *
 * Compile: make
 * Run:     ./hashtable_bench [none|coarse|fine] [num_threads] [num_keys]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include "hashtable.h"

#define DEFAULT_THREADS  4
#define DEFAULT_KEYS     100000

struct hashtable ht;

/* Per-thread argument */
struct thread_arg {
    int tid;
    int start_key;
    int end_key;
    int found_count;    /* number of successful lookups */
    int missing_count;  /* number of failed lookups */
};

/*
 * Get current time in seconds (with microsecond precision).
 */
static double now(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

/*
 * Insert thread: inserts keys [start_key, end_key) into the hash table.
 */
void *insert_thread(void *arg)
{
    struct thread_arg *ta = (struct thread_arg *)arg;

    for (int k = ta->start_key; k < ta->end_key; k++) {
        hashtable_insert(&ht, k, k * 10);
    }

    return NULL;
}

/*
 * Lookup thread: looks up keys [start_key, end_key) in the hash table.
 */
void *lookup_thread(void *arg)
{
    struct thread_arg *ta = (struct thread_arg *)arg;
    ta->found_count = 0;
    ta->missing_count = 0;

    for (int k = ta->start_key; k < ta->end_key; k++) {
        int val = hashtable_lookup(&ht, k);
        if (val == k * 10) {
            ta->found_count++;
        } else {
            ta->missing_count++;
        }
    }

    return NULL;
}

/*
 * Parse the locking strategy from a command-line string.
 */
static lock_strategy_t parse_strategy(const char *s)
{
    if (strcmp(s, "none") == 0)   return LOCK_NONE;
    if (strcmp(s, "coarse") == 0) return LOCK_COARSE;
    if (strcmp(s, "fine") == 0)   return LOCK_FINE;
    fprintf(stderr, "Unknown strategy: %s (use none, coarse, or fine)\n", s);
    exit(1);
}

static const char *strategy_name(lock_strategy_t s)
{
    switch (s) {
        case LOCK_NONE:   return "none (unsafe)";
        case LOCK_COARSE: return "coarse-grained";
        case LOCK_FINE:   return "fine-grained";
    }
    return "unknown";
}

int main(int argc, char *argv[])
{
    lock_strategy_t strategy = LOCK_NONE;
    int nthreads = DEFAULT_THREADS;
    int nkeys = DEFAULT_KEYS;

    if (argc > 1) strategy = parse_strategy(argv[1]);
    if (argc > 2) nthreads = atoi(argv[2]);
    if (argc > 3) nkeys = atoi(argv[3]);

    if (nthreads <= 0 || nkeys <= 0) {
        fprintf(stderr, "Usage: %s [none|coarse|fine] [threads] [keys]\n", argv[0]);
        return 1;
    }

    printf("=== Parallel Hash Table Benchmark ===\n");
    printf("Strategy: %s\n", strategy_name(strategy));
    printf("Threads:  %d\n", nthreads);
    printf("Keys:     %d\n\n", nkeys);

    hashtable_init(&ht, strategy);

    pthread_t *threads = malloc(sizeof(pthread_t) * nthreads);
    struct thread_arg *args = malloc(sizeof(struct thread_arg) * nthreads);

    /* ---- Phase: Parallel Insert ---- */

    int keys_per_thread = nkeys / nthreads;

    for (int i = 0; i < nthreads; i++) {
        args[i].tid = i;
        args[i].start_key = i * keys_per_thread;
        args[i].end_key = (i == nthreads - 1) ? nkeys : (i + 1) * keys_per_thread;
    }

    double t0 = now();
    for (int i = 0; i < nthreads; i++) {
        pthread_create(&threads[i], NULL, insert_thread, &args[i]);
    }
    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }
    double t1 = now();

    double insert_time = t1 - t0;
    printf("Insert: %.3f sec (%.0f ops/sec)\n",
           insert_time, nkeys / insert_time);

    /* ---- Phase: Parallel Lookup ---- */

    for (int i = 0; i < nthreads; i++) {
        args[i].start_key = i * keys_per_thread;
        args[i].end_key = (i == nthreads - 1) ? nkeys : (i + 1) * keys_per_thread;
        args[i].found_count = 0;
        args[i].missing_count = 0;
    }

    double t2 = now();
    for (int i = 0; i < nthreads; i++) {
        pthread_create(&threads[i], NULL, lookup_thread, &args[i]);
    }
    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }
    double t3 = now();

    double lookup_time = t3 - t2;

    int total_found = 0, total_missing = 0;
    for (int i = 0; i < nthreads; i++) {
        total_found += args[i].found_count;
        total_missing += args[i].missing_count;
    }

    printf("Lookup: %.3f sec (%.0f ops/sec)\n",
           lookup_time, nkeys / lookup_time);
    printf("  Found: %d, Missing: %d\n", total_found, total_missing);

    if (total_missing > 0) {
        printf("  WARNING: %d keys were lost! (race condition)\n", total_missing);
    } else {
        printf("  All keys found correctly.\n");
    }

    printf("\nTotal time: %.3f sec\n", insert_time + lookup_time);

    hashtable_destroy(&ht);
    free(threads);
    free(args);

    return 0;
}
