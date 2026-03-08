# Week 04 — Thread & Concurrency 1

## Overview
- **Textbook**: Silberschatz Ch 4 (Threads & Concurrency), Ch 6 (Synchronization Tools) basics
- **xv6 Reference**: Ch 5 (Interrupts), Ch 6.1–6.5 (Locking basics)
- **Learning Objectives**:
  - Understand the Thread concept and Multicore Programming
  - Learn Race Condition, Critical Section, Mutex, Spinlock
  - Observe race conditions and fix them with locks

## Class Schedule

### 1st Hour (Theory — Part 1) — 50 min
- [00:00–00:10] **Quiz** (10 min) — covers Week 3 material
- [00:10–00:15] Last week review / Announcements
- [00:15–00:50] Theory: Thread Concepts
  - Thread vs Process, Multicore Programming
  - Threading Models (Many-to-One, One-to-One, Many-to-Many)
  - Thread Libraries (Pthreads, Windows Threads, Java Threads)
  - Slides: `1_theory/04_thread_concurrency_1_en.md`

### 2nd Hour (Theory — Part 2) — 50 min
- [00:00–00:50] Theory: Synchronization Basics
  - Race Condition, Critical-Section Problem
  - Peterson's Solution, Hardware Support
  - Mutex Locks, Spinlock

### 3rd Hour (Lab) — 50 min
- [00:00–00:05] Lab objectives
- [00:05–00:40] Lab — Race condition & Lock hands-on
  - race_demo.c: Observe incorrect results from concurrent shared variable modification
  - mutex_fix.c: Fix with Mutex protection
  - spinlock_impl.c: Implement Spinlock (atomic operations)
  - deadlock_demo.c: Deadlock scenario
  - Guide: `2_lab/README.md`
- [00:40–00:50] Homework briefing: Parallel hash table (coarse → fine-grained locking)

## Materials
- Theory: `1_theory/04_thread_concurrency_1_en.md`
- Lab: `2_lab/README.md`, `2_lab/examples/` (4 demos)
- Homework: `3_assignment/` — Parallel hash table (skeleton + tests)
- Quiz: Yes (10 min, beginning of 1st hour)
