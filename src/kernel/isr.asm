; openkernel - Interrupt Service Routines (ISRs) and Interrupt Request (IRQ) handlers
; This file defines the assembly stubs for CPU exceptions and hardware interrupts.

%include "isr_common.asm" ; Common macro for pushing/popping registers

; CPU Exceptions (0-31)
; These are handled by isr_handler in idt.c
ISR_NOERRCODE 0  ; Division By Zero
ISR_NOERRCODE 1  ; Debug
ISR_NOERRCODE 2  ; Non-Maskable Interrupt
ISR_NOERRCODE 3  ; Breakpoint
ISR_NOERRCODE 4  ; Overflow
ISR_NOERRCODE 5  ; Bound Range Exceeded
ISR_NOERRCODE 6  ; Invalid Opcode
ISR_NOERRCODE 7  ; Device Not Available
ISR_ERRCODE   8  ; Double Fault
ISR_NOERRCODE 9  ; Coprocessor Segment Overrun (Reserved)
ISR_ERRCODE   10 ; Invalid TSS
ISR_ERRCODE   11 ; Segment Not Present
ISR_ERRCODE   12 ; Stack-Segment Fault
ISR_ERRCODE   13 ; General Protection Fault
ISR_ERRCODE   14 ; Page Fault
ISR_NOERRCODE 15 ; Reserved
ISR_NOERRCODE 16 ; x87 FPU Error
ISR_ERRCODE   17 ; Alignment Check
ISR_NOERRCODE 18 ; Machine Check
ISR_NOERRCODE 19 ; SIMD Floating-Point Exception
ISR_NOERRCODE 20 ; Virtualization Exception
ISR_NOERRCODE 21 ; Reserved
ISR_NOERRCODE 22 ; Reserved
ISR_NOERRCODE 23 ; Reserved
ISR_NOERRCODE 24 ; Reserved
ISR_NOERRCODE 25 ; Reserved
ISR_NOERRCODE 26 ; Reserved
ISR_NOERRCODE 27 ; Reserved
ISR_NOERRCODE 28 ; Reserved
ISR_NOERRCODE 29 ; Reserved
ISR_ERRCODE   30 ; Security Exception
ISR_NOERRCODE 31 ; Reserved

; IRQs (remapped to 0x20-0x2F)
; These are handled by irq_handler in idt.c
IRQ 0  ; Timer
IRQ 1  ; Keyboard
IRQ 2  ; Cascade (PIC2)
IRQ 3  ; COM2
IRQ 4  ; COM1
IRQ 5  ; LPT2
IRQ 6  ; Floppy
IRQ 7  ; LPT1
IRQ 8  ; CMOS clock
IRQ 9  ; Free / SCSI / NIC
IRQ 10 ; Free / SCSI / NIC
IRQ 11 ; Free / SCSI / NIC
IRQ 12 ; PS/2 Mouse
IRQ 13 ; FPU / Co-processor
IRQ 14 ; Primary ATA
IRQ 15 ; Secondary ATA

; System call interrupt (e.g., 0x80)
global isr128
isr128:
    cli
    push byte 0 ; Dummy error code
    push 128    ; Interrupt number (0x80)
    jmp isr_common_stub