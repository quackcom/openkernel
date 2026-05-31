# Roadmap

Development status and planned work for openkernel.

## Completed phases

### Phase 1 — Initial setup
- Project layout, Makefile, linker script  
- Multiboot boot, kernel entry, VGA output  

### Phase 2 — Kernel skeleton
- GDT, IDT, CPU exceptions, IRQ framework  

### Phase 3 — Memory
- Bitmap PMM, paging, 64 MB heap (`kmalloc`)  

### Phase 4 — Processes
- PCB, context switch, round-robin scheduler, preemption  
- Wait queues, spinlocks, mutexes  

### Phase 5 — I/O
- PS/2 keyboard (US/UK/IT), PIT timer  
- Display abstraction (VGA, VBE, framebuffer)  
- PCI enumeration  

### Phase 5b — Shell & FS (current)
- Built-in command shell (text + graphics)  
- **OKFS** — directories, `.txt` read/write/edit  
- OS commands: `os.version`, `os.bootinfo`  

## In progress / next

### Phase 6 — User space
- [ ] System call interface (`int 0x80` — stub exists)  
- [ ] Ring 3 execution, separate address spaces  
- [ ] User-mode programs loaded from OKFS  
- [ ] Shell moved out of kernel (optional)  

### Phase 7 — Persistent storage
- [ ] RAM disk or ATA PIO driver  
- [ ] On-disk OKFS format (`mkfs`, block allocator)  
- [ ] Survive reboot in QEMU with `-hda`  

### Phase 8 — Networking (future)
- [ ] NIC driver  
- [ ] IPv4 / UDP or TCP subset  

## Long-term goals

- Self-hosting toolchain on openkernel  
- x86-64 (long mode) port  
- SMP (APIC, per-CPU run queues)  

## How to help

Pick an unchecked item, discuss in an issue, submit a PR. See [[Contributing-and-License]].

Track repo file: `docs/roadmap/ROADMAP.md` (may lag wiki slightly).
