# Build and Run

## Make targets

| Target | Description |
|--------|-------------|
| `make` / `make all` | Build `build/kernel.elf` |
| `make run` | Build and start QEMU with `-kernel build/kernel.elf` |
| `make iso` | Create `build/openkernel.iso` (needs `grub-mkrescue` + `xorriso` on PATH) |
| `make iso-wsl` | Build ISO via WSL (recommended on Windows) |
| `make run-iso` | Build ISO and boot QEMU with `-cdrom` |
| `make clean` | Remove `build/` |
| `make check-tools` | Verify compiler, NASM, QEMU, Make |
| `make check-tools-iso` | Verify GRUB/xorriso on PATH |
| `make help` | List targets |

## QEMU command (run target)

Equivalent to:

```text
qemu-system-i386 -machine pc -kernel build/kernel.elf -m 256M -vga std -k it
```

- **256 MB** RAM — enough for paging and 64 MB heap
- **`-vga std`** — standard VGA for graphics modes
- **`-k it`** — Italian keyboard layout in QEMU (change as needed)

## ISO build on Windows

Native Windows usually **does not** have `grub-mkrescue` or `xorriso`. Chocolatey does not ship them reliably.

**Recommended:**

```powershell
make iso-wsl
```

This uses WSL Ubuntu, installs `grub-pc-bin` and `xorriso` if needed, and runs `make iso` inside Linux.

**Manual WSL:**

```bash
sudo apt install -y grub-pc-bin xorriso
cd /mnt/c/Users/you/openkernel
make iso
make run-iso
```

## ISO layout

`make iso` stages:

```text
build/iso/
  boot/
    kernel.elf      ← linked kernel
    grub/
      grub.cfg      ← menu entry: multiboot /boot/kernel.elf
```

`src/boot/grub.cfg` defines the GRUB menu and Multiboot invocation.

## Linker layout

`linker.ld` places the kernel at **physical 1 MB** (`0x00100000`):

- `.multiboot_header` first (GRUB requirement)
- `.text`, `.rodata`, `.data`, `.bss`

Virtual mapping at **0xC0000000** is set up later by the VMM in `memory.c`.

## Object files linked

Boot: `boot.asm`  
Kernel C: `kernel.c`, `gdt.c`, `idt.c`, `isr_setup.c`, `memory.c`, `process.c`, `sync.c`, `timer.c`, `keyboard.c`, `display.c`, **`fs.c`**  
Assembly: `isr.asm`, `context_switch.asm`, `keyboard.asm`

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `i686-elf-gcc` not found | Add cross-compiler `bin` to PATH |
| `grub-mkrescue not found` | Use `make iso-wsl` or `make run` without ISO |
| Blank QEMU screen | `make clean && make run` |
| Makefile `if not exist` errors | Use GNU Make for Windows (cmd backend) |

See also: [[Getting-Started]], [[Boot-Process]].
