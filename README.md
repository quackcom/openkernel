# openkernel

A minimal educational x86-based operating system kernel built from scratch.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](LICENSE)
[![Language](https://img.shields.io/badge/language-C-00599C?style=flat-square&logo=c&logoColor=white)](src/kernel)
[![Platform](https://img.shields.io/badge/platform-i686-lightgrey?style=flat-square)](docs/architecture/OVERVIEW.md)
[![Build](https://img.shields.io/badge/build-GNU%20Make-1f72b7?style=flat-square)](Makefile)
[![QEMU](https://img.shields.io/badge/run-QEMU-ff6600?style=flat-square)](https://www.qemu.org/)
[![Contributions](https://img.shields.io/badge/contributions-welcome-brightgreen?style=flat-square)](CONTRIBUTING.md)
[![Issues](https://img.shields.io/github/issues/quackcom/openkernel?style=flat-square)](https://github.com/quackcom/openkernel/issues)

## Quick Start

**Prerequisites**: i686-elf-gcc, NASM, QEMU, GNU Make
See [Setup Guide](docs/setup/SETUP_WINDOWS.md) for installation.

```bash
make run       # Build and run in QEMU (no ISO needed)
make iso-wsl   # Windows: build ISO via WSL (needs grub-mkrescue + xorriso)
```

## Documentation

- **[GitHub Wiki](https://github.com/quackcom/openkernel/wiki)** — full documentation and component reference (source in [`wiki/`](wiki/))
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

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request or issue.

## License

This project is licensed under the [MIT License](LICENSE).
