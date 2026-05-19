/* openkernel - Synchronization Primitives Implementation */
#include "sync.h"
#include "process.h"
#include "kernel.h"

/* Mutex structure */
struct mutex {
    volatile int locked;
    wait_queue_t waiters;
};

void mutex_init(mutex_t *m) {
    m->locked = 0;
    wait_queue_init(&m->waiters);
}

void mutex_lock(mutex_t *m) {
    unsigned long flags;

    /* Save interrupt state and disable interrupts */
    asm volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");

    /* Try to acquire — spin a few times before blocking to avoid
     * unnecessary context switches for short-held locks. */
    for (int spin = 0; ; spin++) {
        if (!m->locked) {
            if (atomic_xchg((atomic_t *)&m->locked, 1) == 0) {
                /* Acquired — restore interrupts and return */
                asm volatile("pushl %0; popfl" : : "r"(flags) : "memory");
                return;
            }
        }

        if (spin < 8) {
            /* Spin a bit — the holder might release soon */
            asm volatile("pause");
        } else {
            /* Lock still held — block on wait queue */
            process_block(&m->waiters);
            /* Woken up — interrupts are still disabled, retry */
            spin = 0;
        }
    }
}

void mutex_unlock(mutex_t *m) {
    unsigned long flags;

    asm volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");

    barrier();
    m->locked = 0;
    barrier();

    /* Wake one waiter (if any) */
    process_wake(&m->waiters);

    asm volatile("pushl %0; popfl" : : "r"(flags) : "memory");
}

int mutex_trylock(mutex_t *m) {
    unsigned long flags;
    int acquired;

    asm volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");

    acquired = (atomic_xchg((atomic_t *)&m->locked, 1) == 0);

    asm volatile("pushl %0; popfl" : : "r"(flags) : "memory");
    return acquired;
}
