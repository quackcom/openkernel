; openkernel Bootloader
; Multiboot-compliant 32-bit boot code

BITS 32

; Multiboot header constants
MULTIBOOT_MAGIC    equ 0x1BADB002
MULTIBOOT_FLAGS    equ 0x00000003  ; bit 0: align, bit 1: mem info
MULTIBOOT_CHECKSUM equ 0xE4524FFB

; Video mode request (used when bit 2 is set in flags)
MULTIBOOT_MODE_TYPE equ 0         ; 0 = linear graphics mode
MULTIBOOT_WIDTH     equ 1024      ; requested width
MULTIBOOT_HEIGHT    equ 768       ; requested height
MULTIBOOT_DEPTH     equ 32        ; requested bits per pixel

; Memory constants
STACK_SIZE equ 0x4000
STACK_ALIGN equ 0x10

section .multiboot_header
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM
    dd 0                         ; header_addr (unused if flags bit 16 = 0)
    dd 0                         ; load_addr (unused)
    dd 0                         ; load_end_addr (unused)
    dd 0                         ; bss_end_addr (unused)
    dd 0                         ; entry_addr (unused)
    dd MULTIBOOT_MODE_TYPE       ; mode_type: 0 = linear, 1 = text
    dd MULTIBOOT_WIDTH           ; width
    dd MULTIBOOT_HEIGHT          ; height
    dd MULTIBOOT_DEPTH           ; depth

section .bss
align 16
    stack_bottom:
    resb STACK_SIZE
    stack_top:

section .text
align 4
    global multiboot_entry

multiboot_entry:
    ; Set up stack (stack_top is in .bss aligned to 16 bytes)
    mov esp, stack_top
    
    ; The .bss section is 16-byte aligned, so stack_top is 16-byte aligned.
    ; We need to subtract 8 to compensate for the two 4-byte pushes ahead,
    ; ensuring ESP is 16-byte aligned at the 'call' instruction.
    sub esp, 8
    
    ; Push Multiboot info pointer (ebx) and magic (eax) for kernel_main
    push ebx
    push eax
    
    ; Call kernel
    extern kernel_main
    call kernel_main
    
    ; Hang
    cli
    jmp .hang
    
.hang:
    hlt
    jmp .hang