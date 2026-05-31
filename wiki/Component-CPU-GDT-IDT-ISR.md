# Component: GDT, IDT, and ISR

**Files:** `gdt.c/h`, `idt.c/h`, `isr.asm`, `isr_setup.c`, `isr_common.asm`

## Purpose

Configure **x86 protected mode** segments and route **CPU exceptions** and **hardware IRQs** to C handlers.

## GDT (`gdt.c`)

The **Global Descriptor Table** defines memory segments:

| Selector | Index | Use |
|----------|-------|-----|
| `0x08` | 0 | 32-bit code, ring 0 |
| `0x10` | 1 | 32-bit data, ring 0 |

`gdt_init()` fills entries and executes `lgdt`. Flat model — base 0, limit 4 GB.

Structures: `gdt_entry_t` (limit, base, access, granularity), `gdt_ptr_t` for `lgdt`.

## IDT (`idt.c`)

The **Interrupt Descriptor Table** has 256 gates:

- Gates 0–31: CPU exceptions  
- Gates 32–47: PIC IRQs (remapped)  
- Gate 128 (`0x80`): syscall stub (future user mode)  

Key functions:

| Function | Role |
|----------|------|
| `idt_init()` | Zero table, load PIC, `lidt` |
| `idt_set_gate(n, handler, sel, flags)` | Set one vector |
| `idt_register_handler(irq, fn)` | Store C callback for IRQ 0–15 |
| `enable_interrupts()` | `sti` inline |

## ISR stubs (`isr.asm`)

Macro `ISR_NOERRCODE` / `ISR_ERRCODE` pushes vector number, calls `isr_common_stub` in assembly.

Saves: `ds`, `es`, `fs`, `gs`, `eax`–`edi`, error code, vector.  
Calls C handler, then `iret`.

## `isr_setup.c`

- Registers assembly stub address per vector with `idt_set_entry_raw`  
- **PIC remap** — IRQ 0–7 → INT 32–39, IRQ 8–15 → 40–47  
- Unmasks timer (IRQ 0) and keyboard (IRQ 1)  
- Exception handlers print vector name and register dump via `printk`  

## IRQ dispatch to drivers

| IRQ | Device | C handler registered in |
|-----|--------|-------------------------|
| 0 | PIT | `timer.c` — `timer_irq_handler` |
| 1 | PS/2 keyboard | `keyboard.c` |

Other IRQs hit generic stub (no driver).

## Exceptions covered

Divide error, debug, NMI, breakpoint, overflow, bound, invalid opcode, device not available, double fault, TSS, segment not present, stack fault, GPF, **page fault**, FPU, alignment, machine check, SIMD.

Page faults print fault address from `CR2` when implemented in handler.

## Syscall vector

INT `0x80` gate with DPL=3 prepared for ring-3 user programs — handler currently logs and returns.

## Related

- [[Boot-Process]]  
- [[Component-Keyboard-and-Timer]]  
- [[Component-Processes-and-Scheduler]] — preemption via timer IRQ  
