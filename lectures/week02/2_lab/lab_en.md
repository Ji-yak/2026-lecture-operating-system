---
theme: default
title: "Week 02 — Lab: Process System Calls"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 2 — Process System Calls

Korea University Sejong Campus, Department of Computer Science

---

# Lab Overview

- **Goal**: Practice core UNIX process system calls through C programs
- **Duration**: ~50 minutes · 4 labs
- **Topics**: `fork()`, `exec()`, `wait()`, `pipe()`, `dup()`

```mermaid
graph LR
    F["fork()<br/>duplicate process"] --> E["exec()<br/>replace image"]
    F --> W["wait()<br/>reap child"]
    F --> P["pipe()<br/>IPC channel"]
    P --> D["dup2()<br/>I/O redirect"]
    style F fill:#e3f2fd
    style E fill:#fff3e0
    style W fill:#e8f5e9
    style P fill:#fce4ec
    style D fill:#f3e5f5
```

---

# Lab 1: fork() and wait()

`fork()` duplicates the calling process — the child gets a **copy** of the parent's address space.

<div class="grid grid-cols-2 gap-4">
<div>

```c
pid_t pid = fork();
if (pid == 0) {
    // Child process
    printf("Child PID: %d\n", getpid());
} else {
    // Parent process
    wait(NULL);
    printf("Parent: child finished\n");
}
```

</div>
<div>

```mermaid
graph TD
    P["Parent<br/>pid = fork()"] -->|"returns child_pid"| PW["Parent<br/>wait()"]
    P -->|"returns 0"| C["Child<br/>runs independently"]
    C --> CE["Child exits"]
    CE --> PW
    PW --> PD["Parent continues"]
    style P fill:#e3f2fd
    style C fill:#fff3e0
```

</div>
</div>

- `fork()` returns **0** to child, **child PID** to parent
- `wait(NULL)` blocks until any child terminates
- **Try**: what happens if you remove `wait()`?

---

# Lab 2: exec()

`exec()` replaces the current process image with a new program. The PID does **not** change.

<div class="grid grid-cols-2 gap-4">
<div>

```c
printf("Before exec\n");

char *args[] = {"ls", "-l", NULL};
execvp("ls", args);

// Never reached if exec succeeds
printf("After exec\n");
```

</div>
<div>

```mermaid
graph TD
    A["Process (my_program)<br/>PID = 42"] -->|"execvp()"| B["Process (ls)<br/>PID = 42"]
    B --> C["ls output..."]
    style A fill:#fce4ec
    style B fill:#e8f5e9
```

**Common pattern**: `fork()` + `exec()`

```mermaid
graph TD
    P["Parent"] -->|fork| C["Child"]
    C -->|exec| N["New Program"]
    P -->|wait| W["Reap child"]
```

</div>
</div>

---

# Lab 3: pipe()

`pipe()` creates a **unidirectional** communication channel between two file descriptors.

<div class="grid grid-cols-2 gap-4">
<div>

```c
int fd[2];
pipe(fd);  // fd[0]=read, fd[1]=write

if (fork() == 0) {
    close(fd[0]);  // close read end
    char *msg = "hello from child";
    write(fd[1], msg, strlen(msg));
    close(fd[1]);
} else {
    close(fd[1]);  // close write end
    char buf[64] = {0};
    read(fd[0], buf, sizeof(buf));
    printf("Received: %s\n", buf);
    close(fd[0]);
}
```

</div>
<div>

```mermaid
graph LR
    subgraph "Child (Producer)"
        CW["write(fd[1])"]
    end
    subgraph "Pipe"
        BUF["kernel buffer"]
    end
    subgraph "Parent (Consumer)"
        PR["read(fd[0])"]
    end
    CW --> BUF --> PR
    style BUF fill:#fff3e0
```

**Rules**:
- Always close the **unused** end
- Models **producer-consumer** pattern
- Closing write end → reader gets EOF

</div>
</div>

---

# Lab 4: dup2() and I/O Redirection

`dup2(oldfd, newfd)` makes `newfd` refer to the same file as `oldfd` — used by shells for redirection.

<div class="grid grid-cols-2 gap-4">
<div>

```c
int fd = open("output.txt",
    O_WRONLY | O_CREAT | O_TRUNC, 0644);
dup2(fd, STDOUT_FILENO);
close(fd);

// This goes to output.txt
printf("Redirected!\n");
```

</div>
<div>

**How `ls > file.txt` works:**

```mermaid
graph TD
    S["Shell"] -->|"1. fork()"| C["Child"]
    C -->|"2. open('file.txt')"| FD["fd = 3"]
    FD -->|"3. dup2(3, 1)"| R["stdout → file.txt"]
    R -->|"4. exec('ls')"| LS["ls writes to stdout<br/>= file.txt"]
    style S fill:#e3f2fd
    style LS fill:#e8f5e9
```

**Exercise**: Combine `fork()` + `pipe()` + `dup2()` to implement `ls | wc -l`

</div>
</div>

---

# Key Takeaways

```mermaid
graph TD
    subgraph "Process Lifecycle"
        F["fork()"] --> E["exec()"]
        F --> W["wait()"]
        E --> X["exit()"]
        X --> W
    end
    subgraph "IPC & Redirection"
        P["pipe()"] --> D["dup2()"]
        D --> SH["Shell pipelines"]
    end
    style F fill:#e3f2fd
    style P fill:#fce4ec
```

| Syscall | Purpose |
|---|---|
| `fork()` | Create child process (copy of parent) |
| `exec()` | Replace process image with new program |
| `wait()` | Block until child exits; reap zombie |
| `pipe()` | Create one-way IPC channel |
| `dup2()` | Redirect file descriptors |

**Shell command** `cmd1 | cmd2` = fork + pipe + dup2 + exec (×2) + wait

> Next week: we go inside xv6 and read the kernel source that implements these calls.
