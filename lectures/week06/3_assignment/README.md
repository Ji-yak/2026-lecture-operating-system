# Week 6 Assignment: User-Level Thread Library (uthread)

> Deadline: Before the next class starts
> Difficulty: ★★★☆☆
> Type: Required assignment

## Overview

In this assignment, you will implement a **user-level thread library**. Just as xv6 kernel's `swtch`/`scheduler` switches processes at the kernel level, you will implement cooperative thread switching in user space.

Inspired by MIT 6.S081's "Multithreading" Lab, you will create a thread library that runs in a **macOS/Linux** environment, not in xv6.

## Learning Objectives

- Understand the core principles of context switching by implementing it yourself.
- Learn context management using the `ucontext` API (`getcontext`, `makecontext`, `swapcontext`, `setcontext`).
- Express process/thread state transitions in code.
- Implement round-robin scheduling yourself.

## Background Knowledge

### What is Context Switching?

It is the process where the CPU saves the **register state** (PC, SP, general-purpose registers, etc.) of the currently running thread and restores the saved state of the next thread to be executed. In xv6, `swtch.S` performs this role, and in this assignment, the POSIX `ucontext` API serves the same purpose.

### ucontext API

```c
#include <ucontext.h>

int  getcontext(ucontext_t *ucp);         // Save current context
void makecontext(ucontext_t *ucp,         // Set the starting function for a context
                 void (*func)(), int argc, ...);
int  swapcontext(ucontext_t *oucp,        // Save current context + switch to new context
                 const ucontext_t *ucp);
int  setcontext(const ucontext_t *ucp);   // Switch to new context (without saving)
```

**Key Function Descriptions:**

| Function | Role | xv6 Equivalent |
|------|------|----------|
| `getcontext` | Save the current execution state to `ucontext_t` | The `sd` (save) part of `swtch` |
| `makecontext` | Set the entry point (function) and stack for a context | Setting `context.ra = forkret` in `allocproc` |
| `swapcontext` | Save current state + switch to another context | `swtch(old, new)` |
| `setcontext` | Switch to another context (without saving) | Jump to scheduler after process exit |

### Thread State Transitions

```
            thread_create
  T_FREE ─────────────────> T_RUNNABLE
                              │    ▲
                   scheduler  │    │  thread_yield
                   dispatch   │    │
                              ▼    │
                            T_RUNNING
                              │
                   thread_exit│
                              ▼
                            T_EXITED
```

## File Structure

```
3_assignment/
├── README.md           ← This file
├── skeleton/
│   ├── uthread.h       ← Thread struct, constant definitions (no modification needed)
│   ├── uthread.c       ← Implement the TODO sections
│   └── main.c          ← Test program (no modification needed)
├── solution/
│   └── uthread_solution.c  ← Solution (available after assignment submission)
└── tests/
    └── test_uthread.sh ← Automated grading script
```

## Implementation Requirements

Implement the `TODO` sections in `skeleton/uthread.c`. There are 4 functions to implement:

### 1. `thread_create()` -- Context Initialization

```c
int thread_create(void (*func)(void *), void *arg)
```

Creates a new thread. Internally:
- Find an empty slot and set the state to `T_RUNNABLE` (already implemented)
- **TODO**: Initialize context with `getcontext`
- **TODO**: Set the thread's dedicated stack (`t->stack`, size `STACK_SIZE`) in the context
- **TODO**: Set `uc_link` to `_sched_context` (scheduler context)
- **TODO**: Use `makecontext` to set the starting function to `_thread_wrapper(i)`

**Hint**: Similar to how xv6's `allocproc()` sets `context.ra = forkret` and `context.sp = kstack + PGSIZE`.

### 2. `thread_yield()` -- Yield CPU

```c
void thread_yield(void)
```

The current thread voluntarily yields the CPU:
- **TODO**: Change state to `T_RUNNABLE`
- **TODO**: Use `swapcontext` to save the current state and switch to the scheduler

**Hint**: This is the same pattern as xv6's `yield()` which changes `state = RUNNABLE` and calls `sched()` -> `swtch()`.

### 3. `thread_schedule()` -- Round-Robin Scheduler

```c
void thread_schedule(void)
```

Selects the next thread to run and switches to it:
- **TODO**: Starting from `(current_thread + 1)`, cycle through to find a `T_RUNNABLE` thread
- **TODO**: If found, change to `T_RUNNING` and switch with `swapcontext`
- **TODO**: When a thread returns after yield/exit, find the next thread again
- **TODO**: If no runnable threads exist, exit the loop and return

**Hint**: The logic is almost identical to xv6's `scheduler()` function. Use the `threads[]` array instead of the `proc[]` array, and `swapcontext()` instead of `swtch()`.

### 4. `thread_exit()` -- Thread Termination

```c
void thread_exit(void)
```

Terminates the current thread and returns to the scheduler:
- Change state to `T_EXITED` (already implemented)
- **TODO**: Use `setcontext` to jump to the scheduler context (no need to save current state)

**Hint**: Similar to how xv6's `exit()` changes the state to `ZOMBIE` and calls `sched()`.

## Build and Run

```bash
cd skeleton/
gcc -Wall -o uthread uthread.c main.c
./uthread
```

## Expected Output

When correctly implemented, 3 threads will execute in an **interleaved** manner:

```
=== User-Level Thread Test ===

Created threads: 1, 2, 3

[Alpha] iteration 0
[Beta] count = 0
[Gamma] step 1 of 5
[Alpha] iteration 1
[Beta] count = 10
[Gamma] step 2 of 5
[Alpha] iteration 2
[Beta] count = 20
[Gamma] step 3 of 5
[Alpha] iteration 3
[Beta] count = 30
[Gamma] step 4 of 5
[Alpha] iteration 4
[Beta] count = 40
[Gamma] step 5 of 5
[Alpha] done
[uthread] thread 1 (thread_1) exited
[Beta] done
[uthread] thread 2 (thread_2) exited
[Gamma] done
[uthread] thread 3 (thread_3) exited

=== All threads finished ===
```

Key point: Alpha, Beta, and Gamma should output **alternately in order**. If Alpha outputs 5 times in a row before Beta outputs, there is an issue with `thread_yield` or `thread_schedule` implementation.

## Automated Testing

```bash
cd tests/
./test_uthread.sh ../skeleton
```

Test items:
1. Compilation success (`-Wall -Werror`)
2. Program terminates normally (check for infinite loop/segfault)
3. All 3 threads produce output
4. All 3 threads complete
5. Interleaved execution
6. Thread exit messages are printed

## Grading Criteria

| Item | Weight |
|------|------|
| `thread_create` implementation (context initialization) | 25% |
| `thread_yield` implementation (context save + scheduler switch) | 20% |
| `thread_schedule` implementation (round-robin + swapcontext) | 35% |
| `thread_exit` implementation (return to scheduler via setcontext) | 10% |
| Code style and comments | 10% |

## Correspondence with xv6

The structure of this assignment corresponds 1:1 with the xv6 kernel scheduler:

| uthread (this assignment) | xv6 | Role |
|---|---|---|
| `thread_create` | `allocproc` | Create new thread/process, initialize context |
| `thread_yield` | `yield` | Voluntary CPU yield |
| `thread_schedule` | `scheduler` | Select next execution target and switch |
| `thread_exit` | `exit` | Thread/process termination |
| `swapcontext` | `swtch` | Context save + restore |
| `struct thread` | `struct proc` | Thread/process state management |
| `threads[]` | `proc[]` | Global thread/process table |

## Important Notes

- `uthread.h` must be included **before** other headers (due to `_XOPEN_SOURCE` definition).
- On macOS, ucontext will show deprecation warnings, but these are automatically suppressed in the header.
- Arguments passed to `makecontext` must be of type `int` (pointers cannot be passed).
- The stack grows downward, but `uc_stack.ss_sp` should be set to the **bottom** (lowest address) of the stack. `makecontext` automatically sets the stack pointer correctly.

## References

- xv6 textbook Chapter 7: Scheduling
- `man getcontext`, `man makecontext`, `man swapcontext`
- MIT 6.S081 Lab: Multithreading (https://pdos.csail.mit.edu/6.S081/)
