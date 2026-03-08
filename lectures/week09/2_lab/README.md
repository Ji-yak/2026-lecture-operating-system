# Week 9 Lab: Synchronization Tools and Examples - sleep/wakeup, pipe

## Learning Objectives

By the end of this lab, you will be able to:

1. Explain how xv6's `sleep()` and `wakeup()` mechanisms implement inter-process coordination
2. Analyze how sleep/wakeup supports the producer-consumer pattern in the pipe implementation
3. Write an inter-process communication program using pipes in xv6
4. Explain why the "lost wakeup" problem occurs and how passing a lock to sleep solves it

## Background Knowledge

### What is sleep/wakeup?

In operating systems, it is very common for a process to need to wait until a certain condition is met. For example:
- A reader waiting for data to arrive in a pipe
- A writer waiting for space to become available in a pipe buffer
- A parent process waiting for a child process to exit

In these cases, busy waiting (a loop that continuously checks the condition) wastes CPU. xv6 provides the **sleep/wakeup** mechanism to solve this:

- **`sleep(chan, lk)`**: Transitions the current process to the `SLEEPING` state and yields the CPU until it is woken up on the channel (`chan`). `lk` is the lock that protects the condition.
- **`wakeup(chan)`**: Transitions all processes sleeping on the given channel to the `RUNNABLE` state.

### The Channel Concept

In sleep/wakeup, a "channel" is an arbitrary address value that identifies a specific condition. In xv6, the address of a related data structure is typically used as the channel. All processes sleeping on the same channel are woken up when wakeup is called on that channel.

---

## Exercise 1: sleep/wakeup Code Analysis (15 min)

### 1.1 Analyzing the `sleep()` Function

Read the `sleep()` function at lines 540-569 of `kernel/proc.c`:

```c
// kernel/proc.c, line 540-569

// Sleep on channel chan, releasing condition lock lk.
// Re-acquires lk when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}
```

### Discussion Questions

**Q1-1.** What are the two parameters of `sleep()`?

- `chan` (void *): The sleep channel. An address value that identifies what event the process is waiting for.
- `lk` (struct spinlock *): The lock that protects the condition. This lock must be held before entering sleep.

**Q1-2.** Trace the order in which locks are exchanged inside the `sleep()` function:

1. `acquire(&p->lock)` - Acquire the process's own lock
2. `release(lk)` - Release the condition lock
3. (After waking up from sleep)
4. `release(&p->lock)` - Release the process lock
5. `acquire(lk)` - Re-acquire the condition lock

> **Key point**: `p->lock` is acquired first before `lk` is released. Why this order is important will be revisited in Exercise 4.

**Q1-3.** After setting `p->chan = chan` and `p->state = SLEEPING`, `sched()` is called. What does `sched()` do?

> `sched()` saves the current process's context and switches to the CPU's scheduler context (context switch). The scheduler finds and runs another RUNNABLE process. The current process will not execute until someone calls `wakeup(chan)`.

**Q1-4.** Why is the channel reset to `p->chan = 0` after waking up from sleep?

> To indicate that the process is no longer waiting on a specific channel. This prevents the process from being mistakenly woken up by subsequent wakeup calls.

### 1.2 Analyzing the `wakeup()` Function

Read the `wakeup()` function at lines 571-587 of `kernel/proc.c`:

```c
// kernel/proc.c, line 571-587

// Wake up all processes sleeping on channel chan.
// Caller should hold the condition lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}
```

### Discussion Questions

**Q1-5.** `wakeup()` iterates through the entire process table. Why is the `p != myproc()` condition necessary?

> Attempting to wake up yourself is meaningless. Also, if you already hold your own `p->lock` and try to acquire it again, a deadlock can occur.

**Q1-6.** wakeup wakes up **all** processes whose channel matches. What are the advantages and disadvantages of this?

> - **Advantage**: The implementation is simple, and wakeup does not need to know which specific process to wake up.
> - **Disadvantage**: Even if only one process can actually proceed, all processes are woken up ("thundering herd" problem). The woken processes must re-check the condition, and if it is not met, they must go back to sleep.

**Q1-7.** Why does `wakeup()` acquire each process's `p->lock`?

> To safely read and modify `p->state` and `p->chan`. This prevents another CPU from concurrently changing the same process's state.

---

## Exercise 2: Pipe Implementation Analysis (15 min)

### 2.1 Pipe Data Structure

Examine the pipe structure defined at lines 11-20 of `kernel/pipe.c`:

```c
// kernel/pipe.c, line 11-20

#define PIPESIZE 512

struct pipe {
  struct spinlock lock;
  char data[PIPESIZE];
  uint nread;     // number of bytes read
  uint nwrite;    // number of bytes written
  int readopen;   // read fd is still open
  int writeopen;  // write fd is still open
};
```

**Q2-1.** The pipe buffer is implemented as a circular buffer. `nread` and `nwrite` are monotonically increasing counters. How is the actual buffer index calculated?

> Using the modulo operation, such as `pi->data[pi->nwrite % PIPESIZE]` and `pi->data[pi->nread % PIPESIZE]`.

**Q2-2.** What is the condition for the buffer being full? What is the condition for it being empty?

> - **Full**: `nwrite == nread + PIPESIZE` (the amount written is PIPESIZE ahead of the amount read)
> - **Empty**: `nread == nwrite` (the amount read equals the amount written)

### 2.2 Analyzing `pipewrite()`

```c
// kernel/pipe.c, line 76-103

int
pipewrite(struct pipe *pi, uint64 addr, int n)
{
  int i = 0;
  struct proc *pr = myproc();

  acquire(&pi->lock);
  while(i < n){
    if(pi->readopen == 0 || killed(pr)){
      release(&pi->lock);
      return -1;
    }
    if(pi->nwrite == pi->nread + PIPESIZE){ //DOC: pipewrite-full
      wakeup(&pi->nread);
      sleep(&pi->nwrite, &pi->lock);
    } else {
      char ch;
      if(copyin(pr->pagetable, &ch, addr + i, 1) == -1)
        break;
      pi->data[pi->nwrite++ % PIPESIZE] = ch;
      i++;
    }
  }
  wakeup(&pi->nread);
  release(&pi->lock);

  return i;
}
```

### Discussion Questions

**Q2-3.** At line 88, when the buffer is full, what does the writer do? Explain step by step.

> 1. `wakeup(&pi->nread)` - Wake up the reader (so it reads data and frees up space)
> 2. `sleep(&pi->nwrite, &pi->lock)` - Sleep on channel `&pi->nwrite`
>    - Inside sleep, `pi->lock` is released so the reader can proceed
>    - When the reader reads data and calls `wakeup(&pi->nwrite)`, the writer is woken up
> 3. After waking up, it returns to the top of the while loop to check the condition again

**Q2-4.** Why is `&pi->nwrite` used as the channel in `sleep(&pi->nwrite, &pi->lock)`?

> It uses the address of the `nwrite` field within the pipe structure as the channel. This is a unique address value that identifies "waiting to be able to write." It matches with the reader's `wakeup(&pi->nwrite)` call.

**Q2-5.** Why is `wakeup(&pi->nread)` called after the writer loop ends (line 99)?

> To notify the reader that "there is data to read" after writing data. The reader may be sleeping on an empty buffer.

### 2.3 Analyzing `piperead()`

```c
// kernel/pipe.c, line 105-134

int
piperead(struct pipe *pi, uint64 addr, int n)
{
  int i;
  struct proc *pr = myproc();
  char ch;

  acquire(&pi->lock);
  while(pi->nread == pi->nwrite && pi->writeopen){  //DOC: pipe-empty
    if(killed(pr)){
      release(&pi->lock);
      return -1;
    }
    sleep(&pi->nread, &pi->lock); //DOC: piperead-sleep
  }
  for(i = 0; i < n; i++){  //DOC: piperead-copy
    if(pi->nread == pi->nwrite)
      break;
    ch = pi->data[pi->nread % PIPESIZE];
    if(copyout(pr->pagetable, addr + i, &ch, 1) == -1) {
      if(i == 0)
        i = -1;
      break;
    }
    pi->nread++;
  }
  wakeup(&pi->nwrite);  //DOC: piperead-wakeup
  release(&pi->lock);
  return i;
}
```

### Discussion Questions

**Q2-6.** Analyze the while condition on line 113. Why does it sleep when both conditions are true?

> - `pi->nread == pi->nwrite`: The buffer is empty (there is no data to read)
> - `pi->writeopen`: The writer is still open (data may still arrive)
>
> If both conditions are true, there may still be data coming, so it waits. If `writeopen` is 0, no more data can arrive, so it does not wait and immediately returns 0 (EOF).

**Q2-7.** Complete the table below to summarize the sleep/wakeup relationship between piperead and pipewrite:

| Action | sleep channel | wakeup channel | Meaning |
|--------|--------------|----------------|---------|
| Writer waiting on full buffer | `&pi->nwrite` | (reader calls) `&pi->nwrite` | "Space available for writing" |
| Reader waiting on empty buffer | `&pi->nread` | (writer calls) `&pi->nread` | "Data available for reading" |

**Q2-8.** In `piperead()`, why does the for loop (line 120) check `if(pi->nread == pi->nwrite) break;` again after exiting the while loop?

> There are two cases for exiting the while loop:
> 1. There is data in the buffer (`pi->nread != pi->nwrite`)
> 2. The writer has been closed (`!pi->writeopen`)
>
> In case 2, there may or may not be data remaining in the buffer. While reading data one byte at a time in the for loop, it must stop if the buffer becomes empty.

---

## Exercise 3: Writing a Producer-Consumer Program (15 min)

### 3.1 Overview

Write an xv6 user program that implements the producer-consumer pattern using pipes.

- **Producer (parent process)**: Writes data to the pipe
- **Consumer (child process)**: Reads data from the pipe and prints it

### 3.2 Writing the Code

Refer to the `examples/lab3_producer_consumer.c` file. Below is the core structure:

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int fds[2];
  int pid;

  // Create pipe
  if(pipe(fds) < 0){
    printf("pipe() failed\n");
    exit(1);
  }

  pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(1);
  }

  if(pid == 0){
    // Child process (Consumer)
    close(fds[1]);  // Close write end
    // ... read from fds[0] ...
    close(fds[0]);
    exit(0);
  } else {
    // Parent process (Producer)
    close(fds[0]);  // Close read end
    // ... write to fds[1] ...
    close(fds[1]);
    wait(0);
    exit(0);
  }
}
```

### Lab Steps

**Step 1**: Read and understand the full code in the `examples/lab3_producer_consumer.c` file.

**Step 2**: Add this program to xv6 and run it.

To add a new user program to xv6:

1. Copy the source file to the `user/` directory:
   ```
   cp practice/week9/lab/examples/lab3_producer_consumer.c user/lab3_producer_consumer.c
   ```

2. Add it to the `UPROGS` list in the `Makefile`:
   ```makefile
   UPROGS=\
       ...
       $U/_lab3_producer_consumer\
   ```

3. Build and run xv6:
   ```
   make clean && make qemu
   ```

4. Run it in the xv6 shell:
   ```
   $ lab3_producer_consumer
   ```

**Step 3**: Observe the output.

Expected output:
```
[Producer] Sending 5 messages through pipe...
[Producer] Sent: msg 0: hello from producer
[Producer] Sent: msg 1: hello from producer
[Producer] Sent: msg 2: hello from producer
[Producer] Sent: msg 3: hello from producer
[Producer] Sent: msg 4: hello from producer
[Producer] Done. Closing write end.
[Consumer] Reading from pipe...
[Consumer] Received: msg 0: hello from producer
[Consumer] Received: msg 1: hello from producer
[Consumer] Received: msg 2: hello from producer
[Consumer] Received: msg 3: hello from producer
[Consumer] Received: msg 4: hello from producer
[Consumer] Pipe closed by producer (read returned 0). Exiting.
```

> **Note**: Since the execution order of the parent and child after fork() depends on the scheduler, the Producer and Consumer messages may be interleaved in the output. This is normal.

### Discussion Questions

**Q3-1.** What happens if the Consumer runs before the Producer?

> When the Consumer calls `read()`, the pipe buffer is empty, so `sleep(&pi->nread, &pi->lock)` is called inside the kernel's `piperead()`. The Consumer enters the SLEEPING state, and when the Producer writes data, it is woken up by `wakeup(&pi->nread)`.

**Q3-2.** When the Producer closes the write end with `close(fds[1])`, what value does the Consumer's `read()` return?

> It returns 0 (EOF). In `piperead()`, `pi->writeopen` becomes 0, so the while loop is exited, and if there is no data in the buffer, the for loop is skipped with `i = 0`, returning 0.

**Q3-3.** What happens if the Producer tries to write data larger than the pipe buffer size (512 bytes) at once?

> In `pipewrite()`, when the buffer becomes full (`nwrite == nread + PIPESIZE`), the writer sleeps. When the Consumer reads data and frees up space, the writer is woken up and writes the rest. Therefore, no data is lost and flow control is automatically achieved.

---

## Exercise 4: Understanding the Lost Wakeup Problem (5 min)

### 4.1 What is a Lost Wakeup?

A "lost wakeup" is a bug where a wakeup signal is lost, causing a process to sleep forever. This problem occurs when locks are not handled correctly when using sleep/wakeup.

### 4.2 Example of an Incorrect Implementation

Suppose `sleep()` is a simple implementation that does not take a lock parameter:

```c
// Incorrect sleep implementation (no lock parameter)
void broken_sleep(void *chan)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->chan = chan;
  p->state = SLEEPING;
  sched();
  p->chan = 0;
  release(&p->lock);
}
```

Writing piperead with this implementation:

```c
// Incorrect piperead - lost wakeup can occur!
int broken_piperead(struct pipe *pi, uint64 addr, int n)
{
  acquire(&pi->lock);
  while(pi->nread == pi->nwrite && pi->writeopen){
    release(&pi->lock);        // (A) Release lock
    // <-- What if wakeup happens here?! -->
    broken_sleep(&pi->nread);  // (B) Enter sleep
  }
  // ... read data ...
  release(&pi->lock);
}
```

### 4.3 Problem Scenario

Assume execution proceeds in the following order:

```
Time  Reader (CPU 0)                    Writer (CPU 1)
----  --------------------------------  --------------------------------
 1    Check buffer empty (condition true)
 2    release(&pi->lock)  // (A)
 3                                      acquire(&pi->lock)
 4                                      Write data to buffer
 5                                      wakeup(&pi->nread)  // Reader is
 6                                      // not SLEEPING yet, so ignored!
 7                                      release(&pi->lock)
 8    broken_sleep(&pi->nread) // (B)
 9    // Sleeps forever - nobody to wake it up!
```

**Core problem**: There is a **gap** between point (A) where the lock is released and point (B) where the process actually enters the SLEEPING state. If the writer calls wakeup during this gap, the reader has not yet gone to sleep, so the wakeup is ignored. After the reader goes to sleep, there is nobody to wake it up.

### 4.4 xv6's Correct Solution

xv6's `sleep()` solves this problem as follows:

```c
// kernel/proc.c, line 540-569 (correct implementation)
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  acquire(&p->lock);   // (1) Acquire process lock
  release(lk);         // (2) Release condition lock

  p->chan = chan;
  p->state = SLEEPING; // (3) Set SLEEPING state

  sched();             // (4) Switch to scheduler

  p->chan = 0;

  release(&p->lock);
  acquire(lk);
}
```

**How it works**:

1. `release(lk)` is done after `acquire(&p->lock)`.
2. Since `lk` is released while holding `p->lock`, even if the writer calls `wakeup()`, it cannot acquire `p->lock` and thus cannot check the reader's state.
3. `p->lock` is only released (inside the scheduler) **after** the reader has fully set its state to `p->state = SLEEPING` and yielded the CPU with `sched()`.
4. Only then can the writer's `wakeup()` acquire `p->lock` and wake up the reader.

In other words, **"checking the condition -> transitioning to sleep state" is done atomically** (protected by p->lock).

```
Time  Reader (CPU 0)                    Writer (CPU 1)
----  --------------------------------  --------------------------------
 1    Check buffer empty (condition true)
 2    acquire(&p->lock)
 3    release(&pi->lock)
 4    p->state = SLEEPING               acquire(&pi->lock)
 5    sched() -> p->lock released       Write data to buffer
 6                                      wakeup(&pi->nread)
 7                                        acquire(&p->lock) // Now succeeds
 8                                        p->state = RUNNABLE // Woken up!
 9                                        release(&p->lock)
```

### Discussion Questions

**Q4-1.** Summarize the rule "you must pass the lock to sleep" in one sentence.

> sleep receives the condition lock and ensures that the transition to SLEEPING state and the lock release happen atomically. This prevents wakeup from being lost in the gap between "checking the condition" and "going to sleep."

**Q4-2.** Run the `examples/lab1_wakeup_demo.c` program to observe blocking/wakeup behavior through pipes.

The steps to add it to xv6 are the same as Exercise 3:
1. Copy the source file to the `user/` directory
2. Add `$U/_lab1_wakeup_demo\` to `UPROGS` in the Makefile
3. Run `make clean && make qemu` and then execute `lab1_wakeup_demo` in the xv6 shell

---

## Summary and Key Takeaways

### 1. sleep/wakeup Mechanism

| Item | Description |
|------|-------------|
| `sleep(chan, lk)` | Transitions the current process to SLEEPING state on channel `chan`. Atomically releases `lk` |
| `wakeup(chan)` | Transitions all processes sleeping on `chan` to RUNNABLE |
| Channel | An arbitrary address value that identifies an event (usually the address of related data) |

### 2. sleep/wakeup Usage in Pipes

| Situation | Function called | sleep channel | Woken up by |
|-----------|----------------|---------------|-------------|
| Reader waiting on empty buffer | `sleep(&pi->nread, &pi->lock)` | `&pi->nread` | Writer's `wakeup(&pi->nread)` |
| Writer waiting on full buffer | `sleep(&pi->nwrite, &pi->lock)` | `&pi->nwrite` | Reader's `wakeup(&pi->nwrite)` |

### 3. Preventing Lost Wakeups

- **Problem**: If wakeup occurs between checking the condition and sleeping, it is lost
- **Solution**: Pass the condition lock to `sleep()` so that the transition to SLEEPING state and lock release are performed atomically
- **Key sequence**: `acquire(p->lock)` -> `release(lk)` -> `SLEEPING` -> `sched()` -> (scheduler releases `p->lock`)

### 4. Producer-Consumer Pattern

- A pipe is a kernel-level producer-consumer implementation
- Automatic flow control: If the buffer is full, the producer sleeps; if empty, the consumer sleeps
- EOF handling: When the write end is closed, the reader's `read()` returns 0
