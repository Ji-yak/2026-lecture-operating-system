---
theme: default
title: "Week 09 — Lab: sleep/wakeup and Pipes"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 9 — sleep/wakeup and Pipes

Korea University Sejong Campus, Department of Computer Science

---

# Lab Overview

**Objectives**: Analyze xv6 synchronization primitives at the source-code level

| # | Topic | Time |
|---|-------|------|
| 1 | sleep/wakeup code analysis | 15 min |
| 2 | Pipe implementation analysis | 15 min |
| 3 | Producer-consumer with pipes | 15 min |
| 4 | Lost wakeup problem | 5 min |

```mermaid
graph LR
    E1["Ex 1<br/>sleep/wakeup"] --> E2["Ex 2<br/>Pipe internals"]
    E2 --> E3["Ex 3<br/>Producer-Consumer"]
    E3 --> E4["Ex 4<br/>Lost Wakeup"]
    style E1 fill:#e3f2fd
    style E2 fill:#fff3e0
    style E3 fill:#e8f5e9
    style E4 fill:#fce4ec
```

---

# Exercise 1: sleep/wakeup

**`sleep(chan, lk)`** — `kernel/proc.c`

```c
void sleep(void *chan, struct spinlock *lk) {
  struct proc *p = myproc();
  acquire(&p->lock);   // (1) acquire process lock
  release(lk);         // (2) release condition lock
  p->chan = chan;
  p->state = SLEEPING; // (3) mark as sleeping
  sched();             // (4) yield CPU to scheduler
  p->chan = 0;
  release(&p->lock);
  acquire(lk);         // (5) re-acquire condition lock
}
```

```mermaid
graph LR
    A["acquire p->lock"] --> B["release cond lock"]
    B --> C["state = SLEEPING"]
    C --> D["sched() → yield CPU"]
    D --> E["... woken up ..."]
    E --> F["re-acquire cond lock"]
    style C fill:#ffcdd2
    style E fill:#c8e6c9
```

- **Channel** = arbitrary address as event ID (e.g., `&pi->nread`)
- `wakeup(chan)` is a **broadcast**: all processes sleeping on `chan` become RUNNABLE

---

# Exercise 2: Pipe Implementation

**Circular buffer** of 512 bytes — `kernel/pipe.c`

<div class="grid grid-cols-2 gap-4">
<div>

```c
struct pipe {
  struct spinlock lock;
  char data[PIPESIZE]; // 512
  uint nread;          // monotonic
  uint nwrite;         // monotonic
  int readopen;
  int writeopen;
};
// index = nread % PIPESIZE
// full: nwrite == nread + PIPESIZE
```

</div>
<div>

```mermaid
graph TD
    subgraph "Pipe Buffer (circular)"
        direction LR
        W["nwrite"] --> |"data flows"| R["nread"]
    end
    WR["pipewrite()"] -->|"writes to buffer"| W
    RD["piperead()"] -->|"reads from buffer"| R
    WR -->|"buffer full?"| S1["sleep(&pi->nwrite)"]
    RD -->|"buffer empty?"| S2["sleep(&pi->nread)"]
    S1 -.->|"woken by reader"| WR
    S2 -.->|"woken by writer"| RD
    style S1 fill:#ffcdd2
    style S2 fill:#ffcdd2
```

</div>
</div>

**EOF**: when write end closes, `piperead` exits the wait loop and returns 0.

---

# Exercise 3: Producer-Consumer with Pipes

**Pattern**: parent = producer, child = consumer

```c
int fds[2];
pipe(fds);
int pid = fork();

if (pid == 0) {                  // Child — Consumer
    close(fds[1]);
    char buf[64]; int n;
    while ((n = read(fds[0], buf, sizeof(buf))) > 0)
        printf("[Consumer] %s\n", buf);
    close(fds[0]);
    exit(0);
} else {                         // Parent — Producer
    close(fds[0]);
    for (int i = 0; i < 5; i++)
        write(fds[1], "hello", 5);
    close(fds[1]);               // signals EOF
    wait(0);
}
```

```mermaid
sequenceDiagram
    participant P as Parent (Producer)
    participant Pipe as Pipe Buffer
    participant C as Child (Consumer)
    P->>Pipe: write("hello")
    Pipe->>C: wakeup → read("hello")
    P->>Pipe: write("hello")
    P->>Pipe: close(write end)
    Pipe->>C: read() returns 0 (EOF)
    C->>C: exit
    P->>P: wait() → reap child
```

---

# Exercise 4: Lost Wakeup Problem

**The problem**: wakeup arrives while the process is **not yet sleeping**

```mermaid
sequenceDiagram
    participant R as Reader (CPU 0)
    participant W as Writer (CPU 1)
    R->>R: check: buffer empty → true
    R->>R: release(pi->lock)
    W->>W: acquire(pi->lock)
    W->>W: write data to buffer
    W->>W: wakeup(&pi->nread)
    Note over R: NOT SLEEPING YET!<br/>wakeup is LOST
    R->>R: sleep(&pi->nread)
    Note over R: 💀 Sleeps forever
```

**xv6's fix**: pass the condition lock to `sleep()`

```c
// Inside sleep():
acquire(&p->lock);   // hold p->lock ...
release(lk);         // ... BEFORE releasing condition lock
p->state = SLEEPING; // state set while p->lock held
sched();
```

- Writer's `wakeup()` must acquire `p->lock` to see `p->state`
- Because `p->lock` is held until after `SLEEPING` is set → **no gap**
- "Check condition → go to sleep" is effectively **atomic**

---

# Key Takeaways

```mermaid
graph TD
    SW["sleep(chan, lk)"] -->|"releases lk atomically<br/>with entering SLEEPING"| SC["Scheduler"]
    WU["wakeup(chan)"] -->|"broadcasts to all<br/>sleeping on chan"| RN["→ RUNNABLE"]
    PI["Pipe"] -->|"uses sleep/wakeup<br/>for flow control"| PC["Producer-<br/>Consumer"]
    LW["Lost Wakeup"] -->|"prevented by<br/>lock overlap"| SW
    style SW fill:#e3f2fd
    style WU fill:#c8e6c9
    style LW fill:#ffcdd2
```

| Concept | Key Insight |
|---|---|
| **sleep/wakeup** | Channel = any address; always recheck condition after wakeup |
| **Pipe** | Circular buffer with automatic flow control (full→block writer, empty→block reader) |
| **Lost wakeup** | Fixed by holding `p->lock` across condition check + state change |
