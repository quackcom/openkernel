/* openkernel - Display Abstraction Layer Implementation */
/* Auto-detects best available graphics mode:
 *   1. Multiboot VBE (GRUB on real hardware) — 1024x768x32
 *   2. Bochs VBE (QEMU / VirtualBox)          — 800x600x32
 *   3. VGA mode 0x13 (real hardware fallback) — 320x200x8
 */

#include "display.h"
#include "kernel.h"
#include "font.h"

/* Framebuffer and mode state */
static display_mode_t current_mode;
static int vga_text_state_saved = 0;

typedef struct {
    uint8_t misc;
    uint8_t seq[5];
    uint8_t crtc[25];
    uint8_t gc[9];
    uint8_t ac[21];
} vga_text_state_t;

static vga_text_state_t saved_text_state;

/* Simple mapper to convert RGB888 to standard VGA 256-color indices */
static uint8_t rgb_to_vga8(uint32_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    if (r > 200 && g > 200 && b > 200) return 15; /* White */
    if (r < 50 && g < 50 && b < 50)    return 0;  /* Black */
    if (r > 200 && g < 100 && b < 100) return 4;  /* Red */
    if (g > 200 && r < 100 && b < 100) return 2;  /* Green */
    if (b > 200 && r < 100 && g < 100) return 1;  /* Blue */

    return (uint8_t)(((r >> 6) << 5) | ((g >> 6) << 2) | (b >> 7));
}

/* ================================================================
 * Backend 1: Multiboot VBE (provided by GRUB on real hardware)
 * ================================================================ */
static int probe_multiboot_vbe(multiboot_info_t *mbd)
{
    if (!(mbd->flags & (1 << 12))) return 0;

    uint8_t type = mbd->framebuffer_type;
    if (type != 1) return 0;

    uint8_t bpp = mbd->framebuffer_bpp;
    if (bpp != 32 && bpp != 24 && bpp != 16) return 0;

    current_mode.width     = mbd->framebuffer_width;
    current_mode.height    = mbd->framebuffer_height;
    current_mode.bpp       = bpp;
    current_mode.pitch     = mbd->framebuffer_pitch;
    current_mode.addr      = (uint8_t *)(uint32_t)mbd->framebuffer_addr_low;
    current_mode.type      = DISP_VESA_VBE;
    current_mode.available = 1;

    printk("  [VBE] Multiboot VBE: %dx%d %dbpp\n",
           current_mode.width, current_mode.height, current_mode.bpp);
    return 1;
}

/* ================================================================
 * Backend 2: Bochs VBE (QEMU -kernel mode)
 * ================================================================ */
#define VBE_INDEX_PORT 0x01CE
#define VBE_DATA_PORT  0x01CF

#define VBE_DISPI_INDEX_ID      0
#define VBE_DISPI_INDEX_XRES    1
#define VBE_DISPI_INDEX_YRES    2
#define VBE_DISPI_INDEX_BPP     3
#define VBE_DISPI_INDEX_ENABLE  4

#define VBE_DISPI_DISABLED     0x00
#define VBE_DISPI_ENABLED      0x01
#define VBE_DISPI_LFB_ENABLED  0x40
#define VBE_DISPI_ID4          0xB0C4

static void vbe_write(uint16_t idx, uint16_t val)
{
    __asm__ volatile ("outw %0, %1" : : "a"(idx), "d"((uint16_t)VBE_INDEX_PORT));
    __asm__ volatile ("outw %0, %1" : : "a"(val), "d"((uint16_t)VBE_DATA_PORT));
}

static uint16_t vbe_read(uint16_t idx)
{
    uint16_t val;
    __asm__ volatile ("outw %0, %1" : : "a"(idx), "d"((uint16_t)VBE_INDEX_PORT));
    __asm__ volatile ("inw %1, %0"  : "=a"(val) : "d"((uint16_t)VBE_DATA_PORT));
    return val;
}

static int probe_bochs_vbe(void)
{
    vbe_write(VBE_DISPI_INDEX_ID, VBE_DISPI_ID4);
    if (vbe_read(VBE_DISPI_INDEX_ID) != VBE_DISPI_ID4) {
        vbe_write(VBE_DISPI_INDEX_ID, 0xB0C0);
        if (vbe_read(VBE_DISPI_INDEX_ID) != 0xB0C0) return 0;
    }
    return 1;
}

static int bochs_set_mode(uint16_t w, uint16_t h, uint16_t bpp)
{
    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    vbe_write(VBE_DISPI_INDEX_XRES, w);
    vbe_write(VBE_DISPI_INDEX_YRES, h);
    vbe_write(VBE_DISPI_INDEX_BPP,  bpp);
    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    if (vbe_read(VBE_DISPI_INDEX_XRES) != w ||
        vbe_read(VBE_DISPI_INDEX_YRES) != h ||
        vbe_read(VBE_DISPI_INDEX_BPP)  != bpp)
        return 0;

    uint32_t fb_addr = 0xFD000000;

    current_mode.width     = w;
    current_mode.height    = h;
    current_mode.bpp       = bpp;
    current_mode.pitch     = w * (bpp / 8);
    current_mode.addr      = (uint8_t *)(uint32_t)fb_addr;
    current_mode.type      = DISP_BOCHS_VBE;
    current_mode.available = 1;

    return 1;
}

/* ================================================================
 * Backend 3: VGA Mode 0x13 (universal fallback)
 * ================================================================ */
static int probe_vga_mode13(void)
{
    uint8_t *vga_test = (uint8_t *)0xA0000;
    volatile uint8_t old = *vga_test;
    vga_test[0] = 0xAA;
    __asm__ volatile("" ::: "memory");
    int writable = (vga_test[0] == 0xAA);
    vga_test[0] = old;

    if (!writable) return 0;

    static const uint8_t seq[] = { 0x03, 0x01, 0x0F, 0x00, 0x0E };
    static const uint8_t crtc[] = {
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
        0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3, 0xFF
    };
    static const uint8_t gc[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF };
    static const uint8_t ac[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x41, 0x00, 0x0F, 0x00, 0x00
    };

    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x63), "Nd"((uint16_t)0x3C2));

    for (uint8_t i = 0; i < 5; i++) {
        __asm__ volatile ("outb %0, %1" : : "a"(i), "Nd"((uint16_t)0x3C4));
        __asm__ volatile ("outb %0, %1" : : "a"(seq[i]), "Nd"((uint16_t)0x3C5));
    }

    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x03), "Nd"((uint16_t)0x3D4));
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"((uint16_t)0x3D5));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)(v | 0x80)), "Nd"((uint16_t)0x3D5));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x11), "Nd"((uint16_t)0x3D4));
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"((uint16_t)0x3D5));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)(v & ~0x80)), "Nd"((uint16_t)0x3D5));

    for (uint8_t i = 0; i < 25; i++) {
        __asm__ volatile ("outb %0, %1" : : "a"(i), "Nd"((uint16_t)0x3D4));
        __asm__ volatile ("outb %0, %1" : : "a"(crtc[i]), "Nd"((uint16_t)0x3D5));
    }

    for (uint8_t i = 0; i < 9; i++) {
        __asm__ volatile ("outb %0, %1" : : "a"(i), "Nd"((uint16_t)0x3CE));
        __asm__ volatile ("outb %0, %1" : : "a"(gc[i]), "Nd"((uint16_t)0x3CF));
    }

    for (uint8_t i = 0; i < 21; i++) {
        __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"((uint16_t)0x3DA));
        __asm__ volatile ("outb %0, %1" : : "a"(i), "Nd"((uint16_t)0x3C0));
        __asm__ volatile ("outb %0, %1" : : "a"(ac[i]), "Nd"((uint16_t)0x3C0));
    }

    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"((uint16_t)0x3DA));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x3C0));

    current_mode.width     = 320;
    current_mode.height    = 200;
    current_mode.bpp       = 8;
    current_mode.pitch     = 320;
    current_mode.addr      = (uint8_t *)0xA0000;
    current_mode.type      = DISP_VGA_GRAPH;
    current_mode.available = 1;

    printk("  [VGA] Mode 0x13: 320x200 256 colors (VGA legacy)\n");
    return 1;
}

/* ================================================================
 * Backend 0: VGA Text Mode (always works)
 * ================================================================ */
static void probe_vga_text(void)
{
    current_mode.width     = 80;
    current_mode.height    = 25;
    current_mode.bpp       = 0;
    current_mode.pitch     = 160;
    current_mode.addr      = (uint8_t *)0xB8000;
    current_mode.type      = DISP_VGA_TEXT;
    current_mode.available = 1;

    printk("  [VGA] Text mode: 80x25\n");
}

/* ================================================================
 * Public API
 * ================================================================ */

static void vga_set_cursor_visible(int visible)
{
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x0A), "Nd"((uint16_t)0x3D4));
    if (visible) {
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x0D), "Nd"((uint16_t)0x3D5));
    } else {
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x3D5));
    }

    if (!visible) {
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x0E), "Nd"((uint16_t)0x3D4));
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFF), "Nd"((uint16_t)0x3D5));
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x0F), "Nd"((uint16_t)0x3D4));
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFF), "Nd"((uint16_t)0x3D5));
    }
}

static void __attribute__((unused)) vga_set_text_mode_80x25(void)
{
    static const uint8_t seq_regs[5] = {
        0x03, 0x00, 0x03, 0x00, 0x07
    };
    static const uint8_t crtc_regs[25] = {
        0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
        0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x50,
        0x9C, 0x8E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
        0xFF
    };
    static const uint8_t gc_regs[9] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF
    };
    static const uint8_t ac_regs[21] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x0C, 0x00, 0x0F, 0x08, 0x00
    };

    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x67), "Nd"((uint16_t)0x3C2));

    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x00), "Nd"((uint16_t)0x3C4));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x01), "Nd"((uint16_t)0x3C5));

    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x01), "Nd"((uint16_t)0x3C4));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x3C5));

    for (uint8_t i = 1; i < 5; i++) {
        __asm__ volatile ("outb %0, %1" : : "a"(i), "Nd"((uint16_t)0x3C4));
        __asm__ volatile ("outb %0, %1" : : "a"(seq_regs[i]), "Nd"((uint16_t)0x3C5));
    }

    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x00), "Nd"((uint16_t)0x3C4));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x03), "Nd"((uint16_t)0x3C5));

    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x11), "Nd"((uint16_t)0x3D4));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x0E), "Nd"((uint16_t)0x3D5));

    for (uint8_t i = 0; i < 25; i++) {
        __asm__ volatile ("outb %0, %1" : : "a"(i), "Nd"((uint16_t)0x3D4));
        __asm__ volatile ("outb %0, %1" : : "a"(crtc_regs[i]), "Nd"((uint16_t)0x3D5));
    }

    for (uint8_t i = 0; i < 9; i++) {
        __asm__ volatile ("outb %0, %1" : : "a"(i), "Nd"((uint16_t)0x3CE));
        __asm__ volatile ("outb %0, %1" : : "a"(gc_regs[i]), "Nd"((uint16_t)0x3CF));
    }

    uint8_t temp;
    for (uint8_t i = 0; i < 21; i++) {
        __asm__ volatile ("inb %1, %0" : "=a"(temp) : "Nd"((uint16_t)0x3DA));
        __asm__ volatile ("outb %0, %1" : : "a"(i), "Nd"((uint16_t)0x3C0));
        __asm__ volatile ("outb %0, %1" : : "a"(ac_regs[i]), "Nd"((uint16_t)0x3C0));
    }

    __asm__ volatile ("inb %1, %0" : "=a"(temp) : "Nd"((uint16_t)0x3DA));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x3C0));
}

static void __attribute__((unused)) vga_restore_default_dac_16(void)
{
    /* Standard EGA/VGA 16-color palette values (6-bit DAC channels). */
    static const uint8_t pal[16][3] = {
        {0x00,0x00,0x00}, {0x00,0x00,0x2A}, {0x00,0x2A,0x00}, {0x00,0x2A,0x2A},
        {0x2A,0x00,0x00}, {0x2A,0x00,0x2A}, {0x2A,0x15,0x00}, {0x2A,0x2A,0x2A},
        {0x15,0x15,0x15}, {0x15,0x15,0x3F}, {0x15,0x3F,0x15}, {0x15,0x3F,0x3F},
        {0x3F,0x15,0x15}, {0x3F,0x15,0x3F}, {0x3F,0x3F,0x15}, {0x3F,0x3F,0x3F}
    };

    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x00), "Nd"((uint16_t)0x3C8));
    for (uint8_t i = 0; i < 16; i++) {
        __asm__ volatile ("outb %0, %1" : : "a"(pal[i][0]), "Nd"((uint16_t)0x3C9));
        __asm__ volatile ("outb %0, %1" : : "a"(pal[i][1]), "Nd"((uint16_t)0x3C9));
        __asm__ volatile ("outb %0, %1" : : "a"(pal[i][2]), "Nd"((uint16_t)0x3C9));
    }
}

static uint8_t vga_read_seq(uint8_t idx)
{
    uint8_t v;
    __asm__ volatile ("outb %0, %1" : : "a"(idx), "Nd"((uint16_t)0x3C4));
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"((uint16_t)0x3C5));
    return v;
}

static uint8_t vga_read_crtc(uint8_t idx)
{
    uint8_t v;
    __asm__ volatile ("outb %0, %1" : : "a"(idx), "Nd"((uint16_t)0x3D4));
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"((uint16_t)0x3D5));
    return v;
}

static uint8_t vga_read_gc(uint8_t idx)
{
    uint8_t v;
    __asm__ volatile ("outb %0, %1" : : "a"(idx), "Nd"((uint16_t)0x3CE));
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"((uint16_t)0x3CF));
    return v;
}

static uint8_t vga_read_ac(uint8_t idx)
{
    uint8_t v;
    uint8_t tmp;
    __asm__ volatile ("inb %1, %0" : "=a"(tmp) : "Nd"((uint16_t)0x3DA));
    __asm__ volatile ("outb %0, %1" : : "a"(idx), "Nd"((uint16_t)0x3C0));
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"((uint16_t)0x3C1));
    return v;
}

static void vga_write_seq(uint8_t idx, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(idx), "Nd"((uint16_t)0x3C4));
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"((uint16_t)0x3C5));
}

static void vga_write_crtc(uint8_t idx, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(idx), "Nd"((uint16_t)0x3D4));
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"((uint16_t)0x3D5));
}

static void vga_write_gc(uint8_t idx, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(idx), "Nd"((uint16_t)0x3CE));
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"((uint16_t)0x3CF));
}

static void vga_write_ac(uint8_t idx, uint8_t val)
{
    uint8_t tmp;
    __asm__ volatile ("inb %1, %0" : "=a"(tmp) : "Nd"((uint16_t)0x3DA));
    __asm__ volatile ("outb %0, %1" : : "a"(idx), "Nd"((uint16_t)0x3C0));
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"((uint16_t)0x3C0));
}

static void vga_snapshot_text_state(void)
{
    if (vga_text_state_saved) return;

    __asm__ volatile ("inb %1, %0" : "=a"(saved_text_state.misc) : "Nd"((uint16_t)0x3CC));
    for (uint8_t i = 0; i < 5; i++) saved_text_state.seq[i] = vga_read_seq(i);
    for (uint8_t i = 0; i < 25; i++) saved_text_state.crtc[i] = vga_read_crtc(i);
    for (uint8_t i = 0; i < 9; i++) saved_text_state.gc[i] = vga_read_gc(i);
    for (uint8_t i = 0; i < 21; i++) saved_text_state.ac[i] = vga_read_ac(i);

    vga_text_state_saved = 1;
}

static void vga_restore_text_state(void)
{
    if (!vga_text_state_saved) return;

    __asm__ volatile ("outb %0, %1" : : "a"(saved_text_state.misc), "Nd"((uint16_t)0x3C2));

    for (uint8_t i = 0; i < 5; i++) vga_write_seq(i, saved_text_state.seq[i]);

    /* Unlock CRTC protected regs before restore. */
    vga_write_crtc(0x11, (uint8_t)(vga_read_crtc(0x11) & ~0x80));
    for (uint8_t i = 0; i < 25; i++) vga_write_crtc(i, saved_text_state.crtc[i]);

    for (uint8_t i = 0; i < 9; i++) vga_write_gc(i, saved_text_state.gc[i]);
    for (uint8_t i = 0; i < 21; i++) vga_write_ac(i, saved_text_state.ac[i]);

    /* Enable video output through AC index latch. */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x3C0));
}

/* Restore an 8x8 font into VGA plane 2 (used by text mode glyph fetch). */
static void __attribute__((unused)) vga_reload_font_plane2(void)
{
    uint8_t old_seq2 = vga_read_seq(0x02);
    uint8_t old_seq4 = vga_read_seq(0x04);
    uint8_t old_gc4  = vga_read_gc(0x04);
    uint8_t old_gc5  = vga_read_gc(0x05);
    uint8_t old_gc6  = vga_read_gc(0x06);

    /* Select plane 2 and disable odd/even addressing for direct font access. */
    vga_write_seq(0x02, 0x04);
    vga_write_seq(0x04, 0x06);
    vga_write_gc(0x04, 0x02);
    vga_write_gc(0x05, 0x00);
    vga_write_gc(0x06, 0x04);

    volatile uint8_t *font_mem = (volatile uint8_t *)0xA0000;
    for (uint16_t ch = 0; ch < 256; ch++) {
        for (uint8_t row = 0; row < 32; row++) {
            uint8_t v = 0x00;
            if (ch < 128 && row < 8) {
                v = font8x8[ch][row];
            }
            font_mem[ch * 32 + row] = v;
        }
    }

    /* Restore previous register state. */
    vga_write_seq(0x02, old_seq2);
    vga_write_seq(0x04, old_seq4);
    vga_write_gc(0x04, old_gc4);
    vga_write_gc(0x05, old_gc5);
    vga_write_gc(0x06, old_gc6);
}

void display_disable(void)
{
    /* Minimal restore path: disable Bochs VBE and use existing VGA text state.
     * Over-programming legacy VGA registers caused invisible text on some setups.
     */
    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    vga_restore_text_state();
    vga_set_cursor_visible(0);
    probe_vga_text();
}

int display_enter_gfx(void)
{
    vga_snapshot_text_state();
    vga_set_cursor_visible(0);

    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x01), "Nd"((uint16_t)0x3C4));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x3C5));

    if (probe_bochs_vbe()) {
        if (bochs_set_mode(800, 600, 32)) {
            __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x02), "Nd"((uint16_t)0x3C4));
            __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x00), "Nd"((uint16_t)0x3C5));

            __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x05), "Nd"((uint16_t)0x3CE));
            __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x00), "Nd"((uint16_t)0x3CF));

            __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x06), "Nd"((uint16_t)0x3CE));
            __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x05), "Nd"((uint16_t)0x3CF));

            uint8_t temp;
            __asm__ volatile ("inb %1, %0" : "=a"(temp) : "Nd"((uint16_t)0x3DA));
            __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x10), "Nd"((uint16_t)0x3C0));
            __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x01), "Nd"((uint16_t)0x3C0));

            __asm__ volatile ("inb %1, %0" : "=a"(temp) : "Nd"((uint16_t)0x3DA));
            __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x3C0));

            volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
            for (int i = 0; i < 80 * 25; i++) vga[i] = ' ' | (0x00 << 8);

            __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x01), "Nd"((uint16_t)0x3C4));
            __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x00), "Nd"((uint16_t)0x3C5));
            return 1;
        }
    }

    if (probe_vga_mode13()) return 1;

    display_disable();
    return 0;
}

int display_init(void)
{
    printk("Display: Probing graphics backends...\n");
    current_mode.available = 0;

    printk("  Probing Bochs VBE...\n");
    if (probe_bochs_vbe()) {
        printk("  Setting 800x600x32...\n");
        if (bochs_set_mode(800, 600, 32)) return 1;
    }

    printk("  Probing VGA mode 0x13...\n");
    if (probe_vga_mode13()) return 1;

    printk("  Falling back to VGA text mode.\n");
    probe_vga_text();
    return 0;
}

int display_init_multiboot(void *mbd)
{
    printk("Display: Trying Multiboot VBE...\n");
    if (probe_multiboot_vbe((multiboot_info_t *)mbd)) return 1;
    printk("  Multiboot VBE not available.\n");
    return 0;
}

const display_mode_t *display_get_mode(void)
{
    return &current_mode;
}

void display_clear(uint32_t color)
{
    if (!current_mode.available) return;

    if (current_mode.type == DISP_VGA_TEXT) {
        for (uint32_t i = 0; i < current_mode.width * current_mode.height; i++)
            ((volatile uint16_t *)current_mode.addr)[i] = ' ' | (0x07 << 8);
        return;
    }

    if (current_mode.bpp == 8) {
        uint8_t c = (uint8_t)color;
        for (uint32_t off = 0; off < (uint32_t)current_mode.width * current_mode.height; off++)
            current_mode.addr[off] = c;
    } else {
        for (uint32_t y = 0; y < current_mode.height; y++) {
            for (uint32_t x = 0; x < current_mode.width; x++) {
                if (current_mode.bpp == 32)
                    ((uint32_t*)current_mode.addr)[y * (current_mode.pitch/4) + x] = color;
                else if (current_mode.bpp == 16)
                    ((uint16_t*)current_mode.addr)[y * (current_mode.pitch/2) + x] = (uint16_t)color;
            }
        }
    }
}

void display_putpixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (!current_mode.available) return;
    if (x >= current_mode.width || y >= current_mode.height) return;

    if (current_mode.bpp == 8) {
        current_mode.addr[y * current_mode.pitch + x] = rgb_to_vga8(color);
    } else if (current_mode.bpp == 32) {
        *(volatile uint32_t *)(current_mode.addr + y * current_mode.pitch + x * 4) = color;
    } else if (current_mode.bpp == 16) {
        uint16_t r = (color >> 16) & 0xFF;
        uint16_t g = (color >> 8) & 0xFF;
        uint16_t b = color & 0xFF;
        uint16_t color16 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        *(volatile uint16_t *)(current_mode.addr + y * current_mode.pitch + x * 2) = color16;
    }
}

void display_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    for (uint32_t row = 0; row < h; row++)
        for (uint32_t col = 0; col < w; col++)
            display_putpixel(x + col, y + row, color);
}

void display_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    for (uint32_t col = 0; col < w; col++) {
        display_putpixel(x + col, y, color);
        display_putpixel(x + col, y + h - 1, color);
    }
    for (uint32_t row = 0; row < h; row++) {
        display_putpixel(x, y + row, color);
        display_putpixel(x + w - 1, y + row, color);
    }
}

/* ================================================================
 * Text rendering — 8x8 font scaled 2x (16x16 px per glyph)
 * Gives readable text at 800x600: ~50 cols x 37 rows
 * ================================================================ */
#define CHAR_SCALE 2
#define CHAR_W (FONT_W * CHAR_SCALE)   /* 16 px wide */
#define CHAR_H (FONT_H * CHAR_SCALE)   /* 16 px tall */





static const uint8_t *select_glyph(unsigned char c)
{
    /* Normalize lower-case for cleaner rendering. */
    if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');

    /* Use standard font for all characters */
    return font8x8[c];
}

void display_putchar(char c, uint32_t x, uint32_t y, uint32_t color)
{
    if (!current_mode.available) return;
    if ((unsigned char)c < 32 || (unsigned char)c > 126) c = ' ';
    const uint8_t *glyph = select_glyph((unsigned char)c);

    for (int row = 0; row < FONT_H; row++) {
        uint8_t bitmap = glyph[row];
        for (int col = 0; col < FONT_W; col++) {
            if (bitmap & (1 << col)) {
                /* Transparent background: only draw lit glyph pixels. */
                for (int sy = 0; sy < CHAR_SCALE; sy++) {
                    for (int sx = 0; sx < CHAR_SCALE; sx++) {
                        display_putpixel(x + col * CHAR_SCALE + sx,
                                         y + row * CHAR_SCALE + sy, color);
                    }
                }
            }
        }
    }
}

void display_puts(const char *str, uint32_t x, uint32_t y, uint32_t color)
{
    uint32_t cx = x;
    uint32_t cy = y;

    while (*str) {
        if (*str == '\n') {
            cx = x;
            cy += CHAR_H;
            if (cy + CHAR_H > current_mode.height) return;
        } else {
            display_putchar(*str, cx, cy, color);
            cx += CHAR_W;
            if (cx + CHAR_W > current_mode.width) {
                cx = x;
                cy += CHAR_H;
                if (cy + CHAR_H > current_mode.height) return;
            }
        }
        str++;
    }
}
