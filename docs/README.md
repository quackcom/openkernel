# Documentation

This directory contains comprehensive documentation for the openkernel project.

## Quick Navigation

### Getting Started
- **[Setup Guide](setup/SETUP_WINDOWS.md)** - Install build tools and dependencies (Windows-focused)
- **[Quick Reference](guides/QUICK_REFERENCE.md)** - Common commands and quick start guide

### Development
- **[Build Guide](build/BUILD.md)** - Detailed build instructions, options, and troubleshooting
- **[Architecture Overview](architecture/OVERVIEW.md)** - System design, layers, and boot sequence
- **[Module Reference](architecture/MODULES.md)** - Detailed documentation of each kernel module
- **[Roadmap](roadmap/ROADMAP.md)** - Development phases and future plans

## Documentation Structure

```
docs/
├── README.md                    # This file
├── setup/
│   └── SETUP_WINDOWS.md        # Installation guide for Windows
├── build/
│   └── BUILD.md                # Build system and instructions
├── guides/
│   └── QUICK_REFERENCE.md      # Quick start and common tasks
├── architecture/
│   ├── OVERVIEW.md             # System architecture and design
│   └── MODULES.md              # Individual module documentation
└── roadmap/
    └── ROADMAP.md              # Development phases and plans
```

## For Different Roles

### First-Time Users
1. Read [Quick Reference](guides/QUICK_REFERENCE.md)
2. Follow [Setup Guide](setup/SETUP_WINDOWS.md)
3. Try building with [Build Guide](build/BUILD.md)

### Developers
1. Review [Architecture Overview](architecture/OVERVIEW.md)
2. Consult [Module Reference](architecture/MODULES.md) for specific modules
3. Use [Build Guide](build/BUILD.md) for development workflow

### Contributors
1. Understand [Architecture Overview](architecture/OVERVIEW.md)
2. Check [Module Reference](architecture/MODULES.md)
3. Follow [Build Guide](build/BUILD.md) development workflow
4. Ensure code follows existing patterns

## Key Concepts

### Multiboot
openkernel uses the Multiboot specification for bootloading, allowing it to boot via GRUB on real hardware or QEMU.

### Protected Mode
Runs in x86 32-bit protected mode, not real mode. GRUB handles the transition from real mode.

### Modular Design
Each subsystem (GDT, IDT, Memory, Processes) is independent and can be studied separately.

### Display Abstraction
Supports multiple graphics backends:
- Multiboot VBE (GRUB on real hardware)
- Bochs VBE (QEMU/VirtualBox)
- VGA Mode 0x13 (fallback)

## Development Phases

See [Architecture Overview](architecture/OVERVIEW.md#development-phases) for current phase status and roadmap.

## See Also

- Main [README](../README.md)
- [Project Source](../src/)
- [Build System](../Makefile)
