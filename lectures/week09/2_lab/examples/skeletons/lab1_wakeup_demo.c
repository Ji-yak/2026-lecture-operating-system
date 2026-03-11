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

  /* TODO: Create a pipe using the pipe() system call.
   *   - pipe(fds) allocates a struct pipe in the kernel and returns
   *     2 file descriptors: fds[0] for reading, fds[1] for writing.
   *   - Handle the error case (pipe() returns < 0). */

  /* TODO: Create a child process using fork().
   *   - fork() duplicates the current process.
   *   - The child gets copies of the parent's file descriptors.
   *   - Handle the error case (fork() returns < 0). */

  /* TODO: Implement the child process (Reader) in the pid == 0 branch:
   *   1. Close the unused write end (fds[1]) with close().
   *      (If not closed, EOF will never be delivered even when the parent
   *       closes its write end, because pi->writeopen would still be > 0.)
   *   2. Print a message indicating the reader is about to call read().
   *   3. Declare a buffer (e.g., char buf[32]) and call read(fds[0], buf, ...).
   *      - The pipe is empty at this point, so read() will block (sleep)
   *        until the Writer writes data.
   *   4. When read() returns with data, null-terminate and print the message.
   *   5. Close fds[0] and call exit(0). */

  /* TODO: Implement the parent process (Writer) in the else branch:
   *   1. Close the unused read end (fds[0]) with close().
   *   2. Use pause(10) to wait 10 ticks so the Reader enters sleep first.
   *   3. Prepare a message string (e.g., "wakeup!").
   *   4. Write the message to the pipe using write(fds[1], msg, strlen(msg)).
   *      - Inside the kernel, pipewrite() copies data and calls
   *        wakeup(&pi->nread) to wake up the sleeping Reader.
   *   5. Close fds[1] (write end).
   *   6. Call wait(0) to wait for the child to terminate.
   *   7. Print a completion message. */
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

  /* TODO: Create a pipe using pipe(). Handle errors. */

  /* TODO: Create a child process using fork(). Handle errors. */

  /* TODO: Implement the child process (Reader) in the pid == 0 branch:
   *   1. Close the unused write end (fds[1]).
   *   2. Use pause(10) to wait 10 ticks so the Writer fills the buffer first.
   *   3. Declare a read buffer (e.g., char buf[128]) and a total counter.
   *   4. In a loop, call read(fds[0], buf, sizeof(buf)) until it returns 0 (EOF).
   *      - Each read() call triggers wakeup(&pi->nwrite) in piperead(),
   *        waking up the blocked Writer.
   *      - Accumulate the total bytes read.
   *   5. Print the total bytes read.
   *   6. Close fds[0] and call exit(0). */

  /* TODO: Implement the parent process (Writer) in the else branch:
   *   1. Close the unused read end (fds[0]).
   *   2. Prepare a large buffer (600 bytes, more than PIPESIZE=512).
   *      Fill it with alphabet characters: 'A' + (i % 26).
   *   3. Print a message indicating the write attempt.
   *   4. Call write(fds[1], bigbuf, 600).
   *      - After writing 512 bytes, the buffer is full.
   *      - pipewrite() calls wakeup(&pi->nread) then sleep(&pi->nwrite).
   *      - When the Reader reads data, the Writer wakes up and writes the rest.
   *   5. Print how many bytes were written.
   *   6. Close fds[1] to signal EOF to the Reader.
   *   7. Call wait(0) to wait for the child.
   *   8. Print a completion message. */
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

  /* TODO: Create a pipe using pipe(). Handle errors. */

  /* TODO: Create a child process using fork(). Handle errors. */

  /* TODO: Implement the child process (Reader) in the pid == 0 branch:
   *   1. Close the unused write end (fds[1]).
   *      (This is required for EOF to work!)
   *   2. Declare a buffer (e.g., char buf[32]).
   *   3. First read(): Call read(fds[0], buf, sizeof(buf) - 1).
   *      - This will block until the Writer sends data.
   *      - When data arrives, null-terminate and print it.
   *   4. Second read(): Call read() again.
   *      - The Writer will have closed the pipe by now.
   *      - Kernel behavior in piperead():
   *        pi->writeopen == 0, so the while loop exits immediately.
   *        No data to copy, so read() returns 0 (EOF).
   *   5. Print the return value of the second read (should be 0 = EOF).
   *   6. Close fds[0] and call exit(0). */

  /* TODO: Implement the parent process (Writer) in the else branch:
   *   1. Close the unused read end (fds[0]).
   *   2. Prepare a message (e.g., "last message") and write it to the pipe.
   *   3. Use pause(10) to give the Reader time to process the first message
   *      and re-enter sleep on the second read().
   *   4. Close the write end (fds[1]).
   *      - Kernel calls pipeclose():
   *        pi->writeopen = 0, then wakeup(&pi->nread).
   *      - The Reader wakes up, sees writeopen == 0, returns 0 (EOF).
   *   5. Call wait(0) to wait for the child.
   *   6. Print a completion message. */
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
