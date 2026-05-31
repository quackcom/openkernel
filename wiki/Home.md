# openkernel Wiki

Welcome to the **openkernel** documentation wiki — a minimal educational **32-bit x86** operating system kernel built from scratch.

This wiki explains how the project is structured, how each component works, and how to build, run, and extend the kernel.

## What is openkernel?

openkernel demonstrates core operating-system concepts in readable C and assembly:

- Multiboot boot via GRUB or QEMU `-kernel`
- Protected mode, GDT, IDT, and interrupt handling
- Physical and virtual memory (paging, heap)
- Preemptive multitasking with a round-robin scheduler
- PS/2 keyboard, PIT timer, VGA/VBE graphics
- In-memory filesystem (**OKFS**) with directories and `.txt` files
- Built-in command shell (text and graphics consoles)

**Repository:** [github.com/quackcom/openkernel](https://github.com/quackcom/openkernel)

## Quick links

| Topic | Page |
|-------|------|
| Install tools and first run | [[Getting-Started]] |
| Build targets, ISO, QEMU | [[Build-and-Run]] |
| Layered design and boot flow | [[Architecture-Overview]] |
| Step-by-step boot sequence | [[Boot-Process]] |
| RAM, paging, linker layout | [[Memory-Layout]] |
| Shell, OS commands, keyboard | [[Shell-and-Commands]] |
| OKFS files and directories | [[Filesystem-OKFS]] |
| All kernel modules explained | [[Components-Overview]] |
| Future work | [[Roadmap]] |

## Component reference

Detailed pages for each major part of the kernel:

| Component | Description |
|-----------|-------------|
| [[Component-Bootloader]] | Multiboot header, GRUB, entry stub |
| [[Component-Kernel-Core]] | `kernel_main`, VGA console, shell |
| [[Component-CPU-GDT-IDT-ISR]] | Segments, interrupts, exceptions |
| [[Component-Memory]] | PMM, VMM, heap (`kmalloc`) |
| [[Component-Processes-and-Scheduler]] | PCB, context switch, preemption |
| [[Component-Synchronization]] | Atomics, spinlocks, mutexes |
| [[Component-Display-and-Graphics]] | VGA, VBE, framebuffer, font |
| [[Component-Keyboard-and-Timer]] | PS/2 input, PIT ticks |
| [[Component-PCI]] | PCI configuration space scan |
| [[Component-Filesystem]] | OKFS API and data structures |

## For contributors

- [[Contributing-and-License]] — MIT license, **collaborator policy**, issue templates (bugs, features, wiki)
- [[Publishing-the-Wiki]] — how to sync this folder to GitHub Wiki

## Version

Current kernel version string: **v0.3** (see `KERNEL_VERSION` in `kernel.h`, command `os.version`).
