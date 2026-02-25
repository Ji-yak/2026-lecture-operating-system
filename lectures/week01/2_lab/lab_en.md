---
theme: default
title: "Week 01 — Lab: Coding Agents"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 1 — Coding Agents

Korea University Sejong Campus, Department of Computer Science

---

# Lab Overview

- **Goal**: Get comfortable using AI-powered coding agents
- **Duration**: ~50 minutes
- **Submission**: None — exploration lab
- **5 Tasks**: Install → File Org → Repo Docs → RALPH → Mini OS

```mermaid
graph LR
    T1["🛠️ Task 1<br/>Install Agent"] --> T2["📁 Task 2<br/>File Organization"]
    T2 --> T3["📝 Task 3<br/>Repo Documentation"]
    T3 --> T4["🔄 Task 4<br/>RALPH Technique"]
    T4 --> T5["💻 Task 5<br/>Build Mini OS"]
```

---

# What Are Coding Agents?

AI-powered CLI tools that understand and generate code **in context**

```mermaid
graph LR
    A["📖 Read<br/>files & context"] --> B["🧠 Plan<br/>what to do"]
    B --> C["⚡ Act<br/>edit & run"]
    C --> D["✅ Verify<br/>check results"]
    D -->|iterate| A
    style A fill:#e3f2fd
    style B fill:#fff3e0
    style C fill:#e8f5e9
    style D fill:#fce4ec
```

- Can read your file system, run commands, and make edits autonomously
- Useful for: scaffolding, refactoring, documentation, debugging

> **Example**: _"Create a Makefile for a C project with debug and release targets"_
> Agent: reads directory → writes Makefile → confirms build succeeds

---

# Available Agents

<div class="grid grid-cols-2 gap-4">
<div>

### Free Options
| Agent | Notes |
|---|---|
| **Gemini CLI** | Google's agent, good default |
| **OpenCode** | Open-source, self-hostable |

</div>
<div>

### Paid Options
| Agent | Notes |
|---|---|
| **Claude Code** | Strong multi-file reasoning |
| **GPT Codex** | OpenAI's coding agent |

</div>
</div>

<br/>

> **Recommendation**: Install **Gemini CLI** if you have no preference — it's free and easy to set up.

---

# Task 1 & 2: Install & File Organization

<div class="grid grid-cols-2 gap-6">
<div>

### Task 1 — Install a Coding Agent

1. Follow the official install guide
2. Verify: `gemini --version`
3. Authenticate with your account

</div>
<div>

### Task 2 — File Organization

Point the agent at a messy directory:

```
"Organize the files in ./downloads
 into subfolders by type and date"
```

Review — did the agent make sensible decisions?

</div>
</div>

---

# Task 3 & 4: Repo Docs & RALPH

<div class="grid grid-cols-2 gap-6">
<div>

### Task 3 — GitHub Repo Documentation

- Open an existing GitHub repository
- Ask the agent to generate a `README.md`
- Check: does it accurately describe the project?

</div>
<div>

### Task 4 — RALPH Technique

```mermaid
graph TD
    R["1️⃣ Request<br/>initial prompt"] --> A["2️⃣ Analyze<br/>review output"]
    A --> L["3️⃣ List issues<br/>find gaps"]
    L --> P["4️⃣ Prompt again<br/>targeted fix"]
    P --> H["5️⃣ Harmonize<br/>satisfactory?"]
    H -->|No| R
    H -->|Yes| Done["✅ Done"]
    style Done fill:#c8e6c9
```

Practice this loop on the README from Task 3.

</div>
</div>

---

# Task 5: Build a Mini OS

Ask your coding agent to scaffold a minimal OS kernel in C:

```
"Create a minimal x86 OS kernel in C that boots with GRUB,
 prints 'Hello, OS!' to the screen, and halts.
 Include a Makefile and linker script."
```

```mermaid
graph LR
    A["boot.asm<br/>(bootloader)"] --> B["kernel.c<br/>(main kernel)"]
    B --> C["linker.ld<br/>(memory layout)"]
    C --> D["Makefile<br/>(build system)"]
    style A fill:#e3f2fd
    style B fill:#fff3e0
    style C fill:#e8f5e9
    style D fill:#fce4ec
```

- Observe how the agent breaks the problem into files
- You do **not** need to successfully boot it — the **process** matters

---

# Summary & Next Steps

**What we practiced today**

| Task | Skill |
|---|---|
| Install agent | Tool setup & authentication |
| File organization | Delegating tasks to AI |
| Repo documentation | Evaluating AI-generated output |
| RALPH technique | Iterative refinement loop |
| Mini OS | Tackling complex systems projects with AI |

<br/>

**Coming up — Week 2 Lab**: Process system calls (`fork`, `exec`, `wait`, `pipe`)

> Coding agents are tools — understanding what they produce is still **your** responsibility.
