/* openkernel Linker Script */
/* Maps kernel sections to proper memory locations */

/* Entry point for bootloader */
ENTRY(multiboot_entry)

/* Memory layout:
   0x00000000 - 0x00400000  : Bootloader and BIOS reserved
   0x00100000 - 0x00200000  : Kernel code (.text) - linked at 1MB
   The Multiboot header must be in the first 8KB of the kernel
*/

SECTIONS
{
    /* Kernel starts at 1MB (0x100000) - standard location for Multiboot kernels */
    . = 0x00100000;
    
    /* Kernel starts here */
    _kernel_start = .;
    
    /* .text section (code) - must come first */
    .text ALIGN(0x1000) :
    {
        *(.multiboot_header)    /* Multiboot header must be first */
        *(.text)                /* All .text sections */
    }
    
    /* .rodata section (read-only data) */
    .rodata ALIGN(0x1000) :
    {
        *(.rodata)
    }
    
    /* .data section (initialized data) */
    .data ALIGN(0x1000) :
    {
        *(.data)
    }
    
    /* .bss section (uninitialized data) - must not be included in kernel file */
    .bss ALIGN(0x1000) :
    {
        *(COMMON)
        *(.bss)
    }
    
    /* Kernel ends here */
    _kernel_end = .;
}
