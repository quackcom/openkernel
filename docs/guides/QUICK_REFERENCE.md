# Quick Reference Guide

## Building the Kernel

```bash
# Build kernel ELF executable
make all

# Build and run with QEMU immediately
make run

# Build ISO image (requires GRUB tools)
make iso

# Build and run ISO with QEMU
make run-iso

# Clean all build artifacts
make clean

# Show all available targets
make help
```

## First-Time Setup on Windows

1. **Install build tools** (follow [Setup Guide](../setup/SETUP_WINDOWS.md)):
   - i686-elf-gcc (cross-compiler)
   - NASM (assembler)
   - QEMU (emulator)
   - GNU Make (build automation)

2. **Navigate to project**:
   ```powershell
   cd c:\Users\rikym\openkernel
   ```

3. **Build and run**:
   ```bash
   make run
   ```

## Expected Output

When running successfully, QEMU should display:

```
openkernel Kernel v0.2
Built: May 13 2026 XX:XX:XX

=== System Ready ===
Press Ctrl+T to toggle graphics test mode.
```

## Keyboard Controls

- **Ctrl+T** - Toggle between graphics test mode and text console
- **Ctrl+Alt+Q** - Quit QEMU

## Troubleshooting

**Problem**: Command not found (i686-elf-gcc, nasm, qemu-system-i386)
- **Solution**: Ensure tools are in your PATH. See [Setup Guide](../setup/SETUP_WINDOWS.md)

**Problem**: QEMU opens but kernel doesn't print anything
- **Solution**: Check that boot.asm and kernel.c compiled correctly. Try: `make clean && make run`

**Problem**: "grub-mkrescue not found" when running `make iso`
- **Solution**: This is optional. `make run` works fine without GRUB. Install if needed with: `choco install grub -y` (on Windows with Chocolatey)

**Problem**: Makefile errors (unknown target)
- **Solution**: Ensure you're in the openkernel directory: `cd c:\Users\rikym\openkernel`

**Problem**: Text in graphics mode appears mirrored or garbled
- **Solution**: This is a known issue being debugged. The issue is with font bitmap rendering.

## Development Workflow

1. **Edit source files** in `src/boot/` or `src/kernel/`
2. **Build**: `make all`
3. **Test**: `make run` (auto-builds first)
4. **Debug**: Check output in QEMU window

## Project Directories

```
openkernel/
├── src/
│   ├── boot/           # Bootloader (Multiboot)
│   ├── kernel/         # Kernel implementation
│   ├── drivers/        # Device drivers (future)
│   └── user/           # User programs (future)
├── build/              # Build output (generated)
├── docs/               # Documentation
├── Makefile            # Build system
├── linker.ld           # Memory layout
└── README.md           # Project overview
```

## See Also

- [Build Guide](../build/BUILD.md)
- [Architecture Overview](../architecture/OVERVIEW.md)
- [Setup Guide](../setup/SETUP_WINDOWS.md)
