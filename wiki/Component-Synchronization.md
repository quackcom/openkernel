# Component: Synchronization

**Files:** `src/kernel/sync.c`, `src/kernel/sync.h`

## Purpose

Primitives for safe concurrent access in a preemptive kernel: **atomics**, **spinlocks**, and **blocking mutexes**.

## Atomic operations

Type `atomic_t` (volatile `int`).

| Op | Behavior |
|----|----------|
| `atomic_get` / `atomic_set` | Plain read/write |
| `atomic_xchg` | Exchange |
| `atomic_cmpxchg` | Compare-and-swap |
| `atomic_inc` / `dec` / `add` | Arithmetic |
| `mb()` | Full memory barrier |
| `barrier()` | Compiler barrier |

Implemented with inline `asm` where needed for true atomicity on x86.

## Spinlocks (`spinlock_t`)

Busy-wait lock with `pause` instruction in spin loop.

| Function | Notes |
|----------|-------|
| `spinlock_init` | `locked = 0` |
| `spinlock_lock` | Loop until acquire |
| `spinlock_trylock` | Non-blocking attempt |
| `spinlock_unlock` | Release |
| `spinlock_lock_irqsave` | Disable IRQs while holding |
| `spinlock_unlock_irqrestore` | Restore flags |

Use for **short** critical sections in ISR or when you cannot sleep.

## Mutexes (`mutex_t`)

**Blocking** — contending thread calls `process_block()` on internal wait queue.

| Function | Behavior |
|----------|----------|
| `mutex_init` | Clear lock + wait queue |
| `mutex_lock` | Acquire or block |
| `mutex_trylock` | Return 0 if busy |
| `mutex_unlock` | Release, wake one waiter |

Requires **interrupts enabled** and process manager initialized.

## When to use what

| Primitive | Best for |
|-----------|----------|
| Spinlock | ISR ↔ bottom half, very short globals |
| Mutex | Longer kernel paths, thread may block |
| Atomics | Counters, flags, lock-free algorithms |

## Related

- [[Component-Processes-and-Scheduler]] — wait queues  
- [[Roadmap]] — user-space futex-like syscalls later  
