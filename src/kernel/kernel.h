/* openkernel Kernel Header */
#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

/* Multiboot structures and constants */
typedef struct {
    uint32_t magic;
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint32_t vbe_mode;
    uint32_t vbe_interface_seg;
    uint32_t vbe_interface_off;
    uint32_t vbe_interface_len;
    uint32_t framebuffer_addr_low;   /* +0x5C */
    uint32_t framebuffer_addr_high;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  framebuffer_red_field_position;
    uint8_t  framebuffer_red_mask_size;
    uint8_t  framebuffer_green_field_position;
    uint8_t  framebuffer_green_mask_size;
    uint8_t  framebuffer_blue_field_position;
    uint8_t  framebuffer_blue_mask_size;
    uint8_t  framebuffer_reserved_field_position;
    uint8_t  framebuffer_reserved_mask_size;
} multiboot_info_t;

/* Function declarations */
void kernel_main(uint32_t magic, multiboot_info_t *mbd);
void kernel_init(void);

/* Debug printing */
void printk(const char *format, ...);
void putchar(char c);
void puts(const char *str);

/* Memory management (future) */
void *kmalloc(size_t size);
void kfree(void *ptr);

/* Halt the CPU */
void halt(void);

#endif /* KERNEL_H */
