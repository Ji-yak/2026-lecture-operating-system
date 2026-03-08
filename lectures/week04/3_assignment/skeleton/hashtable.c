/*
 * hashtable.c - Parallel Hash Table Implementation
 *
 * TODO: Add locking to make this hash table thread-safe.
 *
 * Phase 1: Implement coarse-grained locking (single global mutex).
 * Phase 2: Implement fine-grained locking (per-bucket mutex).
 *
 * The hash function and basic structure are provided.
 * Look for "TODO" comments to find what you need to implement.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"

/*
 * Hash function: maps a key to a bucket index.
 * Uses a simple modulo hash. Keys are assumed to be non-negative.
 */
static int hash(int key)
{
    return key % NBUCKETS;
}

/*
 * Initialize the hash table.
 *
 * TODO: Initialize the global mutex and per-bucket mutexes.
 */
void hashtable_init(struct hashtable *ht, lock_strategy_t strategy)
{
    ht->strategy = strategy;

    for (int i = 0; i < NBUCKETS; i++) {
        ht->buckets[i].head = NULL;

        /* TODO (Phase 2): Initialize per-bucket mutex */
    }

    /* TODO (Phase 1): Initialize global mutex */
}

/*
 * Destroy the hash table: free all entries, destroy mutexes.
 */
void hashtable_destroy(struct hashtable *ht)
{
    for (int i = 0; i < NBUCKETS; i++) {
        struct entry *e = ht->buckets[i].head;
        while (e != NULL) {
            struct entry *next = e->next;
            free(e);
            e = next;
        }
        ht->buckets[i].head = NULL;

        /* TODO (Phase 2): Destroy per-bucket mutex */
    }

    /* TODO (Phase 1): Destroy global mutex */
}

/*
 * Insert a key-value pair into the hash table.
 * If the key already exists, update the value.
 *
 * TODO: Add appropriate locking based on ht->strategy.
 *
 * For LOCK_COARSE: lock the global mutex before accessing any bucket,
 *                  unlock after done.
 * For LOCK_FINE:   lock only the bucket's mutex that this key maps to,
 *                  unlock after done.
 */
void hashtable_insert(struct hashtable *ht, int key, int value)
{
    int idx = hash(key);

    /* TODO: Acquire the appropriate lock */

    /* Check if key already exists; if so, update */
    struct entry *e = ht->buckets[idx].head;
    while (e != NULL) {
        if (e->key == key) {
            e->value = value;
            /* TODO: Release the appropriate lock */
            return;
        }
        e = e->next;
    }

    /* Key not found: insert new entry at head of bucket */
    struct entry *new_entry = malloc(sizeof(struct entry));
    if (new_entry == NULL) {
        fprintf(stderr, "malloc failed\n");
        /* TODO: Release the appropriate lock */
        return;
    }
    new_entry->key = key;
    new_entry->value = value;
    new_entry->next = ht->buckets[idx].head;
    ht->buckets[idx].head = new_entry;

    /* TODO: Release the appropriate lock */
}

/*
 * Look up a key in the hash table.
 * Returns the value if found, -1 otherwise.
 *
 * TODO: Add appropriate locking based on ht->strategy.
 */
int hashtable_lookup(struct hashtable *ht, int key)
{
    int idx = hash(key);

    /* TODO: Acquire the appropriate lock */

    struct entry *e = ht->buckets[idx].head;
    while (e != NULL) {
        if (e->key == key) {
            int val = e->value;
            /* TODO: Release the appropriate lock */
            return val;
        }
        e = e->next;
    }

    /* TODO: Release the appropriate lock */
    return -1;
}

/*
 * Delete a key from the hash table.
 * Returns 1 if found and deleted, 0 otherwise.
 *
 * TODO: Add appropriate locking based on ht->strategy.
 */
int hashtable_delete(struct hashtable *ht, int key)
{
    int idx = hash(key);

    /* TODO: Acquire the appropriate lock */

    struct entry **pp = &ht->buckets[idx].head;
    while (*pp != NULL) {
        if ((*pp)->key == key) {
            struct entry *victim = *pp;
            *pp = victim->next;
            free(victim);
            /* TODO: Release the appropriate lock */
            return 1;
        }
        pp = &(*pp)->next;
    }

    /* TODO: Release the appropriate lock */
    return 0;
}
