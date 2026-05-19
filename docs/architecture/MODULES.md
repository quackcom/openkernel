# Module Reference

Detailed documentation of each kernel module.

## Core Modules

### Kernel Entry Point (`kernel.c`, `kernel.h`)

Main kernel initialization and control.

**Functions**:
- `kernel_main()` - Main entry point
- `kernel_init()` - Subsystem initialization
- `printk()` - Formatted printing (like printf)
- `putchar()` - Print single character
- `puts()` - Print string

**Data**:
- `graphics_mode_active` - Current display mode
- `gfx_log[]` - Graphics mode logging buffer

### GDT Module (`gdt.c`, `gdt.h`)

Global Descriptor Table management for protected mode.

**Functions**:
- `gdt_init()` - Initialize GDT

**Structures**:
- `gdt_entry_t` - GDT descriptor entry
- `gdt_ptr_t` - GDT pointer for LGDT instruction

**Selectors**:
- `GDT_CODE_SEG` (0x08) - Code segment selector
- `GDT_DATA_SEG` (0x10) - Data segment selector

### IDT Module (`idt.c`, `idt.h`)

Interrupt Descriptor Table for exception/interrupt handling.

**Functions**:
- `idt_init()` - Initialize IDT
- `idt_set_gate()` - Add interrupt handler entry
- `idt_register_handler()` - Register an IRQ handler
- `enable_interrupts()` - Enable interrupts (STI)

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
- `memory_init()` - Initialize memory manager (pmm, vmm, heap)
- `kmalloc()` - Kernel memory allocation
- `kfree()` - Free kernel memory
- `memory_get_total()` - Query total memory in KB
- `pmm_init()` - Initialize physical memory manager
- `pmm_alloc_page()` - Allocate a physical page
- `pmm_free_page()` - Free a physical page
- `vmm_init()` - Initialize virtual memory (paging)
- `vmm_map_page()` - Map a virtual page to physical
- `vmm_unmap_page()` - Unmap a virtual page
- `heap_init()` - Initialize heap allocator

**Data Structures**:
- Bitmap-based physical memory tracker
- Page directory and page tables for paging
- Linked-list heap block allocator

### Paging

Implements virtual memory through page tables. Identity maps first 4MB, maps kernel at `KERNEL_VIRTUAL_BASE` (0xC0000000), and establishes mapping for framebuffer and heap regions.

## I/O and Drivers

### Display Abstraction (`display.c`, `display.h`)

High-level display API supporting multiple backends.

**Functions**:
- `display_init()` - Initialize graphics (probe all backends)
- `display_init_multiboot()` - Initialize from Multiboot framebuffer
- `display_enter_gfx()` - Enter graphics mode silently
- `display_disable()` - Restore VGA text mode
- `display_get_mode()` - Get current display mode info
- `display_clear()` - Clear screen to color
- `display_putpixel()` - Draw single pixel
- `display_putchar()` - Draw character at position
- `display_puts()` - Draw string at position
- `display_fill_rect()` - Fill rectangle
- `display_draw_rect()` - Draw rectangle outline

**Backends** (autodetected in priority order):
- Multiboot VBE (GRUB on real hardware) — 1024x768x32
- Bochs VBE (QEMU/VirtualBox) — 800x600x32
- VGA Mode 0x13 (real hardware fallback) — 320x200x8
- VGA Text Mode (always works) — 80x25

**Color Helper**:
- `display_rgb(r, g, b)` — Create 32-bit RGB color

### Framebuffer (`framebuffer.c`, `framebuffer.h`)

Framebuffer graphics primitives (alternative lower-level API).

**Functions**:
- `fb_init()` - Initialize from Multiboot info
- `fb_init_direct()` - Initialize directly (VBE fallback)
- `fb_available()` - Check if framebuffer is ready
- `fb_get_info()` - Get current framebuffer info
- `fb_putpixel()` - Draw single pixel
- `fb_fill_rect()` - Fill a rectangle
- `fb_draw_rect()` - Draw a rectangle outline
- `fb_clear()` - Clear screen

### VBE Module (`vbe.c`, `vbe.h`)

Bochs VESA BIOS Extensions for graphics mode setup (QEMU -kernel mode).

**Functions**:
- `vbe_init()` - Detect VBE capability and set mode
- `vbe_get_mode()` - Get current VBE mode info

### Keyboard (`keyboard.c`, `keyboard.h`)

PS/2 keyboard input handling.

**Functions**:
- `keyboard_init()` - Initialize keyboard driver (enable port, configure IRQ)
- `keyboard_detect_layout()` - Detect/set keyboard layout from command line
- `keyboard_get_modifiers()` - Get current modifier key state
- `keyboard_get_scancode()` - Get next scancode (non-blocking)
- `keyboard_scancode_to_ascii()` - Convert scancode to ASCII character
- `keyboard_set_layout()` - Set layout by ID (0=US, 1=UK, 2=IT)

**Layouts**: us, uk, it

**Modifier Flags**:
- `KEYBOARD_CAPS_LOCK`, `KEYBOARD_NUM_LOCK`, `KEYBOARD_SCROLL_LOCK`
- `KEYBOARD_LSHIFT`, `KEYBOARD_RSHIFT`, `KEYBOARD_LALT`
- `KEYBOARD_LCTRL`, `KEYBOARD_RCTRL`

**Arrow Key Flags** (set by IRQ, consumed in main loop):
- `arrow_up_requested`, `arrow_down_requested`, `arrow_left_requested`, `arrow_right_requested`
- `delete_requested`

### Timer (`timer.c`, `timer.h`)

PIT (Programmable Interval Timer) for system timing.

**Functions**:
- `timer_init()` - Initialize timer with frequency
- `timer_get_ticks()` - Get elapsed ticks
- `timer_sleep()` - Busy-wait sleep for milliseconds

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
- `process_init()` - Initialize process manager and scheduler
- `process_create()` - Create new process with priority
- `process_terminate()` - Terminate a process by PID
- `process_yield()` - Yield CPU to scheduler
- `process_get_current_pid()` - Get current process ID
- `process_get_current_pcb()` - Get current process PCB

**Structures**:
- `pcb_t` - Process Control Block (pid, state, registers, kernel_stack, priority)
- `wait_queue_t` - Wait queue for blocking synchronization (head/tail)

**Scheduler Functions**:
- `scheduler_init()` - Initialize round-robin scheduler
- `scheduler_add_process()` - Add process to ready queue
- `scheduler_remove_process()` - Remove process from ready queue
- `scheduler_schedule()` - Select and switch to next process

**States**:
- `PROCESS_STATE_READY` (0) - Ready to run
- `PROCESS_STATE_RUNNING` (1) - Currently executing
- `PROCESS_STATE_BLOCKED` (2) - Blocked on a wait queue
- `PROCESS_STATE_ZOMBIE` (3) - Terminated, slot available

**Wait Queue Primitives**:
- `process_block(wq)` - Block current process on a wait queue
- `process_wake(wq)` - Wake first process on a wait queue
- `process_wake_all(wq)` - Wake all processes on a wait queue

### Context Switching (`context_switch.asm`)

Low-level CPU state management for process switching.

**Functions**:
- `context_switch()` - Switch to new process (saves/restores EIP, EBP, EBX, ESI, EDI)

### Synchronization (`sync.c`, `sync.h`)

Atomic operations, spinlocks, and blocking mutexes.

**Atomic Operations**:
- `atomic_t` - Volatile integer type
- `atomic_set()`, `atomic_get()` - Simple read/write
- `atomic_xchg()` - Atomic exchange
- `atomic_cmpxchg()` - Atomic compare-and-exchange
- `atomic_inc()`, `atomic_dec()`, `atomic_add()` - Atomic arithmetic
- `mb()` - Full memory barrier
- `barrier()` - Compiler barrier

**Spinlocks**:
- `spinlock_t` - Busy-wait lock with PAUSE hint
- `spinlock_init()`, `spinlock_lock()`, `spinlock_unlock()`, `spinlock_trylock()`
- `spinlock_lock_irqsave()`, `spinlock_unlock_irqrestore()` - Interrupt-safe variants

**Mutexes** (blocking, uses process wait queues):
- `mutex_t` - Opaque blocking mutex
- `mutex_init()`, `mutex_lock()`, `mutex_unlock()`, `mutex_trylock()`

## Utilities

### Font (`font.h`)

IBM CP437 8x8 bitmap font for text rendering.

Contains 128 glyph definitions (0-127 ASCII) in `font8x8[]`.

## Module Dependencies

```
kernel.c
├── gdt.c
├── idt.c
├── timer.c  (via isr)
├── keyboard.c (via isr)
├── display.c
├── memory.c
│   ├── pmm (physical allocator)
│   ├── vmm (paging)
│   └── heap (kmalloc/kfree)
├── process.c
│   ├── context_switch.asm
│   └── sync.c (mutexes via wait queues)
└── pci.c

display.c
├── font.h
├── vbe.c (Bochs VBE backend)
├── framebuffer.c (direct framebuffer)
└── VGA hardware (legacy modes)

isr.asm / isr_setup.c / isr_common.asm
├── timer.c (IRQ 0)
└── keyboard.c (IRQ 1)
```

## See Also

- [Architecture Overview](./OVERVIEW.md)
- [Build Guide](../build/BUILD.md)
