/* openkernel - Bochs VBE Display Initialization */
/* Works with QEMU -kernel mode without GRUB */

#include "vbe.h"

#define VBE_INDEX_PORT 0x01CE
#define VBE_DATA_PORT  0x01CF

#define VBE_INDEX_ID      0
#define VBE_INDEX_XRES    1
#define VBE_INDEX_YRES    2
#define VBE_INDEX_BPP     3
#define VBE_INDEX_ENABLE  4
#define VBE_INDEX_BANK    5
#define VBE_INDEX_VIRT_WIDTH  6
#define VBE_INDEX_VIRT_HEIGHT 7
#define VBE_INDEX_X_OFFSET 8
#define VBE_INDEX_Y_OFFSET 9

#define VBE_DISPI_DISABLED     0x00
#define VBE_DISPI_ENABLED      0x01
#define VBE_DISPI_LFB_ENABLED  0x40
#define VBE_DISPI_ID4          0xB0C4

static vbe_mode_t current_mode;

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

static inline void outl(uint16_t port, uint32_t val)
{
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
    uint32_t addr = 0x80000000 | (bus << 16) | ((slot & 0x1F) << 11) | ((func & 7) << 8) | (off & 0xFC);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

int vbe_init(uint16_t width, uint16_t height, uint16_t bpp)
{
    /* Check VBE presence */
    vbe_write(VBE_INDEX_ID, VBE_DISPI_ID4);
    if (vbe_read(VBE_INDEX_ID) != VBE_DISPI_ID4) {
        vbe_write(VBE_INDEX_ID, 0xB0C0);
        if (vbe_read(VBE_INDEX_ID) != 0xB0C0) return 0;
    }

    /* Disable, set mode, re-enable */
    vbe_write(VBE_INDEX_ENABLE, VBE_DISPI_DISABLED);
    vbe_write(VBE_INDEX_XRES, width);
    vbe_write(VBE_INDEX_YRES, height);
    vbe_write(VBE_INDEX_BPP,  bpp);
    vbe_write(VBE_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    /* Verify mode */
    if (vbe_read(VBE_INDEX_XRES) != width ||
        vbe_read(VBE_INDEX_YRES) != height ||
        vbe_read(VBE_INDEX_BPP)  != bpp)
        return 0;

    /* Find framebuffer address via PCI */
    uint32_t fb_addr = 0;
    for (uint8_t slot = 0; slot < 32; slot++) {
        uint32_t id = pci_read(0, slot, 0, 0);
        if ((id & 0xFFFF) == 0xFFFF) continue;
        uint32_t cls = pci_read(0, slot, 0, 8);
        if (((cls >> 24) & 0xFF) == 0x03 && ((cls >> 16) & 0xFF) == 0x00) {
            uint32_t bar0 = pci_read(0, slot, 0, 0x10);
            if (!(bar0 & 1)) {
                fb_addr = bar0 & ~0xF;
                break;
            }
        }
    }

    /* Fallback: direct test of known addresses */
    if (fb_addr == 0) {
        static const uint32_t addrs[4] = {0xFC000000, 0xFD000000, 0xE0000000, 0xF0000000};
        for (int i = 0; i < 4; i++) {
            volatile uint32_t *p = (volatile uint32_t *)(uint32_t)addrs[i];
            *p = 0xDEADBEEF;  /* Write unique pattern */
            __asm__ volatile ("mfence" ::: "memory");
            if (*p == 0xDEADBEEF) {
                fb_addr = addrs[i];
                break;
            }
        }
    }

    /* Last resort fallback */
    if (fb_addr == 0) fb_addr = 0xFD000000;

    current_mode.width       = width;
    current_mode.height      = height;
    current_mode.bpp         = bpp;
    current_mode.pitch       = width * (bpp / 8);
    current_mode.framebuffer = (uint8_t *)(uint32_t)fb_addr;

    return 1;
}

const vbe_mode_t *vbe_get_mode(void) { return &current_mode; }