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
openkernel Kernel v0.3
Built: <build date> <build time>

[Loading 1/5] [#####] GDT
[Loading 2/5] [#####] IDT/PIC
[Loading 3/5] [#####] ISRs
[Loading 4/5] [#####] PIT Timer
[Loading 5/5] [#####] PS/2 Keyboard
  [OK]

Initializing memory management...
  [OK]

Initializing process management...
  [OK]
  [OK] System Ready (EFLAGS: 0x...)

=== openkernel Boot Info ===
Magic: 0x2BADB002  Flags: 0x...
Memory: ... KB lower, ... KB upper (... MB total)
Loader: ...

openkernel Command Line
Type 'help' for available commands.
> _
```

## Keyboard Controls

- **Ctrl+T** - Toggle between graphics test mode and pseudo-text console
- **Ctrl+Alt+Q** - Quit QEMU
- **Arrow Up/Down** - Scroll terminal history (text mode)
- **Arrow Left/Right** - Move cursor within command line
- **Backspace** - Delete character before cursor
- **Delete** - Delete character at cursor (graphics mode)
- **Enter** - Execute command

## Available Commands

- `help` - Show available commands
- `clear` - Clear the console
- `reboot` - Restart the system
- `shutdown` - Power off the system
- `gfx` - Enter graphics mode
- `layout us|uk|it` - Set keyboard layout
- `cmd.print("msg")` - Print a message
- `os.bootinfo` - Show boot information

## Troubleshooting

**Problem**: Command not found (i686-elf-gcc, nasm, qemu-system-i386)
- **Solution**: Ensure tools are in your PATH. See [Setup Guide](../setup/SETUP_WINDOWS.md)

**Problem**: QEMU hangs or kernel doesn't print anything
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
