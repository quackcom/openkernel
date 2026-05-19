# openkernel

A minimal educational x86-based operating system kernel built from scratch.

## Quick Start

**Prerequisites**: i686-elf-gcc, NASM, QEMU, GNU Make
See [Setup Guide](docs/setup/SETUP_WINDOWS.md) for installation.

```bash
make run    # Build and run in QEMU
```

## Documentation

- [Quick Reference](docs/guides/QUICK_REFERENCE.md) — Common commands and expected output
- [Build Guide](docs/build/BUILD.md) — Build options and troubleshooting
- [Architecture Overview](docs/architecture/OVERVIEW.md) — System design and layers
- [Module Reference](docs/architecture/MODULES.md) — Detailed API documentation
- [Roadmap](docs/roadmap/ROADMAP.md) — Development phases and future plans

## Project Structure

```
src/
├── boot/        # Multiboot-compliant bootloader
├── kernel/      # Kernel: GDT, IDT, memory, processes, drivers, sync
build/           # Build artifacts
docs/            # Documentation
```

## License

Educational use. Refer to individual source files.
