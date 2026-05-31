# Components Overview

Index of every major kernel module with file paths and links to detailed wiki pages.

## Source tree

```text
openkernel/
├── src/boot/
│   ├── boot.asm          Multiboot entry, stack, call kernel
│   ├── grub.cfg          ISO menu for GRUB
│   └── mbr.asm           Legacy MBR (optional)
├── src/kernel/
│   ├── kernel.c/h        Main, shell, VGA console
│   ├── gdt.c/h           Global Descriptor Table
│   ├── idt.c/h           Interrupt Descriptor Table
│   ├── isr.asm           Exception/IRQ stubs
│   ├── isr_setup.c       IDT entries, PIC
│   ├── isr_common.asm    Shared ISR glue
│   ├── memory.c/h        PMM, VMM, heap
│   ├── process.c/h       Scheduler, PCB
│   ├── context_switch.asm  Task switch assembly
│   ├── sync.c/h          Locks, mutexes
│   ├── timer.c/h         PIT driver
│   ├── keyboard.c/h      PS/2 driver
│   ├── keyboard.asm      PS/2 I/O helpers
│   ├── display.c/h       Video abstraction (VGA, VBE, framebuffer consolidated)
│   ├── vbe.c/h           Legacy Bochs VBE (superseded by display.c)
│   ├── framebuffer.c/h   Legacy framebuffer (superseded by display.c)
│   ├── pci.c/h           PCI config scan (in tree, not yet compiled)
│   ├── fs.c/h            OKFS filesystem
│   └── font.h            8×8 bitmap font
├── linker.ld
├── Makefile
└── wiki/                 This documentation
```

## Component pages

| Wiki page | Files | Role |
|-----------|-------|------|
| [[Component-Bootloader]] | `boot.asm`, `grub.cfg` | Load kernel, Multiboot |
| [[Component-Kernel-Core]] | `kernel.c` | Init, shell, VGA |
| [[Component-CPU-GDT-IDT-ISR]] | `gdt.*`, `idt.*`, `isr*` | CPU exceptions, IRQs |
| [[Component-Memory]] | `memory.c` | Pages and heap |
| [[Component-Processes-and-Scheduler]] | `process.c`, `context_switch.asm` | Tasks |
| [[Component-Synchronization]] | `sync.c` | Concurrency |
| [[Component-Display-and-Graphics]] | `display.c`, `font.h` | Video |
| [[Component-Keyboard-and-Timer]] | `keyboard.c`, `timer.c` | Input and time |
| [[Component-PCI]] | `pci.c` | Bus enumeration |
| [[Component-Filesystem]] | `fs.c` | OKFS |

## Initialization order

```text
gdt_init → idt_init → isr_install_all → timer_init → keyboard_init
→ memory_init → fs_init → enable_interrupts → process_init → shell
```

## Interrupt map (handled)

| IRQ | Vector | Handler |
|-----|--------|---------|
| 0 | 32 | Timer (PIT) — `timer_irq_handler` |
| 1 | 33 | Keyboard — keyboard ISR |
| CPU | 0–31 | Exceptions — print diagnostic / halt |

Other IRQ lines have stubs but no driver yet (including ATA 14/15).

## Lines of responsibility

| Concern | Primary module |
|---------|----------------|
| Text on screen | `kernel.c` (VGA), `display.c` (graphics) |
| User input | `keyboard.c` + main loop |
| Time / preemption | `timer.c` + `process.c` |
| Allocation | `memory.c` |
| Files | `fs.c` |
| Hardware discovery | `pci.c` (passive scan) |

## Related

- [[Architecture-Overview]]  
- [[Boot-Process]]  
- [[Module reference in repo]](../docs/architecture/MODULES.md) — API list mirrored from source  
