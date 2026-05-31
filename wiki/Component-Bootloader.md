# Component: Bootloader

**Files:** `src/boot/boot.asm`, `src/boot/grub.cfg`, `linker.ld`

## Purpose

Hand off control from a Multiboot-compliant loader (GRUB or QEMU) to the C kernel with a valid stack and boot information.

## Multiboot header (`boot.asm`)

Located in section `.multiboot_header`:

| Field | Value | Meaning |
|-------|-------|---------|
| Magic | `0x1BADB002` | Multiboot 1 signature |
| Flags | `0x00000003` | Align modules + memory info |
| Checksum | `0xE4524FFB` | Ensures magic + flags + checksum = 0 |

Optional video request: linear framebuffer **1024×768×32** (used when GRUB honors it).

## Entry sequence (`multiboot_entry`)

1. `ESP = stack_top` (16 KB stack, 16-byte aligned before `call`)  
2. Push **EBX** (Multiboot info pointer)  
3. Push **EAX** (magic `0x2BADB002`)  
4. `call kernel_main`  
5. On return: `cli` + infinite `hlt`  

## GRUB configuration (`grub.cfg`)

```grub
menuentry "openkernel" {
    multiboot /boot/kernel.elf
    boot
}
```

Used when building `build/openkernel.iso` — kernel copied to `boot/kernel.elf`.

## Linker contract

`linker.ld` sets:

- `ENTRY(multiboot_entry)`  
- Load address **0x00100000**  
- `.multiboot_header` must appear in first 8 KB of the load segment  

## QEMU direct boot

`make run` uses `-kernel build/kernel.elf`. QEMU’s loader performs Multiboot setup without GRUB — same entry point.

## Legacy MBR (`mbr.asm`)

Optional 512-byte boot sector for floppy images (`build-image.ps1`). Not the primary development path.

## Related

- [[Boot-Process]]  
- [[Build-and-Run]]  
- [[Component-Kernel-Core]]  
