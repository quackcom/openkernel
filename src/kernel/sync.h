/* openkernel - Synchronization Primitives */
/* Atomic ops, spinlocks, mutexes */
#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>

/* ================================================================
 * Atomic operations — lock-free on x86
 * ================================================================ */
typedef volatile int atomic_t;

static inline void atomic_set(atomic_t *a, int val) {
    *a = val;
}

static inline int atomic_get(atomic_t *a) {
    return *a;
}

/* Atomic exchange — xchg is implicitly locked on x86 */
static inline int atomic_xchg(atomic_t *a, int val) {
    int old;
    asm volatile("xchgl %0, %1"
                 : "=r"(old), "+m"(*a)
                 : "0"(val)
                 : "memory");
    return old;
}

/* Atomic compare-and-exchange — returns old value */
static inline int atomic_cmpxchg(atomic_t *a, int old, int new) {
    int ret;
    asm volatile("lock cmpxchgl %2, %1"
                 : "=a"(ret), "+m"(*a)
                 : "r"(new), "0"(old)
                 : "memory");
    return ret;
}

/* Atomic increment — returns the new value */
static inline int atomic_inc(atomic_t *a) {
    int result;
    asm volatile("lock xaddl %0, %1"
                 : "=r"(result), "+m"(*a)
                 : "0"(1)
                 : "memory");
    return result + 1;
}

/* Atomic decrement — returns the new value */
static inline int atomic_dec(atomic_t *a) {
    int result;
    asm volatile("lock xaddl %0, %1"
                 : "=r"(result), "+m"(*a)
                 : "0"(-1)
                 : "memory");
    return result - 1;
}

/* Atomic add — returns the new value */
static inline int atomic_add(atomic_t *a, int val) {
    int result;
    asm volatile("lock xaddl %0, %1"
                 : "=r"(result), "+m"(*a)
                 : "0"(val)
                 : "memory");
    return result + val;
}

/* Full memory barrier (mfence on x86, but a simpler locked op also works) */
static inline void mb(void) {
    asm volatile("mfence" ::: "memory");
}

/* Compiler barrier only */
static inline void barrier(void) {
    asm volatile("" ::: "memory");
}

/* ================================================================
 * Spinlocks — busy-wait with pause hint
 * ================================================================ */
typedef struct {
    atomic_t locked;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

static inline void spinlock_init(spinlock_t *lock) {
    lock->locked = 0;
}

static inline void spinlock_lock(spinlock_t *lock) {
    while (atomic_xchg(&lock->locked, 1) != 0) {
        /* Pause while contended — yields to hyper-thread sibling */
        while (atomic_get(&lock->locked)) {
            asm volatile("pause");
        }
    }
}

static inline void spinlock_unlock(spinlock_t *lock) {
    barrier();
    lock->locked = 0;
}

/* Try once — returns 1 on success, 0 if already locked */
static inline int spinlock_trylock(spinlock_t *lock) {
    return atomic_xchg(&lock->locked, 1) == 0;
}

/* Lock with interrupts disabled.  Returns saved EFLAGS for restore. */
static inline unsigned long spinlock_lock_irqsave(spinlock_t *lock) {
    unsigned long flags;
    asm volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    spinlock_lock(lock);
    return flags;
}

/* Unlock and restore interrupt state from spinlock_lock_irqsave. */
static inline void spinlock_unlock_irqrestore(spinlock_t *lock, unsigned long flags) {
    spinlock_unlock(lock);
    asm volatile("pushl %0; popfl" : : "r"(flags) : "memory");
}

/* ================================================================
 * Mutexes — blocking locks (sleep instead of spin)
 * Requires the process subsystem for wait queues.
 * ================================================================ */

/* Opaque — implementation in sync.c */
typedef struct mutex mutex_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);
int  mutex_trylock(mutex_t *m);   /* 1 on success, 0 if contended */

#endif /* SYNC_H */
