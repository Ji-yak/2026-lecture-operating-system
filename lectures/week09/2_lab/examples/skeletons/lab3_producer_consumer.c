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

  printf("[Consumer] Reading from pipe...\n");

  /* TODO: Implement the message-reading loop.
   *
   * Use the length-prefix protocol to read messages:
   *
   * while(1) {
   *   Step 1: Read 1 byte for the message length.
   *     - Declare a char variable (e.g., char lenbuf).
   *     - Call read(readfd, &lenbuf, 1).
   *     - If read() returns <= 0, break (EOF or error).
   *     - Cast lenbuf to int to get msglen.
   *     - If msglen <= 0 or msglen >= MSG_SIZE, break (invalid length).
   *
   *   Step 2: Read the message body of exactly msglen bytes.
   *     - Pipes are byte streams, so a single read() may not return
   *       all requested bytes. You must loop until all bytes are read.
   *     - Use a total counter: while(total < msglen), call
   *       read(readfd, buf + total, msglen - total) and add to total.
   *     - If read() returns <= 0 during this loop, break.
   *     - If total < msglen after the loop, break (incomplete message).
   *
   *   Step 3: Null-terminate buf and print the received message.
   * }
   */

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

  /* TODO: Implement the message-sending loop.
   *
   * for each message i from 0 to NUM_MESSAGES-1:
   *   1. Create the message string using make_message(i, buf, MSG_SIZE).
   *      - This returns the message length (msglen).
   *
   *   2. Write the length prefix (1 byte) to the pipe:
   *      - Cast msglen to char: char lenbuf = (char)msglen;
   *      - Call write(writefd, &lenbuf, 1).
   *
   *   3. Write the message body to the pipe:
   *      - Call write(writefd, buf, msglen).
   *      - Inside the kernel, pipewrite() copies data into the circular buffer.
   *      - If the buffer becomes full, the Producer sleeps until the Consumer reads.
   *
   *   4. Print the sent message.
   */

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

  /* TODO: Create a pipe using the pipe() system call.
   *   - pipe(fds) allocates a struct pipe in the kernel and returns
   *     two file descriptors.
   *   - Handle the error case (pipe() returns < 0). */

  /* TODO: Create a child process using fork().
   *   - Handle the error case (fork() returns < 0). */

  /* TODO: Implement the child process (Consumer) in the pid == 0 branch:
   *   1. Close the unused write end (fds[1]).
   *      Why: If the child keeps the write end open, pi->writeopen stays > 0
   *      in the kernel, so even when the Producer closes its write end,
   *      the Consumer will never receive EOF (stuck in sleep forever).
   *   2. Call consumer(fds[0]) to read messages.
   *   3. Close fds[0] and call exit(0). */

  /* TODO: Implement the parent process (Producer) in the else branch:
   *   1. Close the unused read end (fds[0]).
   *   2. Call producer(fds[1]) to write messages.
   *   3. Close the write end (fds[1]).
   *      - This triggers EOF delivery: pipeclose() sets pi->writeopen = 0
   *        and calls wakeup(&pi->nread) to wake up the Consumer.
   *   4. Call wait(0) to wait for the child (Consumer) to finish.
   *   5. Call exit(0). */
}
