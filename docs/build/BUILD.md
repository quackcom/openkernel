# Building openkernel

This guide covers how to build openkernel from source.

## Prerequisites

Ensure you have all required tools installed. See [Setup Guide](../setup/SETUP_WINDOWS.md) for installation instructions.

## Build Targets

The Makefile provides several build targets:

```bash
make all          # Build kernel.elf (default)
make run          # Build and run in QEMU
make iso          # Create bootable ISO (requires GRUB)
make run-iso      # Build ISO and run with QEMU
make clean        # Remove all build artifacts
make help         # Display all targets
```

## Building

### Quick Build

To build the kernel executable:

```bash
make all
```

Output: `build/kernel.elf`

### Build and Test

To build and immediately run in QEMU:

```bash
make run
```

This automatically builds the kernel and launches it in QEMU.

### Clean Build

If you encounter build issues, perform a clean build:

```bash
make clean
make all
```

## Build Process

The build system performs these steps:

1. **Assemble bootloader** - Compiles `src/boot/boot.asm` to `build/boot.o`
2. **Compile kernel C code** - Compiles all `.c` files in `src/kernel/`
3. **Assemble kernel assembly** - Compiles assembly helpers (ISRs, context switching)
4. **Link** - Links all object files using `linker.ld` to produce `build/kernel.elf`

## Output Files

After building:

- `build/kernel.elf` - The kernel executable (loaded by bootloader)
- `build/*.o` - Object files (intermediate)
- `build/kernel.iso` - Bootable ISO (if `make iso` was run)
- `build/*-os.img` - Disk image (if `build-image.ps1` was run)

## Troubleshooting

**Compilation errors:**
- Verify all tools are installed correctly
- Check PATH environment variables
- Try a clean build: `make clean && make all`

**Linker errors:**
- Ensure `linker.ld` exists in the project root
- Check that all source files are properly referenced in the Makefile

**QEMU not found:**
- Ensure QEMU is installed and in your PATH
- Verify with: `qemu-system-i386 --version`

## Development Workflow

```bash
# 1. Edit source files in src/
# 2. Build and test
make run

# 3. If needed, clean and rebuild
make clean
make run
```

## Advanced Usage

### Building with Optimization

Edit the Makefile and change CFLAGS:

```makefile
CFLAGS = -ffreestanding -fno-stack-protector -Wall -Wextra -O2 -g
```

Change `-O0` to `-O2` for optimization (slower compilation, faster runtime).

### Creating ISO Image

To create a bootable ISO (requires GRUB tools):

```bash
make iso
```

Then boot with:

```bash
qemu-system-i386 -cdrom build/openkernel.iso
```

## See Also

- [Quick Reference](../guides/QUICK_REFERENCE.md)
- [Architecture Overview](../architecture/OVERVIEW.md)
