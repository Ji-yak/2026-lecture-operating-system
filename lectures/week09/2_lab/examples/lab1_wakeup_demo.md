# Lab 1: sleep/wakeup Demo via Pipe Blocking

## Description

This xv6 user program demonstrates the kernel's `sleep()` and `wakeup()` mechanisms by observing pipe blocking behavior. It contains three demos:

1. **Demo 1 - Reader blocks until Writer sends data**: A reader calls `read()` on an empty pipe and blocks (sleeps on `&pi->nread`). After a delay, the writer sends data, triggering `wakeup(&pi->nread)` to wake the reader.
2. **Demo 2 - Writer blocks when pipe buffer is full**: A writer attempts to write 600 bytes into a pipe with a 512-byte buffer (`PIPESIZE`). When the buffer fills, the writer sleeps on `&pi->nwrite` until the reader consumes data and calls `wakeup(&pi->nwrite)`.
3. **Demo 3 - Pipe close triggers EOF**: The writer sends one message then closes the write end. The reader observes that `read()` returns 0 (EOF) because `pi->writeopen` becomes 0.

## Build & Run

This is an **xv6 user program**. It must be compiled inside the xv6 build system.

1. Copy the source file into the xv6 `user/` directory:
   ```
   cp lab1_wakeup_demo.c /path/to/xv6-riscv/user/wakeup_demo.c
   ```

2. Add the program to `UPROGS` in the xv6 `Makefile`:
   ```makefile
   UPROGS=\
       ...
       $U/_wakeup_demo\
   ```

3. Build and launch xv6:
   ```
   make clean && make qemu
   ```

4. Run in the xv6 shell:
   ```
   $ wakeup_demo
   ```

## What to Observe

- **Demo 1**: The `[Reader] Calling read()...` message appears first. After a 10-tick pause, the writer sends data and the reader wakes up and prints the received string. This shows blocking `read()` and wakeup in action.
- **Demo 2**: The writer tries to write 600 bytes but the pipe buffer is only 512 bytes. The writer blocks until the reader (after a 10-tick delay) starts consuming data. Both sides eventually complete, demonstrating flow control.
- **Demo 3**: The reader receives one message, then calls `read()` again. After the writer closes the pipe, `read()` returns 0 (EOF). This shows how `pipeclose()` sets `writeopen = 0` and wakes the reader.

## Experiments / Try It

1. **Change the delay**: Modify the `pause(10)` values to `pause(50)` or `pause(1)`. Observe how timing affects the interleaving of output messages.
2. **Increase the write size in Demo 2**: Change the 600-byte write to 2000 bytes. How many times does the writer need to block and wake up?
3. **Remove the `close(fds[1])` in the reader (Demo 3)**: If the child does not close the write end it inherited from `fork()`, what happens when it calls `read()` after the parent closes its write end? (Hint: `writeopen` will still be 1 because the child's copy is open.)
4. **Add a Demo 4**: Write a variant where two readers compete on the same pipe. Observe the "thundering herd" effect -- both readers wake up but only one gets the data.

## Discussion Questions

1. In Demo 1, what kernel function is called when the reader blocks on the empty pipe? What channel does it sleep on?
2. In Demo 2, why does the writer call `wakeup(&pi->nread)` before calling `sleep(&pi->nwrite, &pi->lock)` when the buffer is full?
3. In Demo 3, trace the kernel-level sequence from `close(fds[1])` to the reader's `read()` returning 0. Which functions are involved?
4. What would happen if xv6 used a `broken_sleep()` (without the lock parameter) in `piperead()`? Describe a specific interleaving that leads to a lost wakeup.
5. The `pause()` calls are used to control scheduling order. Is this approach reliable on a multi-CPU system? Why or why not?
