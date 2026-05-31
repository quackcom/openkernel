# Memory Layout

How openkernel uses physical and virtual address space.

## Physical map (conceptual)

| Region | Address | Use |
|--------|---------|-----|
| BIOS / IVT | `0x00000000` – `0x000003FF` | Legacy |
| Boot sector (optional) | `0x00007C00` | MBR path only |
| VGA text buffer | `0x000B8000` | 80×25 cells |
| VGA graphics | `0x000A0000` | Mode 0x13 framebuffer |
| **Kernel load** | **`0x00100000`** | ELF linked here (`linker.ld`) |
| Free RAM | After `_kernel_end` | PMM allocatable pages |

## Virtual map (after `vmm_init`)

| Virtual range | Maps to | Purpose |
|---------------|---------|---------|
| `0x00000000` – `0x003FFFFF` | Identity (low 4 MB) | Boot compatibility, hardware |
| `0x000B8000`, `0x000A0000` | Same physical | VGA |
| `0xC0000000` + | Kernel physical @ 1 MB | Higher-half kernel |
| `0xE0000000` – `0xE3FFFFFF` | Allocated pages | **64 MB heap** |
| `0xFD000000` (2 MB) | Identity | Bochs VBE framebuffer (QEMU) |

Constants in `memory.h`:

```c
#define KERNEL_VIRTUAL_BASE  0xC0000000
#define HEAP_START           0xE0000000
#define HEAP_SIZE            0x04000000   /* 64 MB */
#define PAGE_SIZE            4096
```

## Linker script (`linker.ld`)

- **Entry:** `multiboot_entry`  
- **Load address:** `0x00100000`  
- Sections: multiboot header → `.text` → `.rodata` → `.data` → `.bss`  
- Symbols: `_kernel_start`, `_kernel_end` (used by PMM to reserve kernel pages)

## Physical Memory Manager (PMM)

- **Structure:** bitmap, 1 bit per 4 KB page  
- **Capacity:** up to 4 GB RAM (`PMM_MAX_PAGES`)  
- **Init:** mark all used → free pages from `_kernel_end` upward  
- **Alloc:** `pmm_alloc_page()` — search hint for speed  
- **Free:** `pmm_free_page()` — clears bit, updates hint  

## Virtual Memory Manager (VMM)

- One **page directory** (1024 PDEs)  
- On-demand **page tables** (1024 PTEs each)  
- `vmm_map_page(phys, virt, flags)` — sets PTE, `invlpg`  
- Enables paging via **CR3** + **CR0.PG**

Heap mapping: each heap page gets a fresh physical page from PMM.

## Heap allocator

- **Algorithm:** singly linked free list, best-fit  
- **Block header:** `heap_block` (next, size, free flag)  
- **API:** `kmalloc(size)`, `kfree(ptr)`  
- **Used by:** process stacks, OKFS file data, editor buffer  

## Per-process memory (current)

All processes share one address space (no per-process page tables yet). `pcb_t.cr3` is reserved for future user isolation.

Each process has a **4 KB kernel stack** allocated with `kmalloc`.

## OKFS memory

- Up to **128** nodes (files + directories)  
- File payload up to **64 KB** each in heap  
- Editor buffer up to **64 KB** during `edit`  

## Related

- [[Component-Memory]]  
- [[Component-Filesystem]]  
- [[Architecture-Overview]]  
