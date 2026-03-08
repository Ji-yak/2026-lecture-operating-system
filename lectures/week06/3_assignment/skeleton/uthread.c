#include "uthread.h"  /* must be first -- defines _XOPEN_SOURCE */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global thread table */
struct thread threads[MAX_THREADS];

/* Index of the currently running thread */
int current_thread = 0;

/* ---------- Internal helpers ---------- */

/*
 * The scheduler's own context.  When a thread yields or exits, it
 * swaps back to this context.  The scheduler loop then picks the
 * next thread to run.
 */
static ucontext_t _sched_context;

/*
 * Per-thread startup info.
 * makecontext only supports int arguments, so we store the real
 * func and arg in these tables, indexed by thread slot.
 */
static void (*_thread_funcs[MAX_THREADS])(void *);
static void  *_thread_args[MAX_THREADS];

/*
 * _thread_wrapper: entry point for every new thread.
 *
 * This is the function passed to makecontext.  It looks up the real
 * user function and argument, calls it, then calls thread_exit() to
 * clean up when the function returns.
 */
static void _thread_wrapper(int idx) {
    _thread_funcs[idx](_thread_args[idx]);
    thread_exit();
    /* NOT REACHED */
}

/* ---------- Public API ---------- */

void thread_init(void) {
    memset(threads, 0, sizeof(threads));
    memset(_thread_funcs, 0, sizeof(_thread_funcs));
    memset(_thread_args, 0, sizeof(_thread_args));

    /* Thread 0 is the main/calling thread -- already running. */
    threads[0].state = T_RUNNING;
    snprintf(threads[0].name, sizeof(threads[0].name), "main");
    current_thread = 0;
}

int thread_create(void (*func)(void *), void *arg) {
    int i;

    /* Find a free slot */
    for (i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == T_FREE)
            break;
    }
    if (i == MAX_THREADS) {
        fprintf(stderr, "thread_create: no free thread slots\n");
        return -1;
    }

    struct thread *t = &threads[i];
    t->state = T_RUNNABLE;
    snprintf(t->name, sizeof(t->name), "thread_%d", i);

    /* Save the function and argument for the wrapper */
    _thread_funcs[i] = func;
    _thread_args[i] = arg;

    /*
     * TODO: Set up the thread's ucontext so that when we swap to it,
     *       execution starts at _thread_wrapper(i) on the thread's own stack.
     *
     * Steps:
     *   1. Call getcontext(&t->context) to initialize the context struct
     *      with the current execution state.
     *
     *   2. Set t->context.uc_stack.ss_sp to the thread's stack buffer
     *      (t->stack) and ss_size to STACK_SIZE.
     *
     *   3. Set t->context.uc_link to &_sched_context so that if
     *      _thread_wrapper returns (without thread_exit), control
     *      transfers back to the scheduler.
     *
     *   4. Call makecontext(&t->context, (void(*)(void))_thread_wrapper, 1, i)
     *      to set the entry point and its argument.
     *
     * Reference: man getcontext, man makecontext
     */

    /* ====== YOUR CODE HERE ====== */


    /* ====== END YOUR CODE ====== */

    return i;
}

void thread_yield(void) {
    /*
     * TODO: Voluntarily give up the CPU and let the scheduler run
     *       the next thread.
     *
     * Steps:
     *   1. Set this thread's state to T_RUNNABLE (it's ready to run
     *      again, just not right now).
     *
     *   2. Call swapcontext(&threads[current_thread].context, &_sched_context)
     *      to save our state and switch to the scheduler.
     *
     *   When the scheduler eventually swaps back to us, swapcontext
     *   returns and we resume right where we left off.
     */

    /* ====== YOUR CODE HERE ====== */


    /* ====== END YOUR CODE ====== */
}

void thread_schedule(void) {
    /*
     * TODO: The scheduler loop.  Repeatedly find the next RUNNABLE
     *       thread (round-robin) and dispatch it.
     *
     * Algorithm:
     *   int last = current_thread;
     *   while (1) {
     *       // Scan from (last+1) to (last+MAX_THREADS), wrapping around
     *       // Find a thread with state == T_RUNNABLE
     *       // If found:
     *       //   a. Set its state to T_RUNNING
     *       //   b. Update current_thread
     *       //   c. swapcontext(&_sched_context, &threads[idx].context)
     *       //      This dispatches the thread.  When the thread yields
     *       //      or exits, swapcontext returns here.
     *       //   d. Update 'last' to current_thread for round-robin
     *       //   e. Break inner loop to restart scan
     *       // If not found: break (no more runnable threads)
     *   }
     *   // Restore main thread as current
     *   current_thread = 0;
     *   threads[0].state = T_RUNNING;
     */

    /* ====== YOUR CODE HERE ====== */


    /* ====== END YOUR CODE ====== */
}

void thread_exit(void) {
    threads[current_thread].state = T_EXITED;
    printf("[uthread] thread %d (%s) exited\n",
           current_thread, threads[current_thread].name);

    /*
     * TODO: Transfer control back to the scheduler.
     *
     * Since this thread is finished, we don't need to save its context.
     * Use setcontext(&_sched_context) to jump directly to the scheduler
     * without saving (setcontext never returns).
     *
     * Alternative: swapcontext also works, but setcontext is more
     * appropriate since the thread is dead.
     */

    /* ====== YOUR CODE HERE ====== */


    /* ====== END YOUR CODE ====== */
}
