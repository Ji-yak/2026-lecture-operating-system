#include "uthread.h"  /* must be first -- defines _XOPEN_SOURCE */
#include <stdio.h>
#include <stdlib.h>

/*
 * Three test threads. Each prints messages and yields to demonstrate
 * cooperative multithreading with interleaved execution.
 */

void thread_a(void *arg) {
    const char *name = (const char *)arg;
    for (int i = 0; i < 5; i++) {
        printf("[%s] iteration %d\n", name, i);
        thread_yield();
    }
    printf("[%s] done\n", name);
}

void thread_b(void *arg) {
    const char *name = (const char *)arg;
    for (int i = 0; i < 5; i++) {
        printf("[%s] count = %d\n", name, i * 10);
        thread_yield();
    }
    printf("[%s] done\n", name);
}

void thread_c(void *arg) {
    const char *name = (const char *)arg;
    for (int i = 0; i < 5; i++) {
        printf("[%s] step %d of 5\n", name, i + 1);
        thread_yield();
    }
    printf("[%s] done\n", name);
}

int main(void) {
    printf("=== User-Level Thread Test ===\n\n");

    /* Initialize the threading system */
    thread_init();

    /* Create three threads */
    int t1 = thread_create(thread_a, "Alpha");
    int t2 = thread_create(thread_b, "Beta");
    int t3 = thread_create(thread_c, "Gamma");

    if (t1 < 0 || t2 < 0 || t3 < 0) {
        fprintf(stderr, "Failed to create threads\n");
        return 1;
    }

    printf("Created threads: %d, %d, %d\n\n", t1, t2, t3);

    /*
     * Enter the scheduler.  This function runs all threads to completion
     * using round-robin scheduling, then returns when no runnable threads
     * remain.
     */
    thread_schedule();

    printf("\n=== All threads finished ===\n");
    return 0;
}
