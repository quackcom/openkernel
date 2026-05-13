#include "idt.h"
#include <stdint.h>

// External assembly stubs for IRQs
extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

// External assembly stubs for CPU exceptions (0-31)
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

// External assembly stubs for syscalls (if any, e.g., 0x80)
extern void isr128(); // For interrupt 0x80

void isr_install_all(void) {
    // CPU Exceptions (0-31)
    idt_set_entry_raw(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_entry_raw(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set_entry_raw(2, (uint32_t)isr2, 0x08, 0x8E);
    idt_set_entry_raw(3, (uint32_t)isr3, 0x08, 0x8E);
    idt_set_entry_raw(4, (uint32_t)isr4, 0x08, 0x8E);
    idt_set_entry_raw(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set_entry_raw(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set_entry_raw(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set_entry_raw(8, (uint32_t)isr8, 0x08, 0x8E);
    idt_set_entry_raw(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set_entry_raw(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_entry_raw(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_entry_raw(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_entry_raw(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_entry_raw(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_entry_raw(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_entry_raw(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_entry_raw(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_entry_raw(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_entry_raw(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_entry_raw(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_entry_raw(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_entry_raw(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_entry_raw(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_entry_raw(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_entry_raw(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_entry_raw(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_entry_raw(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_entry_raw(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_entry_raw(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_entry_raw(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_entry_raw(31, (uint32_t)isr31, 0x08, 0x8E);

    // IRQs (remapped to 0x20-0x2F)
    idt_set_entry_raw(0x20, (uint32_t)irq0, 0x08, 0x8E); // IRQ0 (Timer)
    idt_set_entry_raw(0x21, (uint32_t)irq1, 0x08, 0x8E); // IRQ1 (Keyboard)
    idt_set_entry_raw(0x22, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_entry_raw(0x23, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_entry_raw(0x24, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_entry_raw(0x25, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_entry_raw(0x26, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_entry_raw(0x27, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_entry_raw(0x28, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_entry_raw(0x29, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_entry_raw(0x2A, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_entry_raw(0x2B, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_entry_raw(0x2C, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_entry_raw(0x2D, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_entry_raw(0x2E, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_entry_raw(0x2F, (uint32_t)irq15, 0x08, 0x8E);

    // Example for syscall 0x80 (if you have one)
    idt_set_entry_raw(0x80, (uint32_t)isr128, 0x08, 0xEE); // DPL=3 for user-mode access
}