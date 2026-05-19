# Roadmap

Development phases and future plans for openkernel.

## Phase 1: Initial Setup (COMPLETE)
- Project directory structure
- Build system (Makefile) configured
- Linker script and memory layout
- Bootloader (Multiboot-compliant via GRUB)
- Kernel entry point with VGA output

## Phase 2: Kernel Skeleton (COMPLETE)
- GDT (Global Descriptor Table) for protected mode
- IDT (Interrupt Descriptor Table)
- CPU exception handlers
- IRQ handling (PIT timer, PS/2 keyboard)

## Phase 3: Memory Management (COMPLETE)
- Physical memory manager (bitmap-based)
- Virtual memory / paging (4KB pages, kernel at 0xC0000000)
- Heap allocator (kmalloc/kfree)

## Phase 4: Process Management (COMPLETE)
- Process Control Blocks with full CPU state
- Context switching
- Round-robin scheduler
- Preemptive multitasking (timer-driven)
- Wait queues and blocking synchronization

## Phase 5: I/O Drivers (COMPLETE)
- PS/2 keyboard (IRQ-driven, multi-layout support)
- PIT timer (100Hz system ticks)
- Display abstraction (VGA, VBE, framebuffer backends)
- PCI bus enumeration
- Spinlocks and mutexes

## Phase 6: User Space & Shell
- [ ] System call interface (write, read, exit, fork)
- [ ] User mode execution (ring 3)
- [ ] Simple shell with command execution
- [ ] Shell programs/utilities

## Phase 7: Filesystem
- [ ] Block I/O and disk driver
- [ ] Simple filesystem (e.g., ext2 or custom)
- [ ] File read/write operations
- [ ] Directory support

## Phase 8: Networking (Future)
- [ ] Network interface driver
- [ ] Protocol stack (IP, TCP/UDP)
- [ ] Network services

## Long-term Goals
- Self-hosting compilation
- Port to x86-64 (long mode)
- SMP support
