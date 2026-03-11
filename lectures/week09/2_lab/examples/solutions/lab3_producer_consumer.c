/*
 * producer_consumer.c - xv6 user program
 *
 * [Overview]
 *   A demo program implementing the Producer-Consumer pattern using pipes.
 *   The Producer (parent) writes multiple messages to the pipe,
 *   and the Consumer (child) reads messages from the pipe and prints them.
 *
 * [Key Concepts]
 *   - Producer-Consumer pattern: One side produces data and the other consumes it.
 *     The pipe's internal buffer serves as a bounded buffer.
 *   - The blocking nature of pipes automatically provides synchronization:
 *     (1) If the Consumer has no data to read, it sleeps (waits until the Producer writes)
 *     (2) If the Producer fills the buffer, it sleeps (waits until the Consumer reads)
 *   - A length-prefix protocol is used to distinguish message boundaries.
 *     A 1-byte length value is sent before each message so the receiver can read exactly.
 *
 * [EOF Handling]
 *   When the Producer closes the write end:
 *   - Kernel calls pipeclose() -> pi->writeopen = 0 -> wakeup(&pi->nread)
 *   - Consumer's read() returns 0 (EOF)
 *   - Consumer detects EOF and exits the loop to terminate
 *
 * [Build Instructions (xv6 environment)]
 *   1. Copy this file to the xv6-riscv/user/ directory
 *   2. Add $U/_producer_consumer\ to UPROGS in the Makefile
 *   3. make clean && make qemu
 */

#include "kernel/types.h"   /* xv6 basic type definitions (uint, uint64, etc.) */
#include "kernel/stat.h"    /* File status structure definition */
#include "user/user.h"      /* xv6 user system call and library function declarations */

#define NUM_MESSAGES 5   /* Number of messages the Producer will send */
#define MSG_SIZE 64      /* Maximum message buffer size */

/*
 * int_to_str: Helper function that converts an integer to a string
 *
 * Since xv6 does not have sprintf(), we must implement it manually.
 * Extracts each digit by dividing by 10 and stores them in reverse order in the string.
 */
static void
int_to_str(int n, char *buf)
{
  if(n == 0){
    buf[0] = '0';
    buf[1] = '\0';
    return;
  }

  char tmp[12];  /* Max digits for int + sign + null */
  int i = 0;
  int neg = 0;

  if(n < 0){
    neg = 1;
    n = -n;
  }

  /* Extract digits from lowest (stored in reverse order) */
  while(n > 0){
    tmp[i++] = '0' + (n % 10);
    n /= 10;
  }

  int j = 0;
  if(neg)
    buf[j++] = '-';

  /* Copy the reversed digits in forward order */
  while(i > 0)
    buf[j++] = tmp[--i];

  buf[j] = '\0';
}

/*
 * make_message: Creates a message string containing a sequence number.
 *
 * Generated format: "msg N: hello from producer"
 * Since xv6 does not have snprintf(), string parts are concatenated manually.
 *
 * Return value: Length of the generated message (excluding null)
 */
static int
make_message(int seq, char *buf, int bufsize)
{
  char num[12];
  int_to_str(seq, num);

  /* Manual string concatenation: Copy each part into the buffer in order */
  char *parts[] = {"msg ", num, ": hello from producer"};
  int nparts = 3;
  int pos = 0;

  for(int p = 0; p < nparts; p++){
    for(int k = 0; parts[p][k] != '\0' && pos < bufsize - 1; k++){
      buf[pos++] = parts[p][k];
    }
  }
  buf[pos] = '\0';
  return pos;  /* Return message length */
}

/*
 * consumer: Consumer function that reads messages from the pipe and prints them
 *
 * Protocol:
 *   Receives in [1-byte length][message body] format.
 *   1. First reads 1 byte to determine the message length.
 *   2. Then reads the message body for that length.
 *   3. If read() returns 0, it is EOF, so terminate.
 *
 * sleep/wakeup behavior:
 *   - If no data is available in read(), sleep(&pi->nread, &pi->lock) inside piperead()
 *   - When Producer calls write(), wakeup(&pi->nread) wakes up the Consumer
 */
static void
consumer(int readfd)
{
  char buf[MSG_SIZE];
  int n;

  printf("[Consumer] Reading from pipe...\n");

  /* Loop that repeatedly reads messages.
   * When reading data from the pipe:
   *   - If data is available, returns immediately
   *   - If no data and writer is open, sleeps (blocking)
   *   - If no data and writer is closed, returns 0 (EOF) */
  while(1){
    /* Step 1: Read the message length (1 byte).
     * The read() system call invokes piperead().
     * If the pipe is empty, it sleeps and waits until the Producer writes. */
    char lenbuf;
    n = read(readfd, &lenbuf, 1);
    if(n <= 0)
      break;  /* EOF or error: exit loop */

    int msglen = (int)lenbuf;
    if(msglen <= 0 || msglen >= MSG_SIZE)
      break;  /* Abnormal length: exit safely */

    /* Step 2: Read the message body.
     * Since pipes are byte streams, a single read() may not
     * return all the requested bytes.
     * (e.g., only partial data remains in the pipe buffer)
     * Therefore, we must read repeatedly until the requested length is fulfilled. */
    int total = 0;
    while(total < msglen){
      n = read(readfd, buf + total, msglen - total);
      if(n <= 0)
        break;  /* EOF or error */
      total += n;
    }
    if(total < msglen)
      break;  /* Incomplete message, terminate */

    buf[total] = '\0';  /* Null-terminate the string */
    printf("[Consumer] Received: %s\n", buf);
  }

  /* When the Producer closes the write end, read() returns 0.
   * This is the EOF delivery mechanism through pipes. */
  printf("[Consumer] Pipe closed by producer (read returned 0). Exiting.\n");
}

/*
 * producer: Producer function that writes messages to the pipe
 *
 * Protocol:
 *   Sends in [1-byte length][message body] format.
 *   This length-prefix approach allows the Consumer to determine message boundaries precisely.
 *
 * sleep/wakeup behavior:
 *   - If the buffer is full during write(), sleep(&pi->nwrite, &pi->lock) inside pipewrite()
 *   - When Consumer calls read(), wakeup(&pi->nwrite) wakes up the Producer
 */
static void
producer(int writefd)
{
  char buf[MSG_SIZE];

  printf("[Producer] Sending %d messages through pipe...\n", NUM_MESSAGES);

  for(int i = 0; i < NUM_MESSAGES; i++){
    /* Create the message string */
    int msglen = make_message(i, buf, MSG_SIZE);

    /* Step 1: Write the message length (1 byte) to the pipe first.
     * The write() system call invokes pipewrite().
     * If the buffer is full, it sleeps and waits until the Consumer reads. */
    char lenbuf = (char)msglen;
    write(writefd, &lenbuf, 1);

    /* Step 2: Write the message body to the pipe.
     * Inside pipewrite(), data is copied 1 byte at a time into pi->data circular buffer.
     * If the buffer becomes full (pi->nwrite == pi->nread + PIPESIZE):
     *   -> wakeup(&pi->nread)  -- Wake up the Consumer
     *   -> sleep(&pi->nwrite)  -- Producer itself sleeps
     * When the Consumer reads data, it wakes up the Producer via wakeup(&pi->nwrite). */
    write(writefd, buf, msglen);

    printf("[Producer] Sent: %s\n", buf);
  }

  printf("[Producer] Done. Closing write end.\n");
}

/*
 * main: Entry point of the Producer-Consumer program
 *
 * Execution flow:
 *   1. Create a communication channel with pipe()
 *   2. Split processes with fork() (child=Consumer, parent=Producer)
 *   3. Each process closes the unused pipe end with close()
 *   4. Producer writes messages, Consumer reads them
 *   5. Producer closes the write end with close() to deliver EOF
 *   6. Parent waits for child termination with wait()
 */
int
main(int argc, char *argv[])
{
  int fds[2];  /* fds[0] = read end, fds[1] = write end */
  int pid;

  /* pipe() system call: Internally calls pipealloc() in the kernel,
   * allocating a struct pipe and returning two file descriptors.
   * Inside the pipe is a 512-byte circular buffer (pi->data[PIPESIZE]). */
  if(pipe(fds) < 0){
    printf("pipe() failed\n");
    exit(1);
  }

  /* fork() system call: Duplicates the current process to create a child process.
   * The child receives a copy of the parent's file descriptor table, so
   * it can access both ends of the same pipe.
   * Return value: child's pid to the parent, 0 to the child */
  pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(1);
  }

  if(pid == 0){
    /* ---- Child Process (Consumer) ----
     *
     * Why the write end must be closed:
     *   The child also received a copy of the write end's file descriptor,
     *   so if not closed, pi->writeopen will still be 1 in piperead().
     *   In that case, even if the Producer calls close(), the Consumer will not
     *   receive EOF and will be stuck in sleep forever.
     *   (Cannot exit the while loop because pi->writeopen > 0) */
    close(fds[1]);

    consumer(fds[0]);  /* Read messages from the pipe and print them */

    close(fds[0]);  /* Close the read end */
    exit(0);        /* Terminate child process */
  } else {
    /* ---- Parent Process (Producer) ----
     *
     * Close the read end. The parent only writes data, so the read end is unnecessary. */
    close(fds[0]);

    producer(fds[1]);  /* Write messages to the pipe */

    /* Close the write end.
     * Inside the kernel, pipeclose() is called:
     *   1. pi->writeopen = 0        -- Mark that the write end is closed
     *   2. wakeup(&pi->nread)       -- Wake up the sleeping Consumer
     * The Consumer, in piperead()'s while condition,
     * sees pi->writeopen == 0 and exits the loop.
     * If there is no data to read, read() returns 0 (EOF). */
    close(fds[1]);

    /* wait() system call: Waits for the child process (Consumer) to finish.
     * Inside the kernel, if the child is still alive,
     * the parent sleeps via sleep(p, &wait_lock).
     * When the child calls exit(), the parent is woken up. */
    wait(0);
    exit(0);  /* Terminate parent process. In xv6, exit() is used even in main */
  }
}
