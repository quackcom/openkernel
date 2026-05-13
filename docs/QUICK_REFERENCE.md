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

1. **Install build tools** (follow docs/SETUP_WINDOWS.md):
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
openkernel Kernel initialized

=== openkernel Boot Information ===
Multiboot magic: 0x2badb002
Multiboot flags: 0x7
Memory (lower): 640 KB
Memory (upper): 261120 KB
Total memory: 256 MB
Boot loader: GRUB 2.xx
...
=== System Ready ===
Kernel halting...
```

## Troubleshooting

**Problem**: Command not found (i686-elf-gcc, nasm, qemu-system-i386)
- **Solution**: Ensure tools are in your PATH. See SETUP_WINDOWS.md

**Problem**: QEMU opens but kernel doesn't print anything
- **Solution**: Check that boot.asm and kernel.c compiled correctly. Try: `make clean && make run`

**Problem**: "grub-mkrescue not found" when running `make iso`
- **Solution**: This is optional. `make run` works fine without GRUB. Install if needed with: `choco install grub -y` (on Windows with Chocolatey)

**Problem**: Makefile errors (unknown target)
- **Solution**: Ensure you're in the openkernel directory: `cd c:\Users\rikym\openkernel`

## Development Workflow

1. **Edit source files** in `src/boot/` or `src/kernel/`
2. **Build**: `make all`
3. **Test**: `make run` (auto-builds first)
4. **Clean up**: `make clean` (removes build artifacts)
5. **Commit**: If using git, `git add . && git commit -m "..."`

## Next Development Steps

After setup works:

1. **Phase 2** (GDT/IDT setup):
   - Create `src/kernel/gdt.c/h` for Global Descriptor Table
   - Create `src/kernel/idt.c/h` for Interrupt Descriptor Table
   - Update `kernel.c` to initialize GDT/IDT before main loop

2. **Phase 3** (Memory management):
   - Create `src/kernel/memory.c/h` for paging and heap

3. **Phase 4** (Process management):
   - Create `src/kernel/process.c/h` for task management
   - Create `src/kernel/scheduler.c/h` for process scheduling

See docs/ for detailed phase documentation.

## Key Commands Cheatsheet

```bash
make              # Build kernel.elf (default)
make run          # Build and test in QEMU
make clean        # Remove build artifacts
make iso          # Create ISO (optional)
make help         # Show all targets
make check-tools  # Verify tools are installed
```

## QEMU Controls

When QEMU window is open:
- **Ctrl+Alt+G** - Toggle mouse capture
- **Ctrl+C** - Quit (in terminal mode)
- **Ctrl+Alt+Del** - Reset kernel
- **Alt+Tab** - Switch applications

## Additional Resources

- Makefile: Detailed build configuration
- linker.ld: Memory layout and section mapping
- src/kernel/kernel.h: Kernel API definitions
- src/boot/boot.asm: CPU initialization code
