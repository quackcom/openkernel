# Getting Started

This guide gets you from zero to a running openkernel VM.

## What you need

| Tool | Purpose |
|------|---------|
| **i686-elf-gcc** | Cross-compiler for 32-bit bare-metal |
| **i686-elf-ld** | Linker (usually bundled with the cross toolchain) |
| **NASM** | Assembles boot and kernel `.asm` files |
| **GNU Make** | Build automation |
| **QEMU** (`qemu-system-i386`) | Runs the kernel in an emulated PC |

Optional (ISO only):

| Tool | Purpose |
|------|---------|
| **grub-mkrescue** | Builds bootable ISO with GRUB |
| **xorriso** | Used by `grub-mkrescue` to create the image |

## Windows setup

See the repository file `docs/setup/SETUP_WINDOWS.md` for full steps. Summary:

1. Install **i686-elf-tools** from [lordmilko/i686-elf-tools releases](https://github.com/lordmilko/i686-elf-tools/releases) and add `bin` to `PATH`.
2. Install **NASM**, **QEMU**, and **Make** (Chocolatey: `choco install nasm qemu gnu-make -y`).
3. Verify:

```powershell
i686-elf-gcc --version
nasm -version
qemu-system-i386 --version
make --version
```

## Clone and build

```powershell
git clone https://github.com/quackcom/openkernel.git
cd openkernel
make all
```

Output: `build/kernel.elf`

## Run in QEMU

```powershell
make run
```

This is the recommended daily workflow — no ISO required.

## First boot

You should see:

1. Kernel version and build date
2. Loading lines for GDT, IDT, ISRs, timer, keyboard
3. Memory and virtual-memory progress (OKFS init)
4. Process manager ready
5. Boot info (Multiboot magic, RAM, loader name)
6. Command prompt: `> _`

Try:

```
help
help --fs
ls
cat readme.txt
os.version
```

## Graphics mode

- **`gfx`** or **Ctrl+T** — switch to framebuffer test / graphics console
- **Ctrl+Alt+Q** — quit QEMU

## Next steps

- [[Build-and-Run]] — all Make targets and ISO on Windows/WSL
- [[Shell-and-Commands]] — full command reference
- [[Filesystem-OKFS]] — files and directories
- [[Architecture-Overview]] — how the kernel is organized
