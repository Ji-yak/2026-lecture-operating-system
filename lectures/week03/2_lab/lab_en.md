---
theme: default
title: "Week 03 — Lab: Exploring xv6 Internals"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 3 — Exploring xv6 Internals

Korea University Sejong Campus, Department of Computer Science

---

# Lab Overview

- **Goal**: Navigate xv6 kernel source and trace how system calls work end-to-end
- **Duration**: ~50 minutes · 5 labs (Lab 0–4)

```mermaid
graph LR
    L0["Lab 0<br/>Environment"] --> L1["Lab 1<br/>Source Structure"]
    L1 --> L2["Lab 2<br/>Syscall Tracing"]
    L2 --> L3["Lab 3<br/>struct proc"]
    L3 --> L4["Lab 4<br/>fork() Deep Dive"]
    style L0 fill:#e3f2fd
    style L2 fill:#fff3e0
    style L4 fill:#e8f5e9
```

---

# Lab 0 & 1: Environment & Source Structure

<div class="grid grid-cols-2 gap-4">
<div>

### Lab 0 — Verify Build

```bash
cd xv6-riscv
make qemu
# → "xv6 kernel is booting"
# → shell prompt ($)
# Exit: Ctrl-A X
```

</div>
<div>

### Lab 1 — Key Kernel Files

| File | Purpose |
|---|---|
| `proc.h` | `struct proc` definition |
| `proc.c` | fork, exit, wait, scheduler |
| `syscall.c` | syscall dispatch table |
| `sysproc.c` | syscall handlers |
| `trap.c` | trap entry from user space |
| `usys.pl` | generates user-space stubs |

</div>
</div>

---

# Lab 2: System Call Tracing

**Full path of `fork()` from user space to kernel:**

```mermaid
graph TD
    U["User program<br/>calls fork()"] --> US["usys.S<br/>li a7, SYS_fork<br/>ecall"]
    US --> TR["trap.c — usertrap()<br/>handles all user traps"]
    TR --> SC["syscall.c — syscall()<br/>reads a7, dispatch table lookup"]
    SC --> SP["sysproc.c — sys_fork()<br/>thin wrapper"]
    SP --> PC["proc.c — fork()<br/>does the actual work"]
    style U fill:#e3f2fd
    style US fill:#fff3e0
    style TR fill:#fce4ec
    style SC fill:#f3e5f5
    style SP fill:#e8f5e9
    style PC fill:#c8e6c9
```

**Exercise**: Add a `printf` in `sys_fork()` and rebuild to confirm you found the right spot.

---

# Lab 3: struct proc Analysis

**Process state machine** — defined in `kernel/proc.h`:

```mermaid
stateDiagram-v2
    [*] --> UNUSED
    UNUSED --> USED : allocproc()
    USED --> RUNNABLE : fork() / userinit()
    RUNNABLE --> RUNNING : scheduler picks it
    RUNNING --> RUNNABLE : yield() / timer interrupt
    RUNNING --> SLEEPING : sleep()
    SLEEPING --> RUNNABLE : wakeup()
    RUNNING --> ZOMBIE : exit()
    ZOMBIE --> UNUSED : wait() reaps
```

**Key fields**: `state`, `pid`, `pagetable`, `trapframe`, `context`, `ofile[]`, `parent`

- **Exercise**: Which fields change at each state transition?

---

# Lab 3: struct proc — Full Definition

```c
struct proc {
  struct spinlock lock;
  enum procstate state;        // UNUSED → USED → RUNNABLE → RUNNING → ZOMBIE
  void *chan;                  // sleep channel (if SLEEPING)
  int killed;                 // pending kill signal
  int xstate;                 // exit status for parent
  int pid;                    // process ID

  struct proc *parent;        // parent process (protected by wait_lock)

  uint64 kstack;              // kernel stack virtual address
  uint64 sz;                  // process memory size (bytes)
  pagetable_t pagetable;      // user page table
  struct trapframe *trapframe;// saved user registers (for trampoline.S)
  struct context context;     // saved kernel registers (for swtch.S)
  struct file *ofile[NOFILE]; // open file descriptors
  struct inode *cwd;          // current working directory
  char name[16];              // process name (for debugging)
};
```

---

# Lab 4: fork() Implementation Deep Dive

```mermaid
graph TD
    A["1. allocproc()<br/>get new proc slot, pid, kstack, trapframe"] --> B["2. uvmcopy()<br/>copy parent's page table + memory"]
    B --> C["3. Copy trapframe<br/>child returns from fork() too"]
    C --> D["4. Set a0 = 0<br/>child sees fork() return 0"]
    D --> E["5. Copy ofile[]<br/>share open file descriptors"]
    E --> F["6. Set parent, state = RUNNABLE<br/>child is ready to run"]
    F --> G["7. Return child PID to parent"]
    style A fill:#e3f2fd
    style D fill:#fff3e0
    style F fill:#e8f5e9
```

**Discussion questions**:
- Why does the child need its **own** trapframe copy?
- What would happen if step 4 (`a0 = 0`) were skipped?
- Why does `uvmcopy` copy **all** pages? (Hint: Week 12 — COW fork)

---

# Key Takeaways

| Concept | Key Insight |
|---|---|
| **xv6** | ~10K lines of C — small enough to read entirely |
| **Syscall path** | user → `ecall` → `usertrap()` → `syscall()` → handler → impl |
| **struct proc** | Kernel's complete view of a process (scheduling, memory, files) |
| **fork()** | `uvmcopy` = heavy lifting; `trapframe->a0 = 0` = child return value |

```mermaid
graph LR
    subgraph "What you explored today"
        SRC["Source<br/>Structure"] --> PATH["Syscall<br/>Path"]
        PATH --> PROC["struct proc<br/>Lifecycle"]
        PROC --> FORK["fork()<br/>Implementation"]
    end
```

> Upcoming: threads, scheduling, and synchronization — all built on top of what you explored today.
