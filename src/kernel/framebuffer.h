/* openkernel - Framebuffer Graphics Driver */
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include "kernel.h"

/* Framebuffer information structure */
typedef struct {
    uint8_t  *address;       /* Physical framebuffer address */
    uint32_t  pitch;         /* Bytes per scanline */
    uint32_t  width;         /* Width in pixels */
    uint32_t  height;        /* Height in pixels */
    uint8_t   bpp;           /* Bits per pixel */
    uint8_t   type;          /* 0 = text, 1 = linear graphics */
} fb_info_t;

/* Initialize framebuffer from Multiboot info */
void fb_init(multiboot_info_t *mbd);

/* Initialize framebuffer directly (from VBE fallback) */
void fb_init_direct(uint8_t *address, uint32_t width, uint32_t height,
                    uint16_t pitch, uint8_t bpp);

/* Check if framebuffer is available */
int fb_available(void);

/* Get framebuffer info */
const fb_info_t *fb_get_info(void);

/* Plot a single pixel */
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);

/* Draw a filled rectangle */
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/* Draw a rectangle outline */
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/* Clear the screen to a color */
void fb_clear(uint32_t color);

/* Create a 32-bit RGBA color */
static inline uint32_t fb_color(uint8_t r, uint8_t g, uint8_t b)
{
    return (r << 16) | (g << 8) | b;
}

#endif /* FRAMEBUFFER_H */