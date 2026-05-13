/* openkernel - GDT (Global Descriptor Table) */
#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* GDT segment selectors */
#define GDT_CODE_SEG 0x08
#define GDT_DATA_SEG 0x10

/* GDT entry structure (8 bytes) */
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

/* GDT pointer structure (for LGDT instruction) */
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

/* Initialize GDT */
void gdt_init(void);

#endif /* GDT_H */