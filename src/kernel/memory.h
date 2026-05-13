/* openkernel - Memory Management Header */
#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* Page size (4KB) */
#define PAGE_SIZE 4096
#define PAGE_ALIGN_UP(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_DOWN(addr) ((addr) & ~(PAGE_SIZE - 1))

/* Memory regions */
#define KERNEL_VIRTUAL_BASE 0xC0000000  /* 3GB - Kernel space start */
#define KERNEL_STACK_SIZE   0x4000      /* 16KB kernel stack */
#define HEAP_START          0xE0000000  /* Heap starts at 3.5GB */
#define HEAP_SIZE           0x04000000  /* 64MB heap size */

/* Page table entries */
#define PDE_PRESENT   0x001
#define PDE_WRITABLE  0x002
#define PDE_USER      0x004
#define PDE_PWT       0x008  /* Page-level write-through */
#define PDE_PCD       0x010  /* Page-level cache disable */
#define PDE_ACCESSED  0x020
#define PDE_DIRTY     0x040
#define PDE_4MB       0x080  /* Page size (0 = 4KB, 1 = 4MB) */
#define PDE_GLOBAL    0x100  /* Global TLB entry */

/* Initialize memory management system */
void memory_init(uint32_t mem_lower, uint32_t mem_upper);

/* Physical memory management */
void* pmm_alloc_page(void);
void pmm_free_page(void* addr);
void pmm_init(uint32_t mem_lower, uint32_t mem_upper);

/* Virtual memory management */
void vmm_map_page(void* phys, void* virt, uint32_t flags);
void vmm_unmap_page(void* virt);
void vmm_init(void);

/* Heap allocation */
void* kmalloc(size_t size);
void kfree(void* ptr);
void heap_init(void);

/* Get total memory */
uint32_t memory_get_total(void);

#endif /* MEMORY_H */