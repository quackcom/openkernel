; Simple MBR-style bootloader for QEMU
; Loads kernel from disk and jumps to it

[ORG 0x7c00]
BITS 16

start:
    cli
    cld
    
    ; Set up segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    
    ; Enable A20 gate (simple method)
    mov ax, 0x2401
    int 0x15
    
    ; Switch to protected mode
    lgdt [gdt_desc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; Far jump to protected mode
    jmp dword 0x08:protected_mode

[BITS 32]
protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x90000
    
    ; Jump to kernel at 1MB
    jmp 0x100000

; GDT
gdt:
    dq 0
    dq 0x00cf9a000000ffff    ; Code segment
    dq 0x00cf92000000ffff    ; Data segment

gdt_desc:
    dw $ - gdt - 1
    dd gdt

; Pad to boot sector size
times 510 - ($ - $$) db 0
dw 0xaa55
