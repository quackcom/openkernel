/* openkernel - Framebuffer Graphics Implementation */
#include "framebuffer.h"

/* Framebuffer info state */
static fb_info_t fb;
static int fb_initialized = 0;

/* Initialize framebuffer from Multiboot info */
void fb_init(multiboot_info_t *mbd)
{
    fb_initialized = 0;

    /* Check if framebuffer info is available (flags bit 12) */
    if (!(mbd->flags & (1 << 12))) {
        return;  /* No framebuffer info available */
    }

    fb.address = (uint8_t *)(uint32_t)mbd->framebuffer_addr_low;
    fb.pitch   = mbd->framebuffer_pitch;
    fb.width   = mbd->framebuffer_width;
    fb.height  = mbd->framebuffer_height;
    fb.bpp     = mbd->framebuffer_bpp;
    fb.type    = mbd->framebuffer_type;

    /* Only support linear graphics mode (type 1) */
    if (fb.type != 1) {
        return;
    }

    /* Only support 32-bit color depth */
    if (fb.bpp != 32) {
        return;
    }

    fb_initialized = 1;
}

/* Initialize framebuffer directly (from VBE fallback) */
void fb_init_direct(uint8_t *address, uint32_t width, uint32_t height,
                    uint16_t pitch, uint8_t bpp)
{
    fb.address = address;
    fb.pitch   = pitch;
    fb.width   = width;
    fb.height  = height;
    fb.bpp     = bpp;
    fb.type    = 1;  /* Linear graphics */
    fb_initialized = (bpp == 32) ? 1 : 0;
}

/* Check if framebuffer is available */
int fb_available(void)
{
    return fb_initialized;
}

/* Get framebuffer info */
const fb_info_t *fb_get_info(void)
{
    return &fb;
}

/* Plot a single pixel */
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (!fb_initialized) return;
    if (x >= fb.width || y >= fb.height) return;

    uint32_t offset = y * fb.pitch + x * 4;  /* 4 bytes per pixel (32-bit) */
    *(uint32_t *)(fb.address + offset) = color;
}

/* Draw a filled rectangle */
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    for (uint32_t row = 0; row < h; row++) {
        for (uint32_t col = 0; col < w; col++) {
            fb_putpixel(x + col, y + row, color);
        }
    }
}

/* Draw a rectangle outline */
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    /* Top and bottom edges */
    for (uint32_t col = 0; col < w; col++) {
        fb_putpixel(x + col, y, color);
        fb_putpixel(x + col, y + h - 1, color);
    }

    /* Left and right edges */
    for (uint32_t row = 0; row < h; row++) {
        fb_putpixel(x, y + row, color);
        fb_putpixel(x + w - 1, y + row, color);
    }
}

/* Clear the screen to a color */
void fb_clear(uint32_t color)
{
    fb_fill_rect(0, 0, fb.width, fb.height, color);
}