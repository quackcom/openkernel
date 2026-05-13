/* openkernel - Memory Management Implementation */
#include "memory.h"
#include "kernel.h"
#include <stdint.h>
#include <stddef.h>

/* Physical Memory Manager */
#define PMM_MAX_PAGES 0x100000  /* Support up to 4GB RAM */
static uint32_t pmm_bitmap[PMM_MAX_PAGES / 32];
static uint32_t pmm_page_count = 0;
static uint32_t pmm_free_pages = 0;

/* External symbols from linker.ld */
extern uint32_t _kernel_start;
extern uint32_t _kernel_end;

/* Virtual Memory - Page Directory and Tables */
static uint32_t* kernel_page_directory = NULL;
static uint32_t* current_page_directory = NULL;

/* Heap management */
typedef struct heap_block {
    struct heap_block* next;
    size_t size;
    int free;
} heap_block;

static heap_block* heap_start = NULL;
static heap_block* heap_end = NULL;

/* Physical Memory Manager Functions */
static void pmm_set_bit(uint32_t index, int value) {
    if (index >= PMM_MAX_PAGES) return;
    uint32_t* word = &pmm_bitmap[index / 32];
    uint32_t bit = index % 32;
    if (value) {
        *word |= (1 << bit);
    } else {
        *word &= ~(1 << bit);
    }
}

static int pmm_get_bit(uint32_t index) {
    if (index >= PMM_MAX_PAGES) return 1;
    uint32_t* word = &pmm_bitmap[index / 32];
    uint32_t bit = index % 32;
    return (*word >> bit) & 1;
}

static uint32_t pmm_find_free_page(void) {
    for (uint32_t i = 0; i < pmm_page_count; i++) {
        if (!pmm_get_bit(i)) {
            return i;
        }
    }
    return PMM_MAX_PAGES;  /* Not found */
}

void pmm_init(uint32_t mem_lower, uint32_t mem_upper) {
    /* Calculate total memory in KB */
    uint32_t total_mem_kb = mem_lower + mem_upper;
    pmm_page_count = total_mem_kb * 1024 / PAGE_SIZE;

    /* Initialize all pages as used */
    for (uint32_t i = 0; i < PMM_MAX_PAGES; i++) {
        pmm_set_bit(i, 1);
    }

    /* Free available pages (from 1MB up to detected memory limit) */
    /* Skip initial 1MB (BIOS, kernel, etc.) */
    
    /* IMPORTANT: We must start freeing memory AFTER the kernel ends. */
    /* We add a small buffer (e.g., 4KB) to be safe. */
    uint32_t kernel_end_phys = (uint32_t)&_kernel_end;
    uint32_t start_page_to_free = (kernel_end_phys + PAGE_SIZE) / PAGE_SIZE;

    pmm_free_pages = 0;
    for (uint32_t i = start_page_to_free; i < pmm_page_count; i++) {
        pmm_set_bit(i, 0);
        pmm_free_pages++;
    }

    printk("Memory: %d KB (%d total pages, %d free pages)\n",
           total_mem_kb, pmm_page_count, pmm_free_pages);
}

void* pmm_alloc_page(void) {
    uint32_t index = pmm_find_free_page();
    if (index >= pmm_page_count) {
        printk("PMM: Out of memory!\n");
        return NULL;
    }

    pmm_set_bit(index, 1);
    pmm_free_pages--;
    return (void*)(index * PAGE_SIZE);
}

void pmm_free_page(void* addr) {
    uint32_t index = (uint32_t)addr / PAGE_SIZE;
    if (index >= pmm_page_count) return;

    pmm_set_bit(index, 0);
    pmm_free_pages++;
}

/* Virtual Memory Manager Functions */
static __attribute__((unused)) void* vmm_get_pde(void* virt) {
    uint32_t pd_index = (uint32_t)virt >> 22;
    return &kernel_page_directory[pd_index];
}

static void* vmm_get_pte(void* virt) {
    uint32_t pd_index = (uint32_t)virt >> 22;
    uint32_t pt_index = ((uint32_t)virt >> 12) & 0x3FF;

    uint32_t* pd_entry = &kernel_page_directory[pd_index];
    if (!(*pd_entry & PDE_PRESENT)) {
        return NULL;
    }

    uint32_t* page_table = (uint32_t*)(*pd_entry & ~0xFFF);
    return &page_table[pt_index];
}

void vmm_map_page(void* phys, void* virt, uint32_t flags) {
    uint32_t pd_index = (uint32_t)virt >> 22;
    uint32_t pt_index = ((uint32_t)virt >> 12) & 0x3FF;

    /* Check if page directory entry exists */
    if (!(kernel_page_directory[pd_index] & PDE_PRESENT)) {
        /* Allocate new page table */
        void* new_pt = pmm_alloc_page();
        if (!new_pt) {
            printk("VMM: Failed to allocate page table!\n");
            return;
        }

        /* Clear page table */
        for (int i = 0; i < 1024; i++) {
            ((uint32_t*)new_pt)[i] = 0;
        }

        /* Map page table in directory */
        kernel_page_directory[pd_index] = (uint32_t)new_pt | PDE_PRESENT | PDE_WRITABLE;
    }

    /* Get page table */
    uint32_t* page_table = (uint32_t*)(kernel_page_directory[pd_index] & ~0xFFF);

    /* Map the page */
    page_table[pt_index] = (uint32_t)phys | flags | PDE_PRESENT;

    /* Invalidate single TLB entry instead of full CR3 reload for performance */
    asm volatile("invlpg (%0)" :: "r" (virt) : "memory");
}

void vmm_unmap_page(void* virt) {
    uint32_t* pte = vmm_get_pte(virt);
    if (pte) {
        *pte = 0;
    /* Invalidate single TLB entry for performance */
    asm volatile("invlpg (%0)" :: "r" (virt) : "memory");
    }
}

void vmm_init(void) {
    /* Allocate page directory */
    kernel_page_directory = (uint32_t*)PAGE_ALIGN_UP((uint32_t)pmm_alloc_page());
    current_page_directory = kernel_page_directory;

    /* Clear page directory */
    for (int i = 0; i < 1024; i++) {
        kernel_page_directory[i] = 0;
    }

    /* Identity map first 4MB (for early boot) */
    for (uint32_t i = 0; i < 1024; i++) {
        vmm_map_page((void*)(i * PAGE_SIZE), (void*)(i * PAGE_SIZE),
                    PDE_WRITABLE | PDE_PRESENT);
    }

    /* Map higher-half kernel to existing physical kernel (at 1MB) */
    for (uint32_t i = 0; i < 0x100000; i += PAGE_SIZE) {
        vmm_map_page((void*)(0x100000 + i), (void*)(KERNEL_VIRTUAL_BASE + i), PDE_WRITABLE | PDE_PRESENT);
    }

    /* Map framebuffer regions directly (identity map for graphics) */
    /* VGA text buffer at 0xB8000 */
    vmm_map_page((void*)0xB8000, (void*)0xB8000, PDE_WRITABLE | PDE_PRESENT);
    /* VGA graphics buffer at 0xA0000 (Mode 0x13) - Map 64KB (16 pages) */
    for (uint32_t i = 0; i < 16; i++) {
        uint32_t addr = 0xA0000 + (i * PAGE_SIZE);
        vmm_map_page((void*)addr, (void*)addr, PDE_WRITABLE | PDE_PRESENT);
    }
    /* Bochs VBE framebuffer at 0xFD000000 - map 2MB for 800x600x32 */
    for (uint32_t page = 0; page < 512; page++) {
        uint32_t phys_addr = 0xFD000000 + (page * 0x1000);
        vmm_map_page((void*)phys_addr, (void*)phys_addr, PDE_WRITABLE | PDE_PRESENT);
    }

    /* Map heap region */
    /* The heap starts at HEAP_START and spans HEAP_SIZE bytes.
       Allocate and map each page of the heap into the virtual address space.
       This ensures that kmalloc can access the heap memory.
    */
    uint32_t heap_pages = HEAP_SIZE / PAGE_SIZE;
    for (uint32_t i = 0; i < heap_pages; i++) {
        void* phys = pmm_alloc_page();
        if (!phys) {
            printk("VMM: Failed to allocate page for heap mapping at index %u\n", i);
            break;
        }
        void* virt = (void*)(HEAP_START + i * PAGE_SIZE);
        vmm_map_page(phys, virt, PDE_WRITABLE | PDE_PRESENT);
    }

    /* Enable paging */
    asm volatile ("mov %0, %%cr3" : : "r"(kernel_page_directory));
    uint32_t cr0;
    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;  /* Enable paging */
    asm volatile ("mov %0, %%cr0" : : "r"(cr0));

    printk("VMM: Paging enabled, kernel mapped at 0x%x\n", KERNEL_VIRTUAL_BASE);
}

/* Heap Allocator */
void heap_init(void) {
    if (HEAP_SIZE <= sizeof(heap_block)) {
        printk("Heap: Error - HEAP_SIZE (%d) is too small for headers!\n", HEAP_SIZE);
        return;
    }

    /* Initialize heap at HEAP_START */
    heap_start = (heap_block*)HEAP_START;
    heap_end = (heap_block*)HEAP_START;

    /* Create initial free block */
    heap_start->size = HEAP_SIZE - sizeof(heap_block);
    heap_start->free = 1;
    heap_start->next = NULL;

    printk("Heap: Initialized at 0x%x, size %d MB\n", HEAP_START, HEAP_SIZE / (1024 * 1024));
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    /* Align size to 16 bytes */
    size = (size + 15) & ~15;

    heap_block* current = heap_start;
    heap_block* best_fit = NULL;

    /* Find best fit block */
    while (current) {
        if (current->free && current->size >= size) {
            if (!best_fit || current->size < best_fit->size) {
                best_fit = current;
            }
        }
        current = current->next;
    }

    if (!best_fit) {
        printk("kmalloc: Out of memory!\n");
        return NULL;
    }

    /* Split block if there's enough space left */
    if (best_fit->size > size + sizeof(heap_block) + 16) {
        heap_block* new_block = (heap_block*)((uint8_t*)best_fit + sizeof(heap_block) + size);
        new_block->size = best_fit->size - size - sizeof(heap_block);
        new_block->free = 1;
        new_block->next = best_fit->next;

        best_fit->size = size;
        best_fit->next = new_block;
    }

    best_fit->free = 0;
    return (void*)((uint8_t*)best_fit + sizeof(heap_block));
}

void kfree(void* ptr) {
    if (!ptr) return;

    heap_block* block = (heap_block*)((uint8_t*)ptr - sizeof(heap_block));
    block->free = 1;

    /* Merge with next block if free */
    if (block->next && block->next->free) {
        block->size += sizeof(heap_block) + block->next->size;
        block->next = block->next->next;
    }
}

/* Memory Management Initialization */
void memory_init(uint32_t mem_lower, uint32_t mem_upper) {
    printk("Initializing memory management...\n");

    /* Initialize physical memory manager */
    pmm_init(mem_lower, mem_upper);

    /* Initialize virtual memory manager */
    vmm_init();

    /* Initialize heap */
    heap_init();

    printk("Memory management initialized.\n");
}

uint32_t memory_get_total(void) {
    return pmm_page_count * PAGE_SIZE / 1024;  /* Return in KB */
}