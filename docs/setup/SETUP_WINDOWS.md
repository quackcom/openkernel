# Build Environment Setup for openkernel on Windows

This guide helps you install the required tools to build openkernel on Windows.

## Required Tools

1. **Cross-compiler (i686-elf-gcc)** - Compiles code for x86-based systems
2. **NASM** - Assembles x86 assembly code
3. **QEMU** - Emulator to test the OS
4. **GNU Make** - Build automation

## Installation Steps

### Option 1: Using OSDev.org Prebuilt Binaries (Recommended)

The OSDev community maintains prebuilt cross-compiler binaries for Windows:

1. Download from: https://github.com/lordmilko/i686-elf-tools/releases
   - Get the latest release for Windows (e.g., `i686-elf-tools-windows.zip`)
   
2. Extract the archive to `C:\OSTools` (or your preferred location)

3. Add to your system PATH:
   - Open "Edit the system environment variables"
   - Click "Environment Variables"
   - Under "System variables", select "Path" and click "Edit"
   - Add the path to bin folder: `C:\OSTools\i686-elf-tools\bin`
   - Click OK, then OK again

4. Verify installation:
   ```powershell
   i686-elf-gcc --version
   i686-elf-ld --version
   ```

### Option 2: Using Chocolatey Package Manager

If you have Chocolatey installed:

```powershell
choco install nasm qemu gnu-make -y
```

Then follow Option 1 for the cross-compiler.

### Option 3: Manual Installation

1. **NASM** (Netwide Assembler):
   - Download from: https://www.nasm.us/
   - Extract and add `bin` folder to PATH

2. **QEMU**:
   - Download from: https://www.qemu.org/download/
   - Install and add installation directory to PATH

3. **GNU Make**:
   - Download from: http://gnuwin32.sourceforge.net/packages/make.htm
   - Install and add to PATH

4. **Cross-compiler**:
   - Follow Option 1 or build from source (advanced)

## Verification

After installation, run these commands in PowerShell:

```powershell
i686-elf-gcc --version
nasm -version
qemu-system-i386 --version
make --version
```

All should return version information without errors.

## Troubleshooting

**"Command not found" error:**
- Make sure all tools are added to your PATH
- Restart PowerShell after modifying PATH
- Use full paths if needed (e.g., `C:\OSTools\i686-elf-tools\bin\i686-elf-gcc.exe`)

**On Windows with Git Bash:**
If you're using Git Bash instead of PowerShell, the same PATH additions work.

## Building a bootable ISO

`make iso` needs **grub-mkrescue** and **xorriso**. These are not included with the usual Windows build tools and are **not** on PATH by default — that is why you may see:

```text
ERROR: grub-mkrescue not found on PATH.
```

### Option A: Use WSL (recommended on Windows)

You already have WSL; install the ISO tools once inside Ubuntu:

```bash
sudo apt update
sudo apt install -y grub-pc-bin xorriso
```

Then either:

**From WSL** (project on the Windows drive):

```bash
cd /mnt/c/Users/rikym/openkernel
make iso
make run-iso
```

**From PowerShell** (delegates to WSL and installs packages if missing):

```powershell
cd c:\Users\rikym\openkernel
make iso-wsl
```

### Option B: Skip the ISO

For day-to-day development you do **not** need an ISO. GRUB is only required to pack a CD image:

```powershell
make run
```

This boots `build/kernel.elf` directly in QEMU (`-kernel`), which is enough to test the kernel and filesystem.

### Verify ISO tools

```powershell
make check-tools-iso
```

On native Windows PATH this will usually fail until you use WSL; that is expected.

## Next Steps

Once tools are installed:
1. Navigate to the openkernel directory: `cd c:\Users\rikym\openkernel`
2. Run `make all` to build the kernel
3. Run `make run` to test with QEMU (no ISO required)
4. Optional: `make iso-wsl` or `make iso` (inside WSL) to build a bootable CD image
