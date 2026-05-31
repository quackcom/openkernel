# Component: Display and Graphics

**Files:** `display.c/h`, `font.h` _(legacy `framebuffer.c/h`, `vbe.c/h` superseded into `display.c`)_

> **Consolidation note:** The VBE setup (`vbe.c/h`) and framebuffer drawing (`framebuffer.c/h`) have been merged into a single `display.c/h` module. The old files remain in the source tree for reference but are no longer compiled.

## Purpose

Unified **graphics output** across multiple video backends, plus bitmap font rendering for the graphics shell.

## Display abstraction (`display.c`)

### Backend priority (probe order)

| Priority | Backend | Typical resolution | When |
|----------|---------|-------------------|------|
| 1 | Multiboot VBE | 1024×768×32 | GRUB provides framebuffer |
| 2 | Bochs VBE | 800×600×32 | QEMU `-kernel`, VirtualBox |
| 3 | VGA mode 0x13 | 320×200×8 | Legacy fallback |
| 4 | VGA text | 80×25 | Always available |

`display_mode_t` holds: width, height, bpp, pitch, framebuffer address, type id.

### Drawing API

| Function | Description |
|----------|-------------|
| `display_init()` / `display_enter_gfx()` | Probe and activate best mode |
| `display_disable()` | Restore saved VGA text registers |
| `display_get_mode()` | Current mode pointer |
| `display_clear(color)` | Fill screen |
| `display_putpixel(x, y, color)` | Single pixel |
| `display_fill_rect` / `display_draw_rect` | Rectangles |
| `display_putchar` / `display_puts` | Scaled 8×8 font text |
| `display_rgb(r,g,b)` | 0x00RRGGBB |

### VGA text state

Before switching to graphics, saves VGA sequencer/CRTC/attribute registers. `display_disable()` restores for return to text mode.

### Bochs VBE integration

I/O ports `0x1CE` / `0x1CF` — Bochs/QEMU **dispi** interface (embedded in `display.c`).

Sets resolution, enables linear framebuffer (`0xFD000000` in QEMU).

### Multiboot VBE integration

When GRUB provides a framebuffer, `display_init_multiboot()` parses the Multiboot info structure and uses the pre-configured linear framebuffer directly.

### VGA mode 0x13 fallback

If neither VBE backend is available, a legacy 320×200 256-color mode is programmed via VGA registers.

## Font (`font.h`)

- **IBM CP437** 8×8 bitmap, 128 ASCII glyphs  
- `display.c` scales glyphs **2×** → 16×16 pixels on screen  
- Transparent background (only lit pixels drawn)  

## Graphics shell integration

`kernel.c` uses display API for:

- Color test screen (`gfx` command)  
- Pseudo-text console (blue background, yellow prompt)  
- Partial redraw: `render_pseudo_text_prompt()` for typing  

## Color notes

VGA 256-color mode uses `rgb_to_vga8()` approximation when 8 bpp fallback is active.

## Related

- [[Component-Kernel-Core]]  
- [[Shell-and-Commands]] — Ctrl+T, `gfx`  
- [[Memory-Layout]] — framebuffer MMIO mappings  
