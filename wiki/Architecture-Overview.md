# Architecture Overview

## Design principles

1. **Educational** — code favors clarity over cleverness  
2. **Minimal** — only features needed to teach real OS concepts  
3. **Self-contained** — builds with a cross-toolchain and QEMU  
4. **32-bit x86** — protected mode, Multiboot, broad emulator support  

## Layer model

```text
┌─────────────────────────────────────────────────────────┐
│  Shell / commands (kernel.c + fs.c)                     │
├─────────────────────────────────────────────────────────┤
│  OKFS filesystem (in-memory, .txt files)                │
├─────────────────────────────────────────────────────────┤
│  Process manager + scheduler + sync (process.c, sync.c)│
├─────────────────────────────────────────────────────────┤
│  Memory: PMM → VMM → heap (memory.c)                    │
├─────────────────────────────────────────────────────────┤
│  Drivers: keyboard, timer, display, PCI                 │
├─────────────────────────────────────────────────────────┤
│  CPU: GDT, IDT, ISR stubs (gdt.c, idt.c, isr*.asm)    │
├─────────────────────────────────────────────────────────┤
│  Boot: Multiboot header + GRUB (boot.asm, grub.cfg)     │
└─────────────────────────────────────────────────────────┘
```

## Layer summary

| Layer | Source | Responsibility |
|-------|--------|----------------|
| Boot | `src/boot/` | Multiboot header, stack, call `kernel_main` |
| Core | `kernel.c` | Init order, VGA shell, graphics UI |
| CPU | `gdt.c`, `idt.c`, `isr*.asm` | Segments, vectors, IRQ/exception dispatch |
| Memory | `memory.c` | Bitmap PMM, page tables, `kmalloc` |
| Processes | `process.c`, `context_switch.asm` | PCB, round-robin, preemption |
| Sync | `sync.c` | Spinlocks, mutexes, atomics |
| I/O | `keyboard.c`, `timer.c`, `display.c`, … | Input, time, video |
| FS | `fs.c` | OKFS paths, directories, `.txt` I/O |
| Utils | `font.h` | 8×8 glyphs for graphics text |

## Key design decisions

| Decision | Rationale |
|----------|-----------|
| **Multiboot** | GRUB loads ELF; QEMU `-kernel` works without ISO |
| **Higher-half kernel** (`0xC0000000`) | Classic layout; separates user/kernel virtual space later |
| **Identity map low 4 MB** | Early access to VGA, boot structures |
| **100 Hz PIT** | Timer IRQ drives preemption (~every 10 ticks) |
| **IRQ-driven keyboard** | Ring buffer of scancodes; main loop polls |
| **Display abstraction** | Probe Multiboot VBE → Bochs VBE → VGA modes |
| **OKFS in RAM** | No disk driver yet; full VFS-like shell commands |

## Module dependency graph

```text
kernel.c
├── gdt.c, idt.c, isr_setup.c / isr.asm
├── timer.c ──────────── IRQ 0
├── keyboard.c ───────── IRQ 1
├── memory.c (pmm, vmm, heap)
├── process.c → context_switch.asm
├── sync.c ← process wait queues
├── display.c → font.h, vbe I/O, VGA registers
├── fs.c → memory.h (kmalloc)
└── pci.c (enumeration only)

boot.asm → kernel_main()
```

## Multiboot information

`kernel.h` defines `multiboot_info_t`. GRUB passes:

- **EAX** = `0x2BADB002` (magic)  
- **EBX** = pointer to info structure  

Used for: memory size (`mem_lower` / `mem_upper`), cmdline (keyboard layout), framebuffer (if present), boot loader name.

## What runs after boot

A single **idle loop** in `kernel_main`:

1. Drain keyboard scancode buffer  
2. Update text or graphics command line  
3. Handle arrow keys / Ctrl+T  
4. `hlt` until next interrupt  

The timer IRQ may call `scheduler_schedule()` for preemptive tasks; the idle process (PID 0) runs the shell loop when no other threads are ready.

## Related pages

- [[Boot-Process]] — ordered init sequence  
- [[Memory-Layout]] — addresses and paging  
- [[Components-Overview]] — per-file reference  
- [[Roadmap]] — planned features  
