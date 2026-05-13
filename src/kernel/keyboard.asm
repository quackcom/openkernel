; openkernel - Generic Low-level I/O Helpers

section .text
global io_inb
global io_outb
global io_wait_read ; Helper to wait for status bit 0 (Output buffer full)
; global io_wait_write ; Not implemented, commented out for clarity

; uint8_t io_inb(uint16_t port)
io_inb:
    mov dx, [esp + 4]
    in al, dx
    ret

; void io_outb(uint16_t port, uint8_t data)
io_outb:
    mov dx, [esp + 4]
    mov al, [esp + 8]
    out dx, al
    ret

; Helper to wait for status bit 0 (Output buffer full)
io_wait_read:
    in al, 0x64
    test al, 1
    jz io_wait_read
    ret