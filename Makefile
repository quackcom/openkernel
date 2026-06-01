# openkernel Makefile
# Build system for x86-64 bootable kernel

# Compiler and tools
CC = i686-elf-gcc
ASM = nasm
LD = i686-elf-ld
GRUB_MKRESCUE = grub-mkrescue

# Flags
CFLAGS = -ffreestanding -fno-stack-protector -Wall -Wextra -O0 -g
ASMFLAGS = -f elf32 -i $(KERNEL_DIR)/
LDFLAGS = -T linker.ld -nostdlib

# Directories
SRC_DIR = src
BUILD_DIR = build
KERNEL_DIR = $(SRC_DIR)/kernel
BOOT_DIR = $(SRC_DIR)/boot

# Source files
BOOT_ASM = $(BOOT_DIR)/boot.asm
ISR_ASM  = $(KERNEL_DIR)/isr.asm
CONTEXT_SWITCH_ASM = $(KERNEL_DIR)/context_switch.asm
IO_ASM   = $(KERNEL_DIR)/keyboard.asm
KERNEL_C = $(KERNEL_DIR)/kernel.c
KERNEL_HEADER = $(KERNEL_DIR)/kernel.h
GDT_C    = $(KERNEL_DIR)/gdt.c
IDT_C    = $(KERNEL_DIR)/idt.c
ISR_SETUP_C = $(KERNEL_DIR)/isr_setup.c
TIMER_C  = $(KERNEL_DIR)/timer.c
KEYBOARD_C = $(KERNEL_DIR)/keyboard.c
DISPLAY_C = $(KERNEL_DIR)/display.c
MEMORY_C = $(KERNEL_DIR)/memory.c
PROCESS_C = $(KERNEL_DIR)/process.c
SYNC_C   = $(KERNEL_DIR)/sync.c
FS_C     = $(KERNEL_DIR)/fs.c
BLOCK_C  = $(KERNEL_DIR)/block.c
OKFS_DISK_C = $(KERNEL_DIR)/okfs_disk.c

# Driver sources
DRV_DIR  = $(SRC_DIR)/drivers
ATA_C    = $(DRV_DIR)/ata.c
RAMDISK_C = $(DRV_DIR)/ramdisk.c

# Object files
BOOT_OBJ = $(BUILD_DIR)/boot.o
ISR_OBJ  = $(BUILD_DIR)/isr.o
CONTEXT_SWITCH_OBJ = $(BUILD_DIR)/context_switch.o
IO_OBJ   = $(BUILD_DIR)/keyboard_asm.o
KERNEL_OBJ = $(BUILD_DIR)/kernel.o
GDT_OBJ  = $(BUILD_DIR)/gdt.o
IDT_OBJ  = $(BUILD_DIR)/idt.o
ISR_SETUP_OBJ = $(BUILD_DIR)/isr_setup.o
TIMER_OBJ = $(BUILD_DIR)/timer.o
KEYBOARD_OBJ = $(BUILD_DIR)/keyboard.o
DISPLAY_OBJ = $(BUILD_DIR)/display.o
MEMORY_OBJ = $(BUILD_DIR)/memory.o
PROCESS_OBJ = $(BUILD_DIR)/process.o
SYNC_OBJ   = $(BUILD_DIR)/sync.o
FS_OBJ     = $(BUILD_DIR)/fs.o
BLOCK_OBJ  = $(BUILD_DIR)/block.o
ATA_OBJ    = $(BUILD_DIR)/ata.o
RAMDISK_OBJ = $(BUILD_DIR)/ramdisk.o
OKFS_DISK_OBJ = $(BUILD_DIR)/okfs_disk.o

# Output files
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
ISO = $(BUILD_DIR)/openkernel.iso

# ISO tools (override if installed elsewhere)
GRUB_MKRESCUE ?= grub-mkrescue
XORRISO ?= xorriso

# Phony targets
.PHONY: all clean run iso iso-wsl check-tools check-tools-iso help

# Default target
all: $(KERNEL_ELF)

# Ensure build directory exists
$(BUILD_DIR):
	@if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)

# Build kernel ELF executable
$(KERNEL_ELF): $(BOOT_OBJ) $(ISR_OBJ) $(CONTEXT_SWITCH_OBJ) $(IO_OBJ) $(GDT_OBJ) $(IDT_OBJ) $(ISR_SETUP_OBJ) $(TIMER_OBJ) $(KEYBOARD_OBJ) $(DISPLAY_OBJ) $(MEMORY_OBJ) $(PROCESS_OBJ) $(SYNC_OBJ) $(FS_OBJ) $(BLOCK_OBJ) $(ATA_OBJ) $(RAMDISK_OBJ) $(OKFS_DISK_OBJ) $(KERNEL_OBJ) | $(BUILD_DIR)
	@echo "Linking kernel..."
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "Kernel built: $@"

# Compile rules - create build dir first
$(BOOT_OBJ): $(BOOT_ASM) | $(BUILD_DIR)
	@echo "Assembling boot code..."
	$(ASM) $(ASMFLAGS) -o $@ $<

$(ISR_OBJ): $(ISR_ASM) $(ISR_COMMON_ASM) | $(BUILD_DIR)
	@echo "Assembling ISR stubs..."
	$(ASM) $(ASMFLAGS) -o $@ $<

$(CONTEXT_SWITCH_OBJ): $(CONTEXT_SWITCH_ASM) | $(BUILD_DIR)
	@echo "Assembling context switch..."
	$(ASM) $(ASMFLAGS) -o $@ $<

$(IO_OBJ): $(IO_ASM) | $(BUILD_DIR)
	@echo "Assembling IO helpers..."
	$(ASM) $(ASMFLAGS) -o $@ $<

$(GDT_OBJ): $(GDT_C) $(KERNEL_DIR)/gdt.h | $(BUILD_DIR)
	@echo "Compiling GDT..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c -o $@ $(GDT_C)

$(IDT_OBJ): $(IDT_C) $(KERNEL_DIR)/idt.h $(KERNEL_HEADER) | $(BUILD_DIR)
	@echo "Compiling IDT..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c -o $@ $(IDT_C)

$(ISR_SETUP_OBJ): $(ISR_SETUP_C) $(KERNEL_DIR)/idt.h | $(BUILD_DIR)
	@echo "Compiling ISR setup..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c -o $@ $(ISR_SETUP_C)

$(TIMER_OBJ): $(TIMER_C) $(KERNEL_DIR)/timer.h $(KERNEL_DIR)/idt.h | $(BUILD_DIR)
	@echo "Compiling timer..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c -o $@ $(TIMER_C)

$(KEYBOARD_OBJ): $(KEYBOARD_C) $(KERNEL_DIR)/keyboard.h $(KERNEL_DIR)/idt.h | $(BUILD_DIR)
	@echo "Compiling keyboard..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c -o $@ $(KEYBOARD_C)

$(DISPLAY_OBJ): $(DISPLAY_C) $(KERNEL_DIR)/display.h $(KERNEL_HEADER) | $(BUILD_DIR)
	@echo "Compiling display..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c -o $@ $(DISPLAY_C)

$(MEMORY_OBJ): $(MEMORY_C) $(KERNEL_DIR)/memory.h $(KERNEL_HEADER) | $(BUILD_DIR)
	@echo "Compiling memory management..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c -o $@ $(MEMORY_C)

$(PROCESS_OBJ): $(PROCESS_C) $(KERNEL_DIR)/process.h $(KERNEL_DIR)/memory.h $(KERNEL_HEADER) | $(BUILD_DIR)
	@echo "Compiling process management..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c -o $@ $(PROCESS_C)

$(SYNC_OBJ): $(SYNC_C) $(KERNEL_DIR)/sync.h $(KERNEL_DIR)/process.h | $(BUILD_DIR)
	@echo "Compiling synchronization primitives..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c -o $@ $(SYNC_C)

$(FS_OBJ): $(FS_C) $(KERNEL_DIR)/fs.h $(KERNEL_DIR)/memory.h $(KERNEL_HEADER) | $(BUILD_DIR)
	@echo "Compiling filesystem..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c -o $@ $(FS_C)

$(BLOCK_OBJ): $(BLOCK_C) $(KERNEL_DIR)/block.h $(KERNEL_HEADER) | $(BUILD_DIR)
	@echo "Compiling block device layer..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c -o $@ $(BLOCK_C)

$(ATA_OBJ): $(ATA_C) $(DRV_DIR)/ata.h $(KERNEL_DIR)/block.h $(KERNEL_HEADER) | $(BUILD_DIR)
	@echo "Compiling ATA PIO driver..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -I$(SRC_DIR) -c -o $@ $(ATA_C)

$(RAMDISK_OBJ): $(RAMDISK_C) $(DRV_DIR)/ramdisk.h $(KERNEL_DIR)/block.h $(KERNEL_DIR)/memory.h $(KERNEL_HEADER) | $(BUILD_DIR)
	@echo "Compiling RAM disk driver..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -I$(SRC_DIR) -c -o $@ $(RAMDISK_C)

$(OKFS_DISK_OBJ): $(OKFS_DISK_C) $(KERNEL_DIR)/okfs_disk.h $(KERNEL_DIR)/block.h $(KERNEL_DIR)/memory.h $(KERNEL_HEADER) | $(BUILD_DIR)
	@echo "Compiling on-disk OKFS..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -I$(SRC_DIR) -c -o $@ $(OKFS_DISK_C)

$(KERNEL_OBJ): $(KERNEL_C) $(KERNEL_HEADER) | $(BUILD_DIR)
	@echo "Compiling kernel..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c -o $@ $(KERNEL_C)

# Create ISO image (requires grub-mkrescue + xorriso on PATH)
iso: $(KERNEL_ELF)
	@echo "Creating ISO image..."
	@if not exist $(BUILD_DIR)\iso\boot\grub mkdir $(BUILD_DIR)\iso\boot\grub
	@copy /Y $(subst /,\,$(KERNEL_ELF)) $(BUILD_DIR)\iso\boot\kernel.elf >nul
	@copy /Y $(subst /,\,$(BOOT_DIR))\grub.cfg $(BUILD_DIR)\iso\boot\grub\grub.cfg >nul
	@where $(GRUB_MKRESCUE) >nul 2>&1 || (echo. && echo ERROR: $(GRUB_MKRESCUE) not found on PATH. && echo. && echo   Windows: run   make iso-wsl   && echo   WSL install:  sudo apt install grub-pc-bin xorriso && echo   Or skip ISO:  make run   && echo. && echo   See docs/setup/SETUP_WINDOWS.md && exit /b 1)
	@where $(XORRISO) >nul 2>&1 || (echo. && echo ERROR: $(XORRISO) not found on PATH. && echo   Install in WSL:  sudo apt install xorriso && echo   Or use:  make iso-wsl && exit /b 1)
	$(GRUB_MKRESCUE) -o $(ISO) $(BUILD_DIR)/iso
	@if not exist $(ISO) (echo ERROR: $(GRUB_MKRESCUE) did not create $(ISO) && exit /b 1)
	@echo ISO created: $(ISO)

# Build ISO using WSL (recommended on Windows)
iso-wsl: $(KERNEL_ELF)
	@echo "Building ISO via WSL..."
	@wsl -e bash -lc "set -e; command -v grub-mkrescue >/dev/null || { echo 'Installing grub-pc-bin and xorriso in WSL...'; sudo apt-get update -qq && sudo apt-get install -y grub-pc-bin xorriso; }; cd \"$$(wslpath -u '$(CURDIR)')\" && $(MAKE) iso GRUB_MKRESCUE=grub-mkrescue XORRISO=xorriso"

# Run with QEMU (with disk image for ATA testing)
DISK_IMG = $(BUILD_DIR)/disk.img

run: $(KERNEL_ELF) $(DISK_IMG)
	@echo "Starting QEMU..."
	qemu-system-i386 -machine pc -kernel build/kernel.elf -m 256M -vga std -drive file=$(DISK_IMG),format=raw

# Create an empty disk image (32 MB, raw format)
$(DISK_IMG): | $(BUILD_DIR)
	@if not exist $(DISK_IMG) ( \
		echo Creating disk image... && \
		fsutil file createnew $(DISK_IMG) 33554432 >nul \
	) else ( \
		echo Disk image already exists. && \
		echo To recreate: del $(DISK_IMG) && make run \
	)

# Run without disk image (RAM disk only)
run-nodisk: $(KERNEL_ELF)
	@echo "Starting QEMU (no disk)..."
	qemu-system-i386 -machine pc -kernel build/kernel.elf -m 256M -vga std

# Create floppy image (1.44 MB)
FLOPPY_IMG = $(BUILD_DIR)/floppy.img
floppy-img: $(KERNEL_ELF)
	@echo "Creating floppy image..."
	@if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	@fsutil file createnew $(FLOPPY_IMG) 1474560 >nul 2>&1 && echo Floppy image: $(FLOPPY_IMG) || echo Failed to create floppy image

# Run with floppy
run-floppy: $(KERNEL_ELF) $(FLOPPY_IMG)
	@echo "Starting QEMU with floppy..."
	qemu-system-i386 -machine pc -kernel build/kernel.elf -m 256M -vga std -drive file=$(FLOPPY_IMG),format=raw,if=floppy

# Run with ISO (requires GRUB)
run-iso: iso
	@echo "Starting QEMU with ISO..."
	qemu-system-i386 -cdrom $(ISO) -serial stdio

# Check if required tools are available
check-tools:
	@where $(CC) >nul 2>&1 || (echo Error: $(CC) not found && exit /b 1)
	@where $(ASM) >nul 2>&1 || (echo Error: $(ASM) not found && exit /b 1)
	@where $(LD) >nul 2>&1 || (echo Error: $(LD) not found && exit /b 1)
	@where qemu-system-i386 >nul 2>&1 || (echo Error: qemu-system-i386 not found && exit /b 1)
	@echo All required tools found!

check-tools-iso:
	@where $(GRUB_MKRESCUE) >nul 2>&1 && where $(XORRISO) >nul 2>&1 && (echo ISO tools found: $(GRUB_MKRESCUE), $(XORRISO)) || (echo ISO tools NOT found on Windows PATH. && echo Try: make iso-wsl && exit /b 1)

# Clean build artifacts
clean:
	@echo "Cleaning build directory..."
	@if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)
	@echo "Clean complete"

# Help target
help:
	@echo "openkernel Build System"
	@echo "Targets:"
	@echo "  make all        - Build kernel ELF (default)"
	@echo "  make iso        - Create bootable ISO (grub-mkrescue + xorriso)"
	@echo "  make iso-wsl    - Create ISO via WSL (Windows, recommended)"
	@echo "  make run        - Build and run with QEMU (-kernel, no ISO)"
	@echo "  make run-iso    - Build ISO and run with QEMU"
	@echo "  make check-tools-iso - Verify grub-mkrescue and xorriso"
	@echo "  make check-tools - Verify all required tools are installed"
	@echo "  make clean      - Remove build artifacts"
	@echo "  make help       - Show this help message"