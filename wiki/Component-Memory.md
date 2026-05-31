# Component: Memory Management

**Files:** `src/kernel/memory.c`, `src/kernel/memory.h`

## Purpose

Three-layer memory system:

1. **PMM** — track and allocate 4 KB physical frames  
2. **VMM** — page tables, virtual mapping  
3. **Heap** — `kmalloc` / `kfree` for kernel objects  

## Public API

| API | Description |
|-----|-------------|
| `memory_init(lower, upper)` | Full init from Multiboot RAM size (KB) |
| `pmm_alloc_page()` / `pmm_free_page()` | Physical page |
| `vmm_map_page(phys, virt, flags)` | Map 4 KB |
| `vmm_unmap_page(virt)` | Clear mapping |
| `kmalloc(size)` / `kfree(ptr)` | Heap |
| `memory_get_total()` | Total RAM in KB |

## PMM details

- Bitmap: 1 bit per page, max 4 GB  
- Init phase 1: mark all pages **used** (progress 0–50%)  
- Init phase 2: free pages from `_kernel_end` to top of RAM (50–100%)  
- **`pmm_search_hint`** — O(1) amortized alloc after init  

Pages below kernel end stay reserved (BIOS, loaded ELF, low memory).

## VMM details

- Allocates page directory from PMM  
- **Identity map** first 1024 pages (0–4 MB)  
- Maps physical kernel at 1 MB → virtual `KERNEL_VIRTUAL_BASE` (`0xC0000000`)  
- Maps VGA (`0xB8000`, `0xA0000` region), Bochs VBE (`0xFD000000`, 512 pages)  
- Maps **16384 pages** for heap at `HEAP_START`  
- Sets **CR3**, enables **CR0.PG**  

Progress bar: `loading_status` label **"Memory management"** (PMM) and **"Virtual memory"** (VMM). Both use `busy_delay()` loops to make the animation visible during QEMU boot (~80000 iterations per tick).

## Heap details

- Start: `0xE0000000`, size: 64 MB  
- Blocks: header + payload, best-fit, split on alloc, merge on free  
- Alignment: 16-byte  

## Page flags

`PDE_PRESENT`, `PDE_WRITABLE`, `PDE_USER` (for future user pages).

## Consumers

| Module | Usage |
|--------|-------|
| `process.c` | 4 KB stacks per PCB |
| `fs.c` | File contents, editor buffer |
| `display.c` | May allocate indirectly via mappings |

## Failure modes

- `kmalloc` returns `NULL` — prints "Out of memory"  
- `pmm_alloc_page` failure during heap map — stops mapping, logs index  

## Related

- [[Memory-Layout]]  
- [[Component-Filesystem]]  
- [[Component-Processes-and-Scheduler]]  
