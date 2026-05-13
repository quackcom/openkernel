# openkernel

A minimal educational x86-based operating system kernel built from scratch.

## Project Status

**Phase 1 - Initial Setup: COMPLETE** ✓
- Project directory structure created
- Build system (Makefile) configured
- Linker script prepared
- Bootloader skeleton (boot.asm) implemented
- Kernel entry point (kernel.c) with VGA output
- GRUB configuration added

## Quick Start

### Prerequisites

Before building, install the required tools:
- **i686-elf-gcc** (cross-compiler)
- **NASM** (assembler)
- **QEMU** (emulator)
- **GNU Make** (build system)

See [Setup Guide](docs/setup/SETUP_WINDOWS.md) for installation instructions on Windows.

### Building

```bash
cd openkernel
make all          # Build kernel ELF
make run          # Build and run in QEMU
make clean        # Clean build artifacts
```

### Build Targets

- `make all` - Build kernel.elf (default)
- `make iso` - Create bootable ISO (requires GRUB)
- `make run` - Build kernel and run with QEMU
- `make run-iso` - Build ISO and run with QEMU
- `make clean` - Remove build artifacts
- `make help` - Display all targets

## Project Structure

```
openkernel/
├── src/
│   ├── boot/           # Bootloader code
│   │   ├── boot.asm    # Multiboot-compliant entry point
│   │   └── grub.cfg    # GRUB configuration
│   ├── kernel/         # Kernel source code
│   │   ├── kernel.c    # Kernel main (VGA output, boot info)
│   │   └── kernel.h    # Kernel definitions
│   ├── drivers/        # Device drivers (future)
│   └── user/           # User-space programs (future)
├── build/              # Build artifacts (generated)
├── docs/               # Documentation
├── Makefile            # Build system
├── linker.ld           # Linker script (memory layout)
└── README.md           # This file
```

## Documentation

Comprehensive documentation is organized in the `docs/` folder:

- **[Setup Guide](docs/setup/SETUP_WINDOWS.md)** - Installation instructions for build tools
- **[Build Guide](docs/build/BUILD.md)** - Detailed build instructions and troubleshooting
- **[Quick Reference](docs/guides/QUICK_REFERENCE.md)** - Quick start and common commands
- **[Architecture Overview](docs/architecture/OVERVIEW.md)** - System design and architecture
- **[Module Reference](docs/architecture/MODULES.md)** - Detailed module documentation

## Current Capabilities

✓ **Bootloader** - Multiboot-compliant, loads kernel via GRUB  
✓ **VGA Output** - Basic text mode printing to screen  
✓ **Boot Information** - Displays Multiboot info (memory, bootloader name)  
✓ **Debug Output** - printk() with format specifiers (%d, %x, %s, %%)  

## Current Limitations

✗ No interrupts/exceptions handling  
✗ No memory management (paging, malloc)  
✗ No process management or multitasking  
✗ No filesystem  
✗ No I/O (keyboard, disk)  
✗ No user-space shell  

## Next Phases

### Phase 2: Kernel Skeleton (GDT, IDT, Exceptions)
- [ ] Set up Global Descriptor Table (GDT)
- [ ] Set up Interrupt Descriptor Table (IDT)
- [ ] Implement CPU exception handlers
- [ ] Test interrupt handling

### Phase 3: Memory Management
- [ ] Physical memory manager
- [ ] Virtual memory (paging)
- [ ] Heap allocator (malloc/free)

### Phase 4: Process Management
- [ ] Process/task structures
- [ ] Process scheduler
- [ ] Context switching
- [ ] Syscall interface

### Phase 5: I/O Drivers
- [ ] Timer driver (PIT/APIC)
- [ ] Keyboard driver
- [ ] Enhanced VGA driver (scrolling)

### Phase 6: User Space & Shell
- [ ] Simple shell
- [ ] Basic syscalls (write, read, exit, fork)
- [ ] Shell programs

### Phase 7: Filesystem (Optional)
- [ ] Basic filesystem (ext2)
- [ ] Ramdisk support

## Development Notes

### Architecture
- Target: x86-64 compatible (currently 32-bit protected mode)
- Bootloader: GRUB Multiboot protocol
- Environment: QEMU emulator
- Build: Cross-compilation on Windows/Linux/macOS

### Key Files
- **boot.asm** - Assembly entry point, stack setup, calls kernel_main
- **kernel.c** - C kernel code with VGA text mode support
- **linker.ld** - Maps code/data sections, sets kernel load address at 1MB
- **Makefile** - Compiles, assembles, links, and runs kernel

### Memory Layout
```
0x00000000 -------- BIOS/Reserved
0x00100000 -------- Kernel base (1MB) - Multiboot entry point
           code (.text)
           constants (.rodata)
           data (.data)
           stack (.bss)
```

## Debugging

### QEMU Tips
- Use `-serial stdio` to redirect serial output to terminal
- Use `-gdb tcp::1234` to enable GDB debugging
- Press `Ctrl+Alt+G` to toggle mouse grabbing in GUI
- Press `Ctrl+C` to exit QEMU from terminal

### Common Issues
- **"i686-elf-gcc not found"** → Install cross-compiler and add to PATH
- **"grub-mkrescue not found"** → GRUB tools optional, `make run` works without ISO
- **QEMU hangs** → Check kernel initialization, may need interrupt handlers

## References

- **OSDev Wiki**: https://wiki.osdev.org/
- **x86-64 Manuals**: https://www.intel.com/content/www/us/en/develop/articles/intel-sdm.html
- **Multiboot Specification**: https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
- **Similar OS Projects**: xv6, MicroKernel, Minix

## License

Educational use. Refer to individual source files for licensing.

## Status Log

- **2026-05-08**: Phase 1 complete - Basic bootloader and kernel skeleton with VGA output
