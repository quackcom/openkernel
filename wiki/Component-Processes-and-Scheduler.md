# Component: Processes and Scheduler

**Files:** `src/kernel/process.c`, `src/kernel/process.h`, `src/kernel/context_switch.asm`

## Purpose

Kernel threads with **preemptive round-robin** scheduling, wait queues for blocking, and cooperative `yield`.

## Process Control Block (`pcb_t`)

| Field | Purpose |
|-------|---------|
| `pid` | Process ID |
| `state` | READY / RUNNING / BLOCKED / ZOMBIE |
| `esp`, `ebp`, `eip` | Saved CPU context |
| `cr3` | Future per-process page directory |
| `kernel_stack` | 4 KB stack from `kmalloc` |
| `priority` | Time slice weight |
| `ticks_remaining` | Preemption counter |
| `next` | Ready-queue link |

Max **64** processes (`MAX_PROCESSES`).

## States

```text
ZOMBIE (unused slot)
   │
   ▼ create
READY ◄──────────────┐
   │ schedule         │ yield / timer
   ▼                  │
RUNNING ──────────────┘
   │
   ▼ process_block (mutex)
BLOCKED ──wake──► READY
```

## Scheduler

- **Algorithm:** round-robin circular ready queue  
- **`scheduler_schedule()`** — pick next READY process, `context_switch` if different  
- **Idle process (PID 0)** — runs shell loop; always re-queued when alone  
- **Timer:** every 10 ticks (~100 ms), IRQ path calls `scheduler_schedule()`  

## `context_switch.asm`

Saves on old stack: `edi, esi, ebx, ebp, eip` (return address).  
Loads from new stack and `ret` into new process.

Callee-saved convention matches stack layout built in `pcb_init()`.

## API

| Function | Description |
|----------|-------------|
| `process_init()` | Clear PCBs, create idle thread |
| `process_create(entry, priority)` | New kernel thread |
| `process_terminate(pid)` | Kill and free stack |
| `process_yield()` | Give up CPU |
| `process_get_current_pid()` | Current PID |
| `process_block(wq)` | Block on wait queue |
| `process_wake(wq)` | Wake one waiter |

## Wait queues

Used by **mutexes** in `sync.c`. Blocked processes are not on ready queue until `process_wake`.

## Current limitations

- Single address space — no ring 3, no `cr3` switch  
- Shell runs in idle context — creating heavy workloads may starve input if misconfigured  
- No `fork` / `exec`  

## Related

- [[Component-Synchronization]]  
- [[Component-CPU-GDT-IDT-ISR]] — timer IRQ  
- [[Roadmap]] — user mode plans  
