/* openkernel - Display Abstraction Layer */
/* Unified graphics driver supporting multiple backends */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

/* Display mode info */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t  bpp;        /* Bits per pixel */
    uint16_t pitch;      /* Bytes per scanline */
    uint8_t  *addr;      /* Framebuffer physical address */
    uint8_t   type;      /* 0=VGA text, 1=VGA graphics, 2=VBE, 3=Bochs */
    int      available;  /* 1 if display is ready */
} display_mode_t;

/* Display backend identifiers (autodetection priority) */
#define DISP_BOCHS_VBE  3   /* Best: QEMU/VirtualBox */
#define DISP_VESA_VBE   2   /* Good: real BIOS */
#define DISP_VGA_GRAPH  1   /* Fallback: real hardware */
#define DISP_VGA_TEXT    0  /* Always works */

/* Initialize display - tries all backends, picks best */
int display_init(void);

/* Enter graphics mode silently (no printk) for runtime toggle */
int display_enter_gfx(void);

/* Disable graphics and restore VGA text mode */
void display_disable(void);

/* Initialize from Multiboot framebuffer (GRUB on real hardware) */
/* Takes a void* to avoid including multiboot definitions here */
int display_init_multiboot(void *mbd);

/* Get current display info */
const display_mode_t *display_get_mode(void);

/* Core drawing primitives (work regardless of backend) */
void display_clear(uint32_t color);
void display_putpixel(uint32_t x, uint32_t y, uint32_t color);
void display_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void display_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/* Text rendering for graphics mode */
void display_putchar(char c, uint32_t x, uint32_t y, uint32_t color);
void display_puts(const char *str, uint32_t x, uint32_t y, uint32_t color);

/* Create a 32-bit RGB color */
static inline uint32_t display_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (r << 16) | (g << 8) | b;
}

#endif /* DISPLAY_H */