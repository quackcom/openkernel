/* openkernel - Bochs VBE Display Interface */
#ifndef VBE_H
#define VBE_H

#include <stdint.h>

/* VBE mode information */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t bpp;
    uint16_t pitch;
    uint8_t  *framebuffer;
} vbe_mode_t;

/* Initialize VBE (Bochs VBE interface, works in QEMU -kernel mode) */
/* Returns 1 on success, 0 on failure */
int vbe_init(uint16_t width, uint16_t height, uint16_t bpp);

/* Get current VBE mode info */
const vbe_mode_t *vbe_get_mode(void);

#endif /* VBE_H */