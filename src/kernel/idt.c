/* openkernel - IDT Implementation */
#include "idt.h"
#include "kernel.h"

/* IDT entries and pointer */
static idt_entry_t idt_entries[256];
static idt_ptr_t   idt_ptr;

/* Array of registered IRQ handlers (for IRQs 0-15) */
static isr_t irq_handlers[16];

/* Set an IDT entry (non-static so isr_setup.c can call it) */
void idt_set_entry_raw(int num, uint32_t base, uint16_t selector, uint8_t flags)
{
    idt_entries[num].base_low  = base & 0xFFFF;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].selector  = selector;
    idt_entries[num].always0   = 0;
    idt_entries[num].flags     = flags;
}

/* PIC I/O ports */
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI   0x20
/* PIC remap constants */
#define ICW1_ICW4     0x11
#define ICW1_INIT     0x10
#define ICW4_8086     0x01
#define PIC_OFFSET_1  0x20  /* Remap IRQ 0-7 to INT 0x20-0x27 */
#define PIC_OFFSET_2  0x28  /* Remap IRQ 8-15 to INT 0x28-0x2F */

/* Externals from io.asm */
extern void io_outb(uint16_t port, uint8_t data);
extern uint8_t io_inb(uint16_t port);

/* External from kernel.c */
extern void halt(void);

/* Remap the PIC */
static void pic_remap(void)
{
    /* ICW1: Init + ICW4 */
    io_outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    io_outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);

    /* ICW2: Vector offsets */
    io_outb(PIC1_DATA, PIC_OFFSET_1);
    io_outb(PIC2_DATA, PIC_OFFSET_2);

    /* ICW3: Tell master PIC there's a slave at IRQ2 */
    io_outb(PIC1_DATA, 0x04);
    io_outb(PIC2_DATA, 0x02);

    /* ICW4: 8086 mode */
    io_outb(PIC1_DATA, ICW4_8086);
    io_outb(PIC2_DATA, ICW4_8086);

    /* Unmask Timer (IRQ0) and Keyboard (IRQ1) explicitly */
    io_outb(PIC1_DATA, 0xFC);
    io_outb(PIC2_DATA, 0xFF);
}

/* Send End-of-Interrupt signal to PIC */
void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8) {
        io_outb(PIC2_CMD, PIC_EOI);
    }
    io_outb(PIC1_CMD, PIC_EOI);
}

/* Initialize IDT */
void idt_init(void)
{
    /* Setup IDT pointer */
    idt_ptr.limit = sizeof(idt_entries) - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    /* Clear all IDT entries */
    for (int i = 0; i < 256; i++) {
        idt_set_entry_raw(i, 0, 0, 0);
    }

    /* Clear IRQ handlers */
    for (int i = 0; i < 16; i++) {
        irq_handlers[i] = 0;
    }

    /* Remap the PIC */
    pic_remap();

    /* Load the IDT */
    asm volatile ("lidt %0" : : "m" (idt_ptr));
}

/* Register a handler for an IRQ line (0-15) */
void idt_register_handler(int irq, isr_t handler)
{
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = handler;
    }
}

/* Register file structure pushed by common ISR */
typedef struct regs_t {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, user_esp, ss;
} __attribute__((packed)) regs_t;

/* CPU exception messages */
static const char *exception_msgs[] = {
    "Division By Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Security Exception",
    "Reserved"
};

/* System call dispatcher */
void syscall_handler(regs_t *r)
{
    printk("Syscall: int 0x%x (eax: 0x%x)\n", r->int_no, r->eax);
    /* Future: handle write, read, fork, etc based on eax */
}

/* Common ISR handler - called from assembly */
void isr_handler(regs_t *r)
{
    if (r->int_no < 32) {
        /* CPU exception - print detailed info and halt */
        printk("\n!!! CPU Exception: %s (int %d)\n",
            exception_msgs[r->int_no], r->int_no);
        printk("  Error code: 0x%x\n", r->err_code);
        printk("  EIP: 0x%x  CS: 0x%x  EFLAGS: 0x%x\n",
            r->eip, r->cs, r->eflags);
        printk("  EAX: 0x%x  EBX: 0x%x  ECX: 0x%x  EDX: 0x%x\n",
            r->eax, r->ebx, r->ecx, r->edx);
        printk("  System halted!\n");
        halt();
    }

    if (r->int_no == 0x80) {
        syscall_handler(r);
    }
}

/* Common IRQ handler - called from assembly */
void irq_handler(regs_t *r)
{
    int irq = r->int_no - 32;

    /* Visual Heartbeat: Only update in text mode to avoid corrupting framebuffer */
    /* (When in graphics mode, 0xB8000 may conflict with the framebuffer region) */
    extern int graphics_mode_active;
    if (!graphics_mode_active) {
        volatile uint16_t *heartbeat = (volatile uint16_t *)0xB8000 + 79;
        *heartbeat = (*heartbeat & 0xFF00) | (((*heartbeat & 0xFF) == '*') ? '.' : '*');
    }

    /* Send EOI BEFORE the handler. This allows the PIC to accept new */
    /* interrupts even if the current handler triggers a context switch. */
    pic_send_eoi(irq);

    /* Call registered handler if any */
    if (irq >= 0 && irq < 16 && irq_handlers[irq]) {
        irq_handlers[irq]();
    }
}
