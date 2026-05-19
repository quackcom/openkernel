/* openkernel - Process Management Implementation */
#include "process.h"
#include "memory.h"
#include "kernel.h"
#include "gdt.h"
#include <stdint.h>
#include <stddef.h>

/* Process management data */
static pcb_t pcbs[MAX_PROCESSES];
static pcb_t* ready_queue = NULL;
static pcb_t* current_process = NULL;
static pid_t next_pid = 1;
static uint32_t process_count = 0;

/* External assembly function for context switching */
extern void context_switch(uint32_t* old_esp, uint32_t new_esp);

/* Forward declarations for internal scheduler functions */
void scheduler_init(void);
void scheduler_add_process(pcb_t* pcb);
void scheduler_remove_process(pid_t pid);
void scheduler_schedule(void);

/* Assembly function to get current ESP */
uint32_t asm_get_esp(void) {
    uint32_t esp;
    __asm__ volatile ("mov %%esp, %0" : "=r"(esp));
    return esp;
}

/* Kernel stack allocation - use 4KB per process */
#define PROCESS_STACK_SIZE 4096  /* 4KB per process */

/* Initialize a PCB */
static void pcb_init(pcb_t* pcb, pid_t pid, void (*entry)(void), uint32_t priority) {
    pcb->pid = pid;
    pcb->state = PROCESS_STATE_READY;
    pcb->esp = 0;
    pcb->ebp = 0;
    pcb->eip = (uint32_t)entry;
    pcb->cr3 = 0;  /* Shared address space for now */
    pcb->priority = priority;
    pcb->ticks_remaining = priority;
    pcb->next = NULL;

    /* Allocate kernel stack */
    pcb->kernel_stack = (uint32_t)kmalloc(PROCESS_STACK_SIZE);
    if (!pcb->kernel_stack) {
        printk("process: Failed to allocate kernel stack for PID %d\n", pid);
        return;
    }

    /* Initialize stack for first context switch */
    uint32_t* stack_top = (uint32_t*)(pcb->kernel_stack + PROCESS_STACK_SIZE);
    
    /* The context_switch assembly expects: EIP, EBP, EBX, ESI, EDI */
    *(--stack_top) = (uint32_t)entry; /* Return address for 'ret' */
    *(--stack_top) = 0x00000000;     /* Initial EBP */
    *(--stack_top) = 0x00000000;     /* Initial EBX */
    *(--stack_top) = 0x00000000;     /* Initial ESI */
    *(--stack_top) = 0x00000000;     /* Initial EDI */

    pcb->esp = (uint32_t)stack_top;
    pcb->ebp = pcb->esp;
}

/* Find a free PCB slot */
static pcb_t* find_free_pcb(void) {
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (pcbs[i].state == PROCESS_STATE_ZOMBIE || pcbs[i].state == 0) {
            return &pcbs[i];
        }
    }
    return NULL;
}

/* Add process to ready queue */
static void add_to_ready_queue(pcb_t* pcb) {
    pcb->state = PROCESS_STATE_READY;

    if (!ready_queue) {
        ready_queue = pcb;
        pcb->next = pcb;  /* Circular queue */
    } else {
        pcb->next = ready_queue->next;
        ready_queue->next = pcb;
    }
}

/* Remove process from ready queue */
static void remove_from_ready_queue(pcb_t* pcb) {
    if (!ready_queue) return;

    if (ready_queue == ready_queue->next) {
        /* Only one process in queue */
        ready_queue = NULL;
    } else {
        pcb_t* current = ready_queue;
        while (current->next != ready_queue) {
            if (current->next == pcb) {
                current->next = pcb->next;
                break;
            }
            current = current->next;
        }
        /* Check if we removed the ready_queue head */
        if (ready_queue == pcb) {
            ready_queue = current;
        }
    }
    pcb->next = NULL;
}

/* Initialize process management */
void process_init(void) {
    printk("Initializing process management...\n");

    /* Initialize all PCBs */
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        pcbs[i].state = PROCESS_STATE_ZOMBIE;
        pcbs[i].pid = 0;
        pcbs[i].next = NULL;
    }

    /* Create idle process (PID 0) */
    current_process = &pcbs[0];
    current_process->pid = 0;
    current_process->state = PROCESS_STATE_RUNNING;
    current_process->priority = 0;
    current_process->ticks_remaining = 1;
    current_process->kernel_stack = (uint32_t)kmalloc(PROCESS_STACK_SIZE);

    if (!current_process->kernel_stack) {
        printk("process: Failed to allocate kernel stack for idle process\n");
        // Try to allocate a small temporary stack as fallback
        current_process->kernel_stack = (uint32_t)kmalloc(1024);
        if (!current_process->kernel_stack) {
            // If that fails, try even smaller allocation
            current_process->kernel_stack = (uint32_t)kmalloc(256);
        }
        if (!current_process->kernel_stack) {
            printk("process: Failed to allocate kernel stack for idle process\n");
            return;
        }
    }

    process_count = 1;
    next_pid = 1;

    /* Initialize scheduler */
    scheduler_init();

    printk("Process management initialized. Idle process (PID 0) running.\n");
}

/* Create a new process */
pid_t process_create(void (*entry)(void), uint32_t priority) {
    if (process_count >= MAX_PROCESSES) {
        printk("process: Maximum process limit reached (%d)\n", MAX_PROCESSES);
        return 0;
    }

    pcb_t* pcb = find_free_pcb();
    if (!pcb) {
        printk("process: No free PCB slots available\n");
        return 0;
    }

    pid_t pid = next_pid++;
    pcb_init(pcb, pid, entry, priority);

    /* Add to scheduler */
    scheduler_add_process(pcb);
    process_count++;

    printk("process: Created process PID %d (priority %d)\n", pid, priority);
    return pid;
}

/* Terminate a process */
void process_terminate(pid_t pid) {
    if (pid == 0) {
        printk("process: Cannot terminate idle process (PID 0)\n");
        return;
    }

    if (pid >= next_pid) {
        printk("process: Invalid PID %d\n", pid);
        return;
    }

    pcb_t* pcb = &pcbs[pid];
    if (pcb->state == PROCESS_STATE_ZOMBIE) {
        printk("process: PID %d already terminated\n", pid);
        return;
    }

    /* Remove from scheduler */
    scheduler_remove_process(pid);

    /* Free resources */
    if (pcb->kernel_stack) {
        kfree((void*)pcb->kernel_stack);
        pcb->kernel_stack = 0;
    }

    pcb->state = PROCESS_STATE_ZOMBIE;
    process_count--;

    printk("process: Terminated PID %d\n", pid);
}

/* Yield CPU to scheduler */
void process_yield(void) {
    if (current_process->pid == 0) {
        /* Idle process doesn't yield */
        return;
    }

    current_process->state = PROCESS_STATE_READY;
    add_to_ready_queue(current_process);

    /* Schedule next process */
    scheduler_schedule();
}

/* Get current process ID */
pid_t process_get_current_pid(void) {
    return current_process ? current_process->pid : 0;
}

/* Get current PCB */
pcb_t* process_get_current_pcb(void) {
    return current_process;
}

/* Scheduler initialization */
void scheduler_init(void) {
    /* Round-robin scheduler initialization */
    printk("Scheduler: Round-robin initialized\n");
}

/* Add process to scheduler */
void scheduler_add_process(pcb_t* pcb) {
    if (!pcb) return;

    add_to_ready_queue(pcb);
    printk("scheduler: Added PID %d to ready queue\n", pcb->pid);
}

/* Remove process from scheduler */
void scheduler_remove_process(pid_t pid) {
    if (pid >= MAX_PROCESSES) return;

    pcb_t* pcb = &pcbs[pid];
    if (pcb->state != PROCESS_STATE_READY) return;

    remove_from_ready_queue(pcb);
    printk("scheduler: Removed PID %d from ready queue\n", pid);
}

/* Scheduler main function */
void scheduler_schedule(void) {
    /* Guard: process management not initialized yet */
    if (!current_process || process_count == 0) return;

    pcb_t* prev_process = current_process;

    if (!ready_queue) {
        /* No other processes ready. If we are already idle, just stay here. */
        if (prev_process->pid == 0) return;
        
        /* Otherwise, switch back to the idle process */
        current_process = &pcbs[0];
    } else {
        /* Pop the next process from the ready queue */
        current_process = ready_queue;
        ready_queue = ready_queue->next;
        if (ready_queue == current_process) ready_queue = NULL;
    }

    /* Fix: Allow the Idle process (PID 0) to be queued so it can run the 
       keyboard polling loop in kernel_main. Without this, kernel_main 
       only runs if no other tasks exist. */
    if (prev_process->state == PROCESS_STATE_RUNNING) {
        prev_process->state = PROCESS_STATE_READY;
        add_to_ready_queue(prev_process);
    }

    current_process->state = PROCESS_STATE_RUNNING;
    current_process->ticks_remaining = current_process->priority;

    /* Only perform context switch if the process actually changed */
    if (prev_process != current_process) {
        context_switch(&prev_process->esp, current_process->esp);
    }
}


/* Simple test process */
void test_process(void) {
    pid_t pid = process_get_current_pid();
    uint32_t count = 0;

    while (1) {
        printk("Process %d: Count %d\n", pid, count++);
        for (volatile int i = 0; i < 1000000; i++);  /* Delay */
        process_yield();
    }
}

/* ================================================================
 * Wait queues — blocking synchronization primitives
 * ================================================================ */

/* Block the current process on a wait queue.
 * The caller MUST have disabled interrupts.
 * The process is removed from the ready queue and placed on the
 * wait queue, then the scheduler picks the next runnable process. */
void process_block(wait_queue_t *wq) {
    if (!current_process) return;

    /* Idle process must never block */
    if (current_process->pid == 0) return;

    /* Move current process to blocked state */
    current_process->state = PROCESS_STATE_BLOCKED;

    /* Append to wait queue tail */
    current_process->next = NULL;
    if (wq->tail) {
        wq->tail->next = current_process;
    } else {
        wq->head = current_process;
    }
    wq->tail = current_process;

    /* Schedule next process (won't re-add current since state is BLOCKED) */
    scheduler_schedule();
}

/* Wake the first process on a wait queue.
 * The caller MUST have disabled interrupts. */
void process_wake(wait_queue_t *wq) {
    if (!wq->head) return;

    /* Dequeue first waiter */
    pcb_t *waiter = wq->head;
    wq->head = waiter->next;
    if (!wq->head) wq->tail = NULL;
    waiter->next = NULL;

    /* Move to ready queue */
    waiter->state = PROCESS_STATE_READY;
    add_to_ready_queue(waiter);
}

/* Wake all processes on a wait queue.
 * The caller MUST have disabled interrupts. */
void process_wake_all(wait_queue_t *wq) {
    while (wq->head) {
        process_wake(wq);
    }
}