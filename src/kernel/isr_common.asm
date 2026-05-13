; openkernel - Common ISR/IRQ Stub Macros
; This file defines macros to generate the assembly stubs for ISRs and IRQs.

; External C handlers defined in idt.c
extern isr_handler
extern irq_handler

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    cli
    push dword 0 ; Dummy error code
    push dword %1 ; Interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    cli
    push dword %1 ; Interrupt number (error code is already on stack)
    jmp isr_common_stub
%endmacro

%macro IRQ 1
global irq%1
irq%1:
    cli
    push dword 0 ; Dummy error code
    push dword (0x20 + %1) ; Interrupt number (remapped IRQ)
    jmp irq_common_stub
%endmacro

; Common stub for CPU exceptions (calls isr_handler)
isr_common_stub:
    pusha           ; Push general purpose registers
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10    ; Load kernel data segment selector (assuming GDT entry 0x10 is kernel data)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp        ; Pass pointer to regs_t struct
    call isr_handler ; Call the C handler
    add esp, 4      ; Clean up stack

    pop gs
    pop fs
    pop es
    pop ds
    popa            ; Pop general purpose registers
    add esp, 8      ; Pop interrupt number and dummy error code
    iret            ; Return from interrupt

; Common stub for hardware interrupts (calls irq_handler)
irq_common_stub:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp        ; Pass pointer to regs_t struct
    call irq_handler
    add esp, 4      ; Clean up stack

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret