# Week 05 — Thread & Concurrency 2

## Overview
- **Textbook**: Silberschatz Ch 7 (Synchronization Examples)
- **xv6 Reference**: Ch 6.6–6.10 (Locking advanced), Ch 9 (Concurrency Revisited)
- **Learning Objectives**:
  - Understand Semaphore, Monitor, and Condition Variable
  - Learn Classic Synchronization Problems
  - Implement Producer-Consumer and Bounded Buffer

## Class Schedule

### 1st Hour (Theory — Part 1) — 50 min
- [00:00–00:10] **Quiz** (10 min) — covers Week 4 material
- [00:10–00:15] Last week review / Announcements
- [00:15–00:50] Theory: Advanced Synchronization Tools
  - Semaphore (Counting, Binary)
  - Monitor, Condition Variable
  - Slides: `1_theory/05_thread_concurrency_2_en.md`

### 2nd Hour (Theory — Part 2) — 50 min
- [00:00–00:50] Theory: Classic Synchronization Problems
  - Producer-Consumer Problem
  - Readers-Writers Problem
  - Dining Philosophers Problem

### 3rd Hour (Lab) — 50 min
- [00:00–00:05] Lab objectives
- [00:05–00:40] Lab — Synchronization pattern implementation
  - producer_consumer.c: Condition Variable-based implementation
  - bounded_buffer.c: Bounded Buffer implementation
  - xv6 spinlock.c code analysis (acquire, release, push_off, pop_off)
  - Guide: `2_lab/README.md`
- [00:40–00:50] Homework briefing: xv6 kalloc Per-CPU free list + Barrier (bonus)

## Materials
- Theory: `1_theory/05_thread_concurrency_2_en.md`
- Lab: `2_lab/README.md`, `2_lab/examples/` (2 examples)
- Homework: `3_assignment/` — kalloc Per-CPU + barrier (skeleton + tests)
- Quiz: Yes (10 min, beginning of 1st hour)
