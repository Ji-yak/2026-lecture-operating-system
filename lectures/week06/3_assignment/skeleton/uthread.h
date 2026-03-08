/*
 * IMPORTANT: This header must be included BEFORE any standard library
 * headers (stdio.h, stdlib.h, etc.) because it defines _XOPEN_SOURCE
 * which must be set before any system header is processed.
 */
#ifndef UTHREAD_H
#define UTHREAD_H

/* Required for ucontext on both macOS and Linux */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

/*
 * Suppress macOS deprecation warnings for ucontext.
 * We do NOT use push/pop here because the calling .c files also
 * need the suppression when they call getcontext/swapcontext/etc.
 */
#ifdef __APPLE__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <ucontext.h>

/* Maximum number of threads */
#define MAX_THREADS 8

/* Stack size for each thread (64 KB) */
#define STACK_SIZE (1 << 16)

/* Thread states */
enum thread_state {
    T_FREE = 0,   /* thread slot is available */
    T_RUNNING,     /* thread is currently running */
    T_RUNNABLE,    /* thread is ready to run */
    T_EXITED       /* thread has finished execution */
};

/* Thread structure */
struct thread {
    enum thread_state state;       /* current state of the thread */
    char             stack[STACK_SIZE]; /* thread's private stack */
    ucontext_t       context;      /* saved execution context */
    char             name[16];     /* thread name for debugging */
};

/* Global thread table */
extern struct thread threads[MAX_THREADS];

/* Index of the currently running thread */
extern int current_thread;

/*
 * thread_init() - Initialize the threading system.
 *
 * Must be called once before any other thread functions.
 * Registers the calling context as thread 0 (the "main" thread).
 */
void thread_init(void);

/*
 * thread_create(func, arg) - Create a new user-level thread.
 *
 * @func: The function the thread will execute. Signature: void func(void *arg)
 * @arg:  Argument passed to func.
 *
 * Returns the thread index on success, or -1 if no free slot.
 */
int thread_create(void (*func)(void *), void *arg);

/*
 * thread_yield() - Voluntarily give up the CPU.
 *
 * Saves the current thread's context and switches to the next
 * runnable thread via thread_schedule().
 */
void thread_yield(void);

/*
 * thread_schedule() - Pick the next runnable thread and switch to it.
 *
 * Uses round-robin scheduling starting from the thread after
 * the current one.
 */
void thread_schedule(void);

/*
 * thread_exit() - Terminate the current thread.
 *
 * Marks the current thread as exited and switches to the next thread.
 * Does not return.
 */
void thread_exit(void);

#endif /* UTHREAD_H */
