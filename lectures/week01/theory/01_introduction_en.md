---
theme: default
title: "Week 1 — Introduction"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems

Week 1 — Introduction

Korea University Sejong Campus, Department of Computer Science

---
layout: section
---

# Part 1. OT

---

# Instructor

**Unggi Lee**, codingchild@korea.ac.kr

- Assistant Professor, Dept. of Computer Science, Korea University Sejong Campus
- Previously:
  - Assistant Professor, Dept. of Computer Engineering, Chosun University
  - AI/NLP Engineer in Global EdTech (Enuma, I-Scream Edu)
  - Elementary School Teacher
- Research ([Google Scholar](https://scholar.google.co.kr/citations?user=xnsGrp0AAAAJ)):
  - AIED: Generative AI in Education, Pedagogical Alignment, Knowledge Tracing
  - NLP & Robotics: Large Language Models (LLMs), Vision-Language-Action (VLA)
- Lab Activities ([Lab](https://codingchild2424.github.io/lab-website/)):
  - Published a VLA preprint with undergraduates at Chosun University
  - Industry–academia partnerships with Upstage, Newdive, and others
  - Collaborations with researchers in the US, Singapore, and the UK

---

# Syllabus

- You can find the syllabus in **LMS**
- Total **15 weeks**
  - 8th week — **Midterm Exam**
  - 15th week — **Final Exam**

---

# Grading

| Component | Weight |
|-----------|--------|
| Assignment | **10%** |
| Midterm Exam (written) | **30%** |
| Final Exam (written) | **30%** |
| Final Exam (project) | **30%** |
| Attendance | 0% |

> However, absent for **one-third (1/3)** or more of the total class hours → no grade will be awarded.

---

# Assignment (10%)

**In-class Quiz: 5%**
- Quizzes in **10 classes** → each **0.5%**
- Week 3, 4, 5, 6, 7, 9, 10, 11, 12, 13

**Take-home Assignment: 5%**
- Assignments in **5 classes** → each **1%**
- Week 2, 3, 4, 5, 6

---

# Midterm & Final Exam (Written)

- **Handwritten** (digital devices not allowed)
- **1 hour** each

---

# Final Exam — Project (30%)

- Starting from **Week 9**
- **3–4 members** per team
- Unlimited use of **coding agents**
  - Claude Code, Codex, Gemini CLI, OpenCode, etc.
- You must:
  - **Design and develop** an OS prototype
  - Write **OS spec documentation**
  - Give a **presentation** (in-class, Week 14)
- Evaluation:
  - Professor: **15%**
  - Students (peer review): **15%**

---

# Class Format

| Hour | Content |
|------|---------|
| **1st** | Theory Lecture (Part 1) |
| **2nd** | Theory Lecture (Part 2) + Quiz |
| **3rd** | Hands-on Lab |

- Textbook: Silberschatz, **Operating System Concepts** 10th Edition
- Lab reference: **xv6** (RISC-V), MIT 6.1810

---
layout: section
---

# Part 2. What is an Operating System?

A Big-Picture Overview

---

# What is an Operating System?

- The **one program running at all times** on the computer = **Kernel**
- Everything else is either a **system program** or an **application program**

<br>

**OS is like a government:**
> It does not perform useful functions by itself, but provides an **environment** in which other programs can do useful work.

---

# Where the OS Sits

```
┌─────────────────────────────────────┐
│              User                   │
├─────────────────────────────────────┤
│  Application Programs              │
│  (browser, editor, games, ...)     │
├─────────────────────────────────────┤
│  Operating System (Kernel)         │
│  - Process mgmt  - Memory mgmt    │
│  - File system   - I/O system     │
│  - Networking    - Security        │
├─────────────────────────────────────┤
│  Hardware                          │
│  (CPU, RAM, Disk, NIC, GPU, ...)   │
└─────────────────────────────────────┘
```

---

# Two Roles of the OS

**Resource Allocator**
- Manages CPU time, memory space, storage, I/O devices
- Distributes conflicting resource requests **efficiently and fairly**

**Control Program**
- Controls execution of user programs
- Prevents errors and improper use of the computer

---

# Key OS Concepts — Overview

| Concept | What it does | We cover in... |
|---------|-------------|----------------|
| **Process** | A program in execution | Week 2–3 |
| **Thread** | Lightweight unit of execution within a process | Week 4–5 |
| **CPU Scheduling** | Decides which process runs next | Week 6–7 |
| **Synchronization** | Coordinates concurrent access to shared resources | Week 9 |
| **Deadlock** | Circular wait among processes | Week 10 |
| **Memory Management** | Maps virtual addresses to physical memory | Week 11–12 |
| **File System** | Organizes data on persistent storage | Week 13 |
| **Security** | Protects resources from unauthorized access | Week 14 |

---

# How Does the OS Protect Itself?

**Dual-Mode Operation**

| Mode | Mode bit | Who runs here |
|------|----------|--------------|
| **Kernel mode** | 0 | OS kernel — full hardware access |
| **User mode** | 1 | Applications — restricted access |

- **System call**: The only way for a user program to request OS services
- User program → **trap** → kernel mode → service → return to user mode
- **Privileged instructions** (I/O, timer, interrupts) only run in kernel mode

---

# System Calls — The OS API

```
User Program
    │
    │  read(fd, buf, n)
    ▼
C Library (libc)
    │
    │  syscall instruction (trap)
    ▼
┌────────────────────┐
│   Kernel           │
│   → sys_read()     │
│   → disk I/O       │
│   → copy to buf    │
└────────────────────┘
    │
    ▼
Return to user program
```

- Even a simple `cp in.txt out.txt` triggers **thousands** of system calls

---

# How a Computer System Works

```
        CPU            Memory
        ┌──┐          ┌──────┐
        │  │◄────────►│      │
        └──┘          │      │
         ▲            │      │
         │ interrupt  │      │
        ┌──┐          │      │
        │  │ Device    │      │
        │  │ Controller│      │
        └──┘          └──────┘
         ▲
         │
      [ Disk, USB, NIC, ... ]
```

- CPU + device controllers connected via **bus**, compete for **shared memory**
- Devices signal completion via **interrupts**
- **DMA**: Bulk data transfer without CPU intervention

---

# Storage Hierarchy

```
        ▲  Faster / Smaller / Expensive
        │
   Registers        < 1 KB     ~0.3 ns
   Cache (L1/L2)    < 16 MB    ~1-25 ns
   Main Memory      < 64 GB    ~100 ns
   SSD              < 1 TB     ~50 μs
   HDD              < 10 TB    ~5 ms
        │
        ▼  Slower / Larger / Cheap
```

- **Caching**: Copy frequently used data to faster storage
- Registers ↔ Cache: managed by **hardware**
- Main memory and below: managed by **OS**

---

# OS Structures

| Structure | Key Idea | Example |
|-----------|----------|---------|
| **Monolithic** | Everything in one kernel binary | Linux, traditional UNIX |
| **Microkernel** | Minimal kernel + user-space services | Mach, QNX |
| **Hybrid** | Mix of both | macOS (Mach + BSD), Windows |
| **Loadable Modules** | Core kernel + dynamic modules | Linux (LKM) |

Most modern OSes are **hybrid** — pragmatic, not pure.

---

# What We'll Use: xv6

- **xv6**: A simple, Unix-like teaching OS by MIT
- Written in **C** for **RISC-V** architecture
- ~10,000 lines of code — small enough to read entirely
- Implements: processes, virtual memory, file system, shell
- We'll **read, modify, and extend** xv6 throughout the semester

```bash
git clone https://github.com/mit-pdos/xv6-riscv
cd xv6-riscv
make qemu    # Boot xv6 in QEMU emulator
```

---

# Course Roadmap

| Week | Topic | Week | Topic |
|------|-------|------|-------|
| **1** | Introduction + Coding Agents | **9** | Synchronization |
| **2–3** | Process | **10** | Deadlocks |
| **4–5** | Thread & Concurrency | **11–12** | Memory Management |
| **6–7** | CPU Scheduling | **13** | File System |
| **8** | _Midterm Exam_ | **14** | Security + Project Presentation |
| | | **15** | _Final Exam (Written)_ |

---

# Summary

- **OS = Kernel**: The always-running program that manages hardware resources
- **Dual-mode** (user / kernel) protects the OS from user programs
- **System calls** are the interface between applications and the kernel
- Key topics: **Process, Thread, Scheduling, Sync, Memory, File System**
- We'll use **xv6** to see how a real OS works inside
- Next week: **Process** — fork, exec, wait, pipe

---

# Q & A

codingchild@korea.ac.kr
