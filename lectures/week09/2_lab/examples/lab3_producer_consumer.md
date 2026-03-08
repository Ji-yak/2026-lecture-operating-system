# Lab 3: Producer-Consumer with Pipes

## Description

This xv6 user program implements the classic producer-consumer pattern using a pipe for inter-process communication.

- **Producer (parent process)**: Generates 5 numbered messages (e.g., `msg 0: hello from producer`) and writes them to the pipe. Each message is preceded by a 1-byte length header.
- **Consumer (child process)**: Reads messages from the pipe using the length-header protocol and prints each one. When the producer closes the write end, `read()` returns 0 (EOF) and the consumer exits.

The program demonstrates how the pipe's built-in sleep/wakeup mechanism provides automatic flow control: if the consumer is slow, the producer blocks when the buffer fills; if the producer is slow, the consumer blocks on the empty buffer.

## Build & Run

This is an **xv6 user program**. It must be compiled inside the xv6 build system.

1. Copy the source file into the xv6 `user/` directory:
   ```
   cp lab3_producer_consumer.c /path/to/xv6-riscv/user/producer_consumer.c
   ```

2. Add the program to `UPROGS` in the xv6 `Makefile`:
   ```makefile
   UPROGS=\
       ...
       $U/_producer_consumer\
   ```

3. Build and launch xv6:
   ```
   make clean && make qemu
   ```

4. Run in the xv6 shell:
   ```
   $ producer_consumer
   ```

## What to Observe

- The producer sends 5 messages sequentially and the consumer receives them.
- Because `fork()` scheduling order is nondeterministic, `[Producer]` and `[Consumer]` messages may be interleaved. This is expected behavior.
- After the producer closes the write end, the consumer detects EOF and prints a closing message.
- Expected output (order may vary):
  ```
  [Producer] Sending 5 messages through pipe...
  [Producer] Sent: msg 0: hello from producer
  [Consumer] Reading from pipe...
  [Consumer] Received: msg 0: hello from producer
  ...
  [Consumer] Pipe closed by producer (read returned 0). Exiting.
  ```

## Experiments / Try It

1. **Increase `NUM_MESSAGES`**: Change it from 5 to 50 or 100. With more messages, you are more likely to see the producer block when the 512-byte pipe buffer fills up. Observe interleaved output as flow control kicks in.
2. **Add a delay in the consumer**: Insert `pause(5);` inside the consumer's read loop. This forces the pipe buffer to fill up, causing the producer to block. Verify that no messages are lost despite the delay.
3. **Add a delay in the producer**: Insert `pause(5);` inside the producer's write loop. The consumer will block on each `read()` waiting for the next message. Observe how the output timing changes.
4. **Send larger messages**: Modify `make_message()` to produce messages longer than 512 bytes (the pipe buffer size). What happens? Does the length-header protocol still work correctly?
5. **Multiple consumers**: Fork two child processes that both read from the same pipe. Which consumer gets which messages? Is there a guarantee about ordering?
6. **Remove `close(fds[1])` in the child**: If the consumer does not close the write end it inherited, what happens after the parent closes its write end? Does the consumer ever see EOF?

## Discussion Questions

1. Why does the consumer close `fds[1]` (the write end) before reading? What would happen if it did not?
2. The program uses a 1-byte length header before each message. Why is this necessary? What would go wrong with a simpler approach like reading a fixed number of bytes?
3. At what point in the kernel code does the consumer transition from SLEEPING to RUNNABLE when the producer writes data?
4. When the producer calls `close(fds[1])`, trace the kernel path that leads to the consumer's `read()` returning 0.
5. Could this program lose data if the producer writes faster than the consumer reads? Explain how the pipe's flow control prevents this.
