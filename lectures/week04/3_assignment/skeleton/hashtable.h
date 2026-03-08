/*
 * hashtable.h - Parallel Hash Table Definitions
 *
 * Thread-safe hash table with two locking strategies:
 *   1. Coarse-grained: single mutex for the entire table
 *   2. Fine-grained:   one mutex per bucket
 */

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <pthread.h>

/* Number of buckets in the hash table */
#define NBUCKETS 13

/* Locking strategy */
typedef enum {
    LOCK_NONE,          /* No locking (unsafe) */
    LOCK_COARSE,        /* Single global mutex */
    LOCK_FINE           /* Per-bucket mutex */
} lock_strategy_t;

/* A single key-value entry in the hash table */
struct entry {
    int key;
    int value;
    struct entry *next;  /* Linked list for chaining */
};

/* A bucket: head of a linked list of entries */
struct bucket {
    struct entry *head;
    pthread_mutex_t lock;  /* Per-bucket lock (used in fine-grained mode) */
};

/* The hash table */
struct hashtable {
    struct bucket buckets[NBUCKETS];
    pthread_mutex_t global_lock;  /* Global lock (used in coarse-grained mode) */
    lock_strategy_t strategy;
};

/*
 * Initialize the hash table with the given locking strategy.
 */
void hashtable_init(struct hashtable *ht, lock_strategy_t strategy);

/*
 * Destroy the hash table, freeing all entries and mutexes.
 */
void hashtable_destroy(struct hashtable *ht);

/*
 * Insert a key-value pair.
 * If the key already exists, update the value.
 */
void hashtable_insert(struct hashtable *ht, int key, int value);

/*
 * Look up a key.
 * Returns the value if found, or -1 if not found.
 */
int hashtable_lookup(struct hashtable *ht, int key);

/*
 * Delete a key.
 * Returns 1 if the key was found and deleted, 0 otherwise.
 */
int hashtable_delete(struct hashtable *ht, int key);

#endif /* HASHTABLE_H */
