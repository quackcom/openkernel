# Module Reference

Detailed documentation of each kernel module.

## Core Modules

### Kernel Entry Point (`kernel.c`, `kernel.h`)

Main kernel initialization and control.

**Functions**:
- `kernel_main()` - Main entry point
- `printk()` - Formatted printing (like printf)
- Various subsystem initializers

**Data**:
- `graphics_mode_active` - Current display mode
- `gfx_log[]` - Graphics mode logging buffer

### GDT Module (`gdt.c`, `gdt.h`)

Global Descriptor Table management for protected mode.

**Functions**:
- `gdt_init()` - Initialize GDT
- `gdt_set_gate()` - Add descriptor entry

**Structures**:
- `gdt_entry_t` - GDT descriptor entry
- `gdt_ptr_t` - GDT pointer for LGDT instruction

### IDT Module (`idt.c`, `idt.h`)

Interrupt Descriptor Table for exception/interrupt handling.

**Functions**:
- `idt_init()` - Initialize IDT
- `idt_set_gate()` - Add interrupt handler entry

**Structures**:
- `idt_entry_t` - IDT descriptor entry
- `idt_ptr_t` - IDT pointer for LIDT instruction

### ISR Module (`isr.asm`, `isr_setup.c`, `isr_common.asm`)

Interrupt Service Routines and CPU exception handlers.

**Exceptions**:
- Divide by Zero, Debug, NMI, Breakpoint
- Overflow, Bound Range Exceeded
- Invalid Opcode, Device Not Available
- Double Fault, Invalid TSS, Segment Not Present
- Stack Segment Fault, General Protection Fault
- Page Fault, x87 FPU Error, Alignment Check
- Machine Check, SIMD Exception

**IRQs**:
- Timer (IRQ 0)
- Keyboard (IRQ 1)
- Other standard IRQs

## Memory Management

### Memory Module (`memory.c`, `memory.h`)

Physical and virtual memory management.

**Functions**:
- `memory_init()` - Initialize memory manager
- `kmalloc()` - Kernel memory allocation
- `kfree()` - Free kernel memory
- `memory_get_info()` - Query memory statistics

**Data Structures**:
- Memory map tracking
- Free block management

### Paging

Implements virtual memory through page tables (future phase).

## I/O and Drivers

### Display Abstraction (`display.c`, `display.h`)

High-level display API supporting multiple backends.

**Functions**:
- `display_init()` - Initialize graphics
- `display_putchar()` - Draw character
- `display_puts()` - Draw string
- `display_fill_rect()` - Fill rectangle
- `display_draw_rect()` - Draw rectangle outline
- `display_clear()` - Clear screen

**Backends**:
- Multiboot VBE (GRUB on real hardware)
- Bochs VBE (QEMU/VirtualBox)
- VGA Mode 0x13 (fallback)

### Framebuffer (`framebuffer.c`, `framebuffer.h`)

Framebuffer graphics primitives.

**Functions**:
- `framebuffer_init()` - Initialize framebuffer
- `framebuffer_putpixel()` - Draw single pixel
- `framebuffer_get_mode()` - Get current mode

### VBE Module (`vbe.c`, `vbe.h`)

VESA BIOS Extensions for graphics mode setup.

**Functions**:
- `vbe_probe()` - Detect VBE capability
- `vbe_set_mode()` - Switch to graphics mode
- `vbe_read()`, `vbe_write()` - VBE register access

### Keyboard (`keyboard.c`, `keyboard.h`, `keyboard.asm`)

PS/2 keyboard input handling.

**Functions**:
- `keyboard_init()` - Initialize keyboard driver
- `keyboard_handler()` - Interrupt handler
- `keyboard_getchar()` - Get next character
- `keyboard_detect_layout()` - Set keyboard layout (us, uk, it)

### Timer (`timer.c`, `timer.h`)

PIT (Programmable Interval Timer) for system timing.

**Functions**:
- `timer_init()` - Initialize timer
- `timer_get_ticks()` - Get elapsed ticks
- `timer_sleep()` - Sleep for milliseconds

### PCI (`pci.c`, `pci.h`)

PCI bus enumeration and device discovery.

**Functions**:
- `pci_init()` - Scan PCI bus
- `pci_get_device()` - Find device by class/vendor
- `pci_read_config()` - Read device config

## Process Management

### Process Module (`process.c`, `process.h`)

Process/task management and scheduling.

**Functions**:
- `process_init()` - Initialize process manager
- `process_create()` - Create new process
- `process_exit()` - Terminate process
- `process_schedule()` - Select next process

**Structures**:
- `process_t` - Process control block
- `process_state_t` - Process state enum

### Context Switching (`context_switch.asm`)

Low-level CPU state management for process switching.

**Functions**:
- `context_switch()` - Switch to new process
- `set_kernel_stack()` - Set TSS kernel stack

## Utilities

### Font (`font.h`)

IBM CP437 8x8 bitmap font for text rendering.

Contains 128 glyph definitions (0-127 ASCII).

## Module Dependencies

```
kernel.c
├── gdt.c
├── idt.c
├── isr.c/isr.asm
├── memory.c
├── process.c
│   └── context_switch.asm
├── timer.c
├── keyboard.c
│   └── keyboard.asm
├── display.c
│   ├── framebuffer.c
│   │   ├── vbe.c
│   │   └── font.h
│   └── font.h
└── pci.c
```

## See Also

- [Architecture Overview](./OVERVIEW.md)
- [Build Guide](../build/BUILD.md)
