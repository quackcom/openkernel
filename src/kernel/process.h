/* openkernel - Process Management Header */
#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>

/* Process states */
#define PROCESS_STATE_READY    0
#define PROCESS_STATE_RUNNING  1
#define PROCESS_STATE_BLOCKED  2
#define PROCESS_STATE_ZOMBIE   3

/* Maximum number of processes */
#define MAX_PROCESSES 64

/* Process ID type */
typedef uint32_t pid_t;

/* Process Control Block (PCB) */
typedef struct pcb {
    pid_t pid;                    /* Process ID */
    uint32_t state;               /* Process state */
    uint32_t esp;                /* Stack pointer */
    uint32_t ebp;                /* Base pointer */
    uint32_t eip;                /* Instruction pointer */
    uint32_t cr3;                /* Page directory (for future per-process address spaces) */
    uint32_t kernel_stack;       /* Kernel stack address */
    uint32_t priority;           /* Process priority */
    uint32_t ticks_remaining;    /* Time slices remaining */
    struct pcb* next;           /* Next PCB in queue */
} pcb_t;

/* Initialize process management */
void process_init(void);

/* Create a new process */
pid_t process_create(void (*entry)(void), uint32_t priority);

/* Terminate a process */
void process_terminate(pid_t pid);

/* Yield CPU to scheduler */
void process_yield(void);

/* Get current process ID */
pid_t process_get_current_pid(void);

/* Get current PCB */
pcb_t* process_get_current_pcb(void);

/* Scheduler functions */
void scheduler_init(void);
void scheduler_add_process(pcb_t* pcb);
void scheduler_remove_process(pid_t pid);
void scheduler_schedule(void);

/* Context switching */
void context_switch(uint32_t* prev_esp, uint32_t next_esp);

/* Process entry point wrapper */
void process_wrapper(void);

#endif /* PROCESS_H */