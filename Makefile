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

# Output files
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
ISO = $(BUILD_DIR)/openkernel.iso

# Phony targets
.PHONY: all clean run iso check-tools

# Default target
all: $(KERNEL_ELF)

# Ensure build directory exists
$(BUILD_DIR):
	@if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)

# Build kernel ELF executable
$(KERNEL_ELF): $(BOOT_OBJ) $(ISR_OBJ) $(CONTEXT_SWITCH_OBJ) $(IO_OBJ) $(GDT_OBJ) $(IDT_OBJ) $(ISR_SETUP_OBJ) $(TIMER_OBJ) $(KEYBOARD_OBJ) $(DISPLAY_OBJ) $(MEMORY_OBJ) $(PROCESS_OBJ) $(SYNC_OBJ) $(KERNEL_OBJ) | $(BUILD_DIR)
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

$(KERNEL_OBJ): $(KERNEL_C) $(KERNEL_HEADER) | $(BUILD_DIR)
	@echo "Compiling kernel..."
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c -o $@ $(KERNEL_C)

# Create ISO image (requires GRUB)
iso: $(KERNEL_ELF)
	@echo "Creating ISO image..."
	@if not exist $(BUILD_DIR)\iso\boot\grub mkdir $(BUILD_DIR)\iso\boot\grub
	@copy /Y $(subst /,\,$(KERNEL_ELF)) $(BUILD_DIR)\iso\boot\kernel.elf >nul
	@copy /Y $(subst /,\,$(BOOT_DIR))\grub.cfg $(BUILD_DIR)\iso\boot\grub\grub.cfg >nul
	-@grub-mkrescue -o $(ISO) $(BUILD_DIR)/iso 2>nul || echo "Warning: grub-mkrescue not found or failed, install GRUB tools and xorriso"
	@echo "ISO created: $(ISO)"

# Run with QEMU
run: $(KERNEL_ELF)
	@echo "Starting QEMU..."
	qemu-system-i386 -machine pc -kernel build/kernel.elf -m 256M -vga std -k it

# Run with ISO (requires GRUB)
run-iso: iso
	@echo "Starting QEMU with ISO..."
	qemu-system-i386 -cdrom $(ISO) -serial stdio

# Check if required tools are available
check-tools:
	@command -v $(CC) >/dev/null 2>&1 || (echo "Error: $(CC) not found" && exit 1)
	@command -v $(ASM) >/dev/null 2>&1 || (echo "Error: $(ASM) not found" && exit 1)
	@command -v $(LD) >/dev/null 2>&1 || (echo "Error: $(LD) not found" && exit 1)
	@command -v qemu-system-i386 >/dev/null 2>&1 || (echo "Error: qemu-system-i386 not found" && exit 1)
	@echo "All required tools found!"

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
	@echo "  make iso        - Create bootable ISO (requires GRUB)"
	@echo "  make run        - Build and run with QEMU"
	@echo "  make run-iso    - Build ISO and run with QEMU"
	@echo "  make check-tools - Verify all required tools are installed"
	@echo "  make clean      - Remove build artifacts"
	@echo "  make help       - Show this help message"