# Boot Process

End-to-end flow from power-on to the `>` prompt.

## 1. Firmware and GRUB

**Path A — `make run` (QEMU `-kernel`):**  
QEMU loads `kernel.elf` at 1 MB and sets up Multiboot registers.

**Path B — `make run-iso`:**  
GRUB reads `grub.cfg`, loads `/boot/kernel.elf`, passes Multiboot info.

## 2. `boot.asm` entry (`multiboot_entry`)

Location: `src/boot/boot.asm`

1. **Multiboot header** in `.multiboot_header` (magic, flags, checksum, optional 1024×768×32 mode request).
2. **Stack** — 16 KB in `.bss`, `ESP` aligned for C calls.
3. **Arguments** — `push ebx` (Multiboot info), `push eax` (magic).
4. **`call kernel_main`**.

If `kernel_main` returns, the boot code halts in a loop.

## 3. `kernel_main()` validation

Location: `src/kernel/kernel.c`

```c
if (magic != 0x2BADB002) { halt forever; }
```

Wrong magic means the kernel was not loaded by a Multiboot-compliant loader.

## 4. `kernel_init()` — early hardware

| Step | Module | Action |
|------|--------|--------|
| VGA clear | `kernel.c` | Text mode 80×25 @ `0xB8000` |
| GDT | `gdt_init()` | Code/data segments, flat protected mode |
| IDT + PIC | `idt_init()` | 256 gates, remap IRQs |
| ISRs | `isr_install_all()` | CPU exceptions + IRQ stubs |
| Timer | `timer_init(100)` | PIT ~100 Hz, IRQ 0 handler |
| Keyboard | `keyboard_init()` | PS/2 enable, IRQ 1 handler |

Loading animation prints inline progress for each step.

## 5. Keyboard layout

If Multiboot flags include cmdline (`mbd->flags & (1 << 2)`):

```c
keyboard_detect_layout((const char*)mbd->cmdline);
```

Supports `layout=us`, `layout=uk`, `layout=it` in the boot command line.

## 6. Memory management

```c
memory_init(mbd->mem_lower, mbd->mem_upper);
```

Sub-steps (see [[Component-Memory]]):

1. **PMM** — bitmap of physical pages; mark kernel region used; free rest  
2. **VMM** — page directory, identity map, map kernel high, VGA, VBE, heap region  
3. **Heap** — `kmalloc` / `kfree` at `0xE0000000`, 64 MB  

Progress UI: `[OK] Memory management` then animated **Virtual memory** bar.

## 7. Filesystem

```c
fs_init();
```

Creates root `/`, seeds `readme.txt` and `docs/about.txt`.

## 8. Interrupts and processes

```c
enable_interrupts();   // STI
process_init();      // idle PCB, scheduler
```

Timer preemption becomes active. EFLAGS checked to confirm interrupts enabled.

## 9. Boot diagnostics

Prints Multiboot magic, flags, RAM sizes, loader name. Saves values for `os.bootinfo` command.

## 10. Interactive shell

Clears screen, prints welcome, shows `> ` prompt.

## Main loop (runtime)

```text
forever:
    if graphics_toggle (Ctrl+T): switch gfx / pseudo-console
    while scancode available:
        handle keys for text cmdline OR graphics OR edit mode
    handle arrow / delete flags
    hlt
```

## Boot timeline diagram

```text
GRUB/QEMU
   │
   ▼
boot.asm ──► kernel_main
   │
   ├─► kernel_init (GDT, IDT, ISR, timer, keyboard)
   ├─► memory_init (PMM, VMM, heap)
   ├─► fs_init
   ├─► enable_interrupts
   ├─► process_init
   └─► shell loop (hlt + keyboard)
```

## Related

- [[Component-Bootloader]]  
- [[Component-Kernel-Core]]  
- [[Memory-Layout]]  
