/* openkernel - GDT Implementation */
#include "gdt.h"

/* GDT entries: null, code, data */
static gdt_entry_t gdt_entries[3];
static gdt_ptr_t   gdt_ptr;

/* Set a GDT entry */
static void gdt_set_entry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    gdt_entries[num].base_low    = base & 0xFFFF;
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = limit & 0xFFFF;
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;
    gdt_entries[num].granularity |= gran & 0xF0;

    gdt_entries[num].access = access;
}

/* Initialize GDT */
void gdt_init(void)
{
    /* Setup GDT pointer */
    gdt_ptr.limit = sizeof(gdt_entries) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    /* Null descriptor (required) */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* Code segment: base=0, limit=4GB, ring 0, code, 32-bit */
    gdt_set_entry(1, 0, 0xFFFFFFFF,
        0x9A,  /* Present | Ring 0 | Code segment | Execute/Read */
        0xCF); /* 4KB granularity | 32-bit | 4GB limit */

    /* Data segment: base=0, limit=4GB, ring 0, data, 32-bit */
    gdt_set_entry(2, 0, 0xFFFFFFFF,
        0x92,  /* Present | Ring 0 | Data segment | Read/Write */
        0xCF); /* 4KB granularity | 32-bit | 4GB limit */

    /* Load the GDT */
    asm volatile ("lgdt %0" : : "m" (gdt_ptr));

    /* Reload segment registers */
    asm volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "ljmp $0x08, $1f\n"
        "1:\n"
        :
        :
        : "eax"
    );
}