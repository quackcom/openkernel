/* openkernel - IDT (Interrupt Descriptor Table) */
#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* IDT entry structure (8 bytes) */
typedef struct {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed)) idt_entry_t;

/* IDT pointer structure (for LIDT instruction) */
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

/* Interrupt handler function type */
typedef void (*isr_t)(void);

/* Initialize IDT */
void idt_init(void);

/* Register an ISR handler */
void idt_register_handler(int irq, isr_t handler);

/* Set IDT entry directly (used by isr_setup.c) */
void idt_set_entry_raw(int num, uint32_t base, uint16_t selector, uint8_t flags);

/* Helper: enable/disable interrupts */
static inline void enable_interrupts(void) { asm volatile("sti"); }
static inline void disable_interrupts(void) { asm volatile("cli"); }

/* Forward declaration for register file structure */
struct regs_t;

/* ISR handler called from assembly */
void isr_handler(struct regs_t *r);

/* IRQ handler called from assembly */
void irq_handler(struct regs_t *r);

#endif /* IDT_H */