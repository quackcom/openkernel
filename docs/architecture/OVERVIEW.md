# openkernel Architecture Overview

## Project Goals

openkernel is a minimal educational x86-based operating system kernel built from scratch. It demonstrates fundamental OS concepts including bootloading, memory management, interrupt handling, and process management.

## Design Principles

1. **Educational** - Code is structured for clarity and learning
2. **Minimal** - Only essential features, no bloat
3. **Self-contained** - Few external dependencies
4. **x86-based** - 32-bit x86 architecture for broad compatibility

## Architecture Layers

### Layer 1: Bootloader
- **Location**: `src/boot/boot.asm`, `src/boot/grub.cfg`
- **Purpose**: Entry point, sets up basic CPU state, transfers control to kernel
- **Bootloader**: Multiboot-compliant, loaded by GRUB
- **Responsibilities**:
  - Set up stack
  - Load kernel ELF
  - Pass Multiboot information to kernel

### Layer 2: Kernel Core
- **Location**: `src/kernel/kernel.c`, `src/kernel/kernel.h`
- **Purpose**: Main kernel logic and initialization
- **Responsibilities**:
  - Initialize subsystems (GDT, IDT, memory, processes)
  - Handle Multiboot information
  - Print boot diagnostics
  - Provide main control loop

### Layer 3: CPU Management
- **GDT (Global Descriptor Table)**: `src/kernel/gdt.c`, `src/kernel/gdt.h`
  - Defines memory segments for x86 protected mode
  - Manages privilege levels and memory protection
  
- **IDT (Interrupt Descriptor Table)**: `src/kernel/idt.c`, `src/kernel/idt.h`
  - Maps interrupt/exception vectors to handlers
  - Defines interrupt gates and trap gates
  
- **ISR (Interrupt Service Routines)**: `src/kernel/isr.asm`, `src/kernel/isr.c`, `src/kernel/isr_setup.c`
  - CPU exception handlers (divide by zero, page fault, etc.)
  - IRQ handlers (external interrupts)
  - Stub code and common interrupt handling logic

### Layer 4: Memory Management
- **Location**: `src/kernel/memory.c`, `src/kernel/memory.h`
- **Purpose**: Physical and virtual memory management
- **Responsibilities**:
  - Track physical memory
  - Manage page tables for virtual memory
  - Provide memory allocation/deallocation

### Layer 5: Process Management
- **Location**: `src/kernel/process.c`, `src/kernel/process.h`
- **Purpose**: Process/task creation and management
- **Responsibilities**:
  - Create and destroy processes
  - Manage process state
  - Provide process scheduler
  
- **Context Switching**: `src/kernel/context_switch.asm`
  - Low-level assembly for saving/restoring CPU state
  - Switches between different process contexts

### Layer 5b: Synchronization
- **Location**: `src/kernel/sync.c`, `src/kernel/sync.h`
- **Purpose**: Thread-safe primitives
- **Responsibilities**:
  - Atomic operations (xchg, cmpxchg, add, inc, dec)
  - Spinlocks with interrupt-safe variants
  - Blocking mutexes using process wait queues

### Layer 6: I/O and Drivers
- **Timer**: `src/kernel/timer.c`, `src/kernel/timer.h`
  - PIT (Programmable Interval Timer) driver
  - Provides system ticks and timing
  
- **Keyboard**: `src/kernel/keyboard.c`, `src/kernel/keyboard.h`, `src/kernel/keyboard.asm`
  - PS/2 keyboard driver
  - Reads keyboard events
  - Manages keyboard layout
  
- **Display**: `src/kernel/display.c`, `src/kernel/display.h`
  - Abstraction layer for video output
  - Supports multiple video modes (VGA, VBE, framebuffer)
  
- **Framebuffer**: `src/kernel/framebuffer.c`, `src/kernel/framebuffer.h`
  - Direct framebuffer graphics support
  - Pixel drawing and text rendering
  
- **VBE (VESA BIOS Extensions)**: `src/kernel/vbe.c`, `src/kernel/vbe.h`
  - Bochs VBE graphics mode initialization
  - Hardware-independent graphics setup
  
- **PCI**: `src/kernel/pci.c`, `src/kernel/pci.h`
  - PCI bus enumeration
  - Device discovery and initialization

### Layer 7: Utilities
- **Font**: `src/kernel/font.h`
  - 8x8 bitmap font (IBM CP437)
  - Text rendering glyphs

## Memory Layout

As defined by `linker.ld`:

```
0x00000000 - 0x00007BFF  : BIOS reserved
0x00007C00 - 0x00007DFF  : Boot sector
0x00100000                : Kernel load address (1MB)
0xC0000000                : Virtual kernel space (with paging)
```

## Boot Sequence

1. **GRUB Bootloader** loads kernel.elf at 0x00100000
2. **boot.asm** entry point executes (sets up stack, calls kernel_main)
3. **kernel_main()** runs:
   - Validates Multiboot magic
   - Calls **kernel_init()** which initializes:
     - GDT, IDT, ISRs (interrupt handling)
     - PIT timer (IRQ 0 at 100 Hz)
     - PS/2 keyboard (IRQ 1)
   - Detects keyboard layout from Multiboot command line
   - Initializes memory manager (pmm, vmm, heap)
   - Enables interrupts for preemptive multitasking
   - Initializes process manager
   - Prints boot diagnostics
4. **Idle loop** dispatches keyboard input to command prompt (text or graphics mode)

## Development Phases

### Phase 1: Initial Setup (COMPLETE)
- ✓ Project structure
- ✓ Build system
- ✓ Bootloader
- ✓ Kernel entry point
- ✓ VGA output

### Phase 2: Kernel Skeleton (COMPLETE)
- [x] GDT implementation
- [x] IDT implementation
- [x] Exception handlers
- [x] Interrupt handling

### Phase 3: Memory Management (COMPLETE)
- [x] Physical memory manager (bitmap-based PMM)
- [x] Virtual memory / paging (page directory + page tables)
- [x] Heap allocator (kmalloc/kfree)

### Phase 4: Process Management (COMPLETE)
- [x] Process structures (PCB with full state)
- [x] Context switching
- [x] Process scheduler (round-robin)
- [x] Wait queues and synchronization primitives

### Phase 5: I/O Drivers (COMPLETE)
- [x] Keyboard driver (PS/2, IRQ-driven, multi-layout)
- [x] Timer driver (PIT, IRQ-driven)
- [x] Display abstraction (VGA text, VGA graphics, VBE, framebuffer)
- [x] PCI enumeration

### Phase 6: Filesystem
- [ ] Simple filesystem
- [ ] File reading/writing
- [ ] Directory support

### Phase 7: User Space
- [ ] User mode execution
- [ ] Shell/command interpreter
- [ ] Basic utilities

## Key Design Decisions

1. **Multiboot Compliance**: Allows booting via GRUB on real hardware or QEMU
2. **Protected Mode Only**: Skips real mode complexity, assumes GRUB handles 32-bit setup
3. **32-bit x86**: Good balance of simplicity and real-world relevance
4. **Modular Design**: Each subsystem (GDT, IDT, Memory, etc.) is independent
5. **Paging**: Kernel mapped at 0xC0000000 with identity mapping for early boot
6. **Preemptive Multitasking**: Timer IRQ triggers round-robin scheduler
7. **Display Abstraction**: Supports multiple graphics backends (VGA, VBE, framebuffer)
8. **Synchronization Primitives**: Atomic ops, spinlocks, and blocking mutexes via wait queues

## See Also

- [Build Guide](../build/BUILD.md)
- [Quick Reference](../guides/QUICK_REFERENCE.md)
- [Setup Guide](../setup/SETUP_WINDOWS.md)
