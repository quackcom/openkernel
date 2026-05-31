# Component: Kernel Core

**Files:** `src/kernel/kernel.c`, `src/kernel/kernel.h`

## Purpose

Central orchestration: boot validation, subsystem init, VGA text console, built-in shell, graphics UI integration.

## `kernel.h` highlights

- **`multiboot_info_t`** — full Multiboot v1 structure (memory, cmdline, framebuffer fields)  
- **`KERNEL_VERSION`** / **`KERNEL_VERSION_STRING`** — e.g. `0.2`  
- Declarations: `kernel_main`, `printk`, `kmalloc`, `halt`, loading status helpers  

## `kernel_main()`

1. Verify Multiboot magic  
2. `kernel_init()` — GDT, IDT, ISRs, timer, keyboard  
3. Keyboard layout from cmdline  
4. `memory_init()`  
5. `fs_init()`  
6. `enable_interrupts()`  
7. `process_init()`  
8. Print boot info, start shell loop  

## `kernel_init()`

Early boot only — does **not** init memory or filesystem. Shows loading ticks for GDT, IDT/PIC, ISRs, PIT, PS/2.

## VGA text console

- Buffer: `0xB8000`, 80×25, attribute `0x07`  
- **Scrollback:** 100 lines in `terminal_history[][]`  
- **Scroll:** arrow up/down adjust `scroll_view_offset`  
- **`printk`:** minimal `%d`, `%x`, `%s`, `%%`  
- **Prompt:** `show_text_prompt()` — inverse-video cursor in input line  

## Loading status API

Used during PMM/VMM init (`memory.c`):

- `loading_status_start(label)`  
- `loading_status_set_progress(0–100)`  
- `loading_status_tick()` — animated dots  
- `loading_status_done()` — replace line with `[OK]`  

## Shell (`execute_console_command`)

Dispatches: `help`, `help --os`, `clear`, `reboot`, `shutdown`, `gfx`, `layout *`, `cmd.print()`, `os.version`, `os.bootinfo`.

Filesystem commands delegated to `fs_handle_command()` via `execute_text_command()`.

## Graphics mode

- `graphics_mode_active`, `pseudo_text_mode_active`  
- `render_pseudo_text_screen()` — full redraw (header + log + prompt)  
- `render_pseudo_text_prompt()` — prompt-only update (no flicker)  
- `render_graphics_test_screen()` — color rects demo  

## Saved boot state

Variables for `os.bootinfo`: `saved_magic`, `saved_flags`, `saved_mem_*`, `saved_boot_loader`.

## Power control

- **Reboot:** `out 0x64, 0xFE` (8042 reset)  
- **Shutdown:** writes to QEMU shutdown ports `0x604`, `0xB004`, `0x4004`  

## Related

- [[Shell-and-Commands]]  
- [[Boot-Process]]  
- [[Component-Display-and-Graphics]]  
