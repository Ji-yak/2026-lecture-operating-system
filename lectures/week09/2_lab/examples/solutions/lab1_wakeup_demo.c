/*
 * wakeup_demo.c - xv6 user program
 *
 * [Overview]
 *   A program that observes the xv6 kernel's sleep/wakeup mechanism
 *   through pipe's blocking behavior.
 *
 * [Key Concepts]
 *   - sleep(chan, lock): Puts the current process to sleep at the chan (channel) address.
 *     The process state changes to SLEEPING and yields the CPU.
 *   - wakeup(chan): Wakes up all processes sleeping on that chan by setting them to RUNNABLE.
 *   - Pipes internally use a 512-byte circular buffer.
 *   - If the pipe is empty, the reader sleeps; if full, the writer sleeps.
 *   - When the writer closes the write end of the pipe, EOF (0) is delivered to the reader.
 *
 * [IPC (Inter-Process Communication) Perspective]
 *   Pipe is the most basic IPC mechanism provided by xv6.
 *   It is a unidirectional byte stream that transfers data between parent and child via fork().
 *   The blocking nature of pipes itself is synchronization utilizing sleep/wakeup.
 *
 * [3 Scenarios Demonstrated in This Program]
 *   Demo 1: Reader blocks (sleeps) on empty pipe -> Writer writes data to wakeup
 *   Demo 2: Writer blocks (sleeps) on full pipe -> Reader reads to wakeup
 *   Demo 3: Writer closes pipe -> EOF delivered to reader (wakeup then returns 0)
 *
 * [Build Instructions (xv6 environment)]
 *   1. Copy this file to the xv6-riscv/user/ directory
 *   2. Add $U/_wakeup_demo\ to UPROGS in the Makefile
 *   3. make clean && make qemu
 */

#include "kernel/types.h"   /* xv6 basic type definitions (uint, uint64, etc.) */
#include "kernel/stat.h"    /* File status structure definition */
#include "user/user.h"      /* xv6 user system call and library function declarations */

/* ============================================================
 * Demo 1: Reader waits for Writer
 * ============================================================
 * Scenario:
 *   When the Reader (child) calls read() first, the pipe is empty so
 *   sleep(&pi->nread, &pi->lock) is called in the kernel's piperead().
 *   After the Writer (parent) pauses and then writes data,
 *   wakeup(&pi->nread) is called inside pipewrite(),
 *   waking up the Reader.
 *
 * sleep/wakeup channel:
 *   - &pi->nread: Channel where the reader sleeps (when there is no data)
 *   - After writing data, the writer wakes up the reader via wakeup(&pi->nread)
 * ============================================================ */
static void
demo_reader_waits(void)
{
  int fds[2];   /* fds[0]: read end, fds[1]: write end */
  int pid;

  printf("=== Demo 1: Reader blocks until Writer sends data ===\n");

  /* pipe() system call: Calls pipealloc() in the kernel to
   * allocate a struct pipe and return 2 file descriptors for read/write.
   * Pipe internal buffer size: PIPESIZE = 512 bytes */
  if(pipe(fds) < 0){
    printf("pipe() failed\n");
    exit(1);   /* xv6's exit(): Terminates the process and releases resources */
  }

  /* fork() system call: Duplicates the current process to create a child process.
   * The child receives a copy of the parent's file descriptor table, so
   * it can access both ends (read end, write end) of the same pipe.
   * Return value: child's pid to the parent, 0 to the child */
  pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(1);
  }

  if(pid == 0){
    /* ---- Child Process (Reader) ---- */

    /* close() system call: Close the unused write end.
     * If the child does not close the write end, even when the parent closes
     * the write end later, pi->writeopen may still remain 1,
     * preventing EOF from being received. */
    close(fds[1]);

    printf("[Reader] Calling read()... (pipe is empty, will block)\n");

    /* At this point, the pipe buffer is empty.
     * Kernel internal behavior (piperead function):
     *   1. acquire(&pi->lock)  -- Acquire spinlock on pipe structure
     *   2. while(pi->nread == pi->nwrite && pi->writeopen)
     *        -> Condition is true (buffer empty, writer still open)
     *        -> sleep(&pi->nread, &pi->lock) is called:
     *             a. acquire(&p->lock)      -- Acquire process lock
     *             b. release(&pi->lock)     -- Release pipe lock (writer can proceed)
     *             c. p->chan = &pi->nread    -- Set the sleep channel address
     *             d. p->state = SLEEPING    -- Change process state
     *             e. sched()                -- Yield CPU, switch to scheduler
     *
     * The Reader stops here and waits until the Writer wakes it up. */

    char buf[32];
    /* read() system call: Reads data from the pipe.
     * If there is no data, it sleeps through the above process,
     * and when the writer writes data, it wakes up via wakeup and copies the data. */
    int n = read(fds[0], buf, sizeof(buf) - 1);

    /* When the Writer calls write(), wakeup(&pi->nread) occurs in pipewrite():
     *   1. wakeup() iterates through the entire process table (proc[NPROC])
     *   2. Finds a process where p->state == SLEEPING && p->chan == &pi->nread
     *   3. Changes p->state to RUNNABLE (ready to run)
     *   4. Scheduler runs the Reader again
     *   5. Returns from sched() inside sleep()
     *   6. Re-checks while loop condition in piperead() -> data exists, exits loop
     *   7. Copies buffer data to user space in the for loop */

    if(n > 0){
      buf[n] = '\0';
      printf("[Reader] Woke up! Received %d bytes: \"%s\"\n", n, buf);
    }

    close(fds[0]);  /* Close the read end to release pipe resources */
    exit(0);        /* Terminate child process */
  } else {
    /* ---- Parent Process (Writer) ---- */

    close(fds[0]);  /* Close the unused read end */

    /* pause(n): Pauses the process for n ticks in xv6.
     * Deliberately introduces a delay so that the Reader calls read() first
     * and enters the sleep state. */
    printf("[Writer] Waiting 10 ticks before sending data...\n");
    pause(10);

    char *msg = "wakeup!";
    printf("[Writer] Writing \"%s\" to pipe...\n", msg);

    /* Kernel internal behavior when write() system call is invoked (pipewrite function):
     *   1. acquire(&pi->lock)           -- Acquire pipe lock
     *   2. Copy 1 byte at a time into pi->data circular buffer
     *   3. wakeup(&pi->nread)           -- Wake up the Reader!
     *   4. release(&pi->lock)           -- Release pipe lock */
    write(fds[1], msg, strlen(msg));

    printf("[Writer] Data sent. Reader should wake up now.\n");

    close(fds[1]);  /* Close the write end */

    /* wait() system call: Waits until the child process terminates.
     * If the child is still running, the parent sleeps via sleep(&wait_lock).
     * When the child calls exit(), the parent is woken up. */
    wait(0);
    printf("=== Demo 1 complete ===\n\n");
  }
}

/* ============================================================
 * Demo 2: Writer blocking during large data transfer
 * ============================================================
 * Scenario:
 *   The pipe buffer size is 512 bytes (PIPESIZE).
 *   When the Writer tries to write more than 512 bytes, the buffer becomes full and
 *   sleep(&pi->nwrite, &pi->lock) is called in pipewrite().
 *   When the Reader reads data, wakeup(&pi->nwrite) wakes up
 *   the Writer to write the remainder.
 *
 * sleep/wakeup channel:
 *   - &pi->nwrite: Channel where the writer sleeps (when the buffer is full)
 *   - After reading data, the reader wakes up the writer via wakeup(&pi->nwrite)
 * ============================================================ */
static void
demo_writer_blocks(void)
{
  int fds[2];
  int pid;

  printf("=== Demo 2: Writer blocks when pipe buffer is full ===\n");

  /* Create a new pipe via pipe() system call */
  if(pipe(fds) < 0){
    printf("pipe() failed\n");
    exit(1);
  }

  /* Separate child (Reader) and parent (Writer) via fork() */
  pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(1);
  }

  if(pid == 0){
    /* ---- Child Process (Reader) - deliberately reads slowly ---- */
    close(fds[1]);  /* Close the unused write end */

    /* Reader waits for 10 ticks first so that
     * the Writer fills the 512-byte buffer and blocks (sleeps). */
    printf("[Reader] Waiting 10 ticks before reading...\n");
    pause(10);

    char buf[128];
    int total = 0;
    int n;

    printf("[Reader] Starting to read (Writer may be blocked)...\n");

    /* Repeatedly calls read() to read all data from the pipe.
     * When read() returns 0, it means EOF, so the loop exits.
     * Each time the Reader reads data, inside the kernel:
     *   piperead() -> pi->nread incremented -> wakeup(&pi->nwrite)
     * This wakes up the sleeping Writer to RUNNABLE state. */
    while((n = read(fds[0], buf, sizeof(buf))) > 0){
      total += n;
    }

    printf("[Reader] Total bytes read: %d\n", total);

    close(fds[0]);  /* Close the read end */
    exit(0);
  } else {
    /* ---- Parent Process (Writer) - writes large data at once ---- */
    close(fds[0]);  /* Close the unused read end */

    /* Write 600 bytes (88 bytes more than PIPESIZE=512)
     * Uses a buffer filled with alphabet characters */
    char bigbuf[600];
    for(int i = 0; i < 600; i++){
      bigbuf[i] = 'A' + (i % 26);
    }

    printf("[Writer] Trying to write 600 bytes (PIPESIZE=512)...\n");

    /* Kernel internal behavior when write() system call is invoked (pipewrite function):
     *   1. Enter pipewrite(), acquire(&pi->lock)
     *   2. Copy 1 byte at a time in a while loop into pi->data circular buffer
     *   3. After writing 512 bytes: pi->nwrite == pi->nread + PIPESIZE (buffer full)
     *   4. wakeup(&pi->nread)              -- Wake up the Reader
     *   5. sleep(&pi->nwrite, &pi->lock)   -- Writer itself sleeps
     *
     *   Then when the Reader reads data:
     *   6. pi->nread incremented inside piperead() (buffer space freed)
     *   7. wakeup(&pi->nwrite)             -- Wake up the Writer!
     *   8. Writer wakes up and writes the remaining 88 bytes */

    int written = write(fds[1], bigbuf, 600);
    printf("[Writer] Successfully wrote %d bytes\n", written);

    close(fds[1]);  /* Close the write end to signal EOF to the Reader */

    /* Wait for the child (Reader) to terminate via wait() */
    wait(0);
    printf("=== Demo 2 complete ===\n\n");
  }
}

/* ============================================================
 * Demo 3: Pipe close and EOF
 * ============================================================
 * Scenario:
 *   When the Writer closes the write end, pipeclose() is called,
 *   setting pi->writeopen = 0 and calling wakeup(&pi->nread).
 *   The Reader, in piperead()'s while condition, sees pi->writeopen == 0,
 *   exits the loop, and returns 0 if the buffer is empty (EOF).
 *
 * Key to EOF handling:
 *   piperead()'s while condition: (pi->nread == pi->nwrite && pi->writeopen)
 *   - Buffer empty (nread == nwrite) and writer open -> sleep (wait)
 *   - Buffer empty and writer closed (writeopen == 0) -> return 0 (EOF)
 *   Thus, closing the write end serves as the EOF signal.
 * ============================================================ */
static void
demo_pipe_close(void)
{
  int fds[2];
  int pid;

  printf("=== Demo 3: Pipe close triggers EOF for reader ===\n");

  /* Create a pipe via pipe() system call */
  if(pipe(fds) < 0){
    printf("pipe() failed\n");
    exit(1);
  }

  /* Separate child (Reader) and parent (Writer) via fork() */
  pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(1);
  }

  if(pid == 0){
    /* ---- Child Process (Reader) ---- */
    close(fds[1]);  /* Close the unused write end (required for receiving EOF!) */

    char buf[32];
    int n;

    printf("[Reader] Waiting for data...\n");

    /* First read(): Blocks (sleeps) until data arrives.
     * When the Writer writes data, it wakes up via wakeup and receives the data. */
    n = read(fds[0], buf, sizeof(buf) - 1);
    if(n > 0){
      buf[n] = '\0';
      printf("[Reader] Got: \"%s\"\n", buf);
    }

    printf("[Reader] Calling read() again... waiting for more data or EOF\n");

    /* Second read(): When the Writer calls close(), EOF is returned (returns 0)
     * Kernel internal behavior (piperead function):
     *   1. acquire(&pi->lock)
     *   2. while(pi->nread == pi->nwrite && pi->writeopen)
     *      -> pi->writeopen == 0, so the while condition is false
     *      -> Immediately exits the loop (does not sleep)
     *   3. for loop: pi->nread == pi->nwrite, so no data to copy
     *   4. return 0  -- This is the EOF signal */
    n = read(fds[0], buf, sizeof(buf) - 1);
    printf("[Reader] read() returned %d (0 = EOF)\n", n);

    close(fds[0]);  /* Close the read end */
    exit(0);
  } else {
    /* ---- Parent Process (Writer) ---- */
    close(fds[0]);  /* Close the unused read end */

    /* Send one piece of data */
    char *msg = "last message";
    /* write() system call: Writes a message to the pipe and wakes up the reader */
    write(fds[1], msg, strlen(msg));
    printf("[Writer] Sent \"%s\"\n", msg);

    /* Wait briefly so the Reader reads the first message
     * and has time to enter sleep again on the second read() */
    pause(10);
    printf("[Writer] Closing write end of pipe...\n");

    /* close() system call -> Calls pipeclose() inside the kernel:
     *   1. acquire(&pi->lock)
     *   2. pi->writeopen = 0          -- Mark that the write end is closed
     *   3. wakeup(&pi->nread)         -- Wake up the sleeping Reader
     *   4. release(&pi->lock)
     *   The Reader wakes up and re-checks the while condition,
     *   and since pi->writeopen == 0, it exits the loop and returns 0 (EOF) */
    close(fds[1]);

    /* Wait for the child (Reader) to terminate via wait() */
    wait(0);
    printf("=== Demo 3 complete ===\n\n");
  }
}

/* main: Runs the three demos in order */
int
main(int argc, char *argv[])
{
  printf("\n");
  printf("========================================\n");
  printf("  Sleep/Wakeup Demo via Pipe Blocking\n");
  printf("========================================\n\n");

  demo_reader_waits();   /* Demo 1: Reader sleeps on empty pipe -> Writer wakes up */
  demo_writer_blocks();  /* Demo 2: Writer sleeps on full pipe -> Reader wakes up */
  demo_pipe_close();     /* Demo 3: Writer closes -> EOF delivered to Reader */

  printf("All demos completed.\n");
  exit(0);  /* In xv6, exit() is used even in main (return is not available) */
}
