/* openkernel - Simple PCI Bus Implementation */
#include "pci.h"

/* PCI config I/O ports */
#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

/* Read a 32-bit value from PCI config space */
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    uint32_t result;

    asm volatile (
        "movl %1, %%edx\n"
        "movl %2, %%eax\n"
        "outl %%eax, %%edx\n"
        "movl %3, %%edx\n"
        "inl %%edx, %%eax\n"
        "movl %%eax, %0\n"
        : "=r"(result)
        : "i"(PCI_CONFIG_ADDR), "r"(address), "i"(PCI_CONFIG_DATA)
        : "%eax", "%edx", "memory"
    );

    return result;
}

/* Write a 32-bit value to PCI config space */
void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value)
{
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);

    asm volatile (
        "movl %1, %%edx\n"
        "movl %2, %%eax\n"
        "outl %%eax, %%edx\n"
        "movl %3, %%edx\n"
        "movl %4, %%eax\n"
        "outl %%eax, %%edx\n"
        :
        : "i"(PCI_CONFIG_ADDR), "r"(address), "i"(PCI_CONFIG_DATA), "r"(value)
        : "%eax", "%edx", "memory"
    );
}

/* Find a device with given vendor/device ID */
uint8_t pci_find_device(uint16_t vendor_id, uint16_t device_id)
{
    for (uint8_t bus = 0; bus < 1; bus++) {           /* Only bus 0 */
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = pci_config_read(bus, slot, func, 0);
                uint16_t vendor = id & 0xFFFF;
                uint16_t device = (id >> 16) & 0xFFFF;

                if (vendor == 0xFFFF) break;  /* No device, skip remaining funcs */
                if (vendor == vendor_id && device == device_id) {
                    return (slot << 3) | func;
                }

                /* If header type indicates single function, skip other funcs */
                if (func == 0) {
                    uint32_t header = pci_config_read(bus, slot, func, 0x0C);
                    if (!(header & 0x800000)) break;
                }
            }
        }
    }
    return 0xFF;  /* Not found */
}

/* Find VGA controller */
uint8_t pci_find_vga(void)
{
    for (uint8_t bus = 0; bus < 1; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t id = pci_config_read(bus, slot, 0, 0);
            uint16_t vendor = id & 0xFFFF;
            if (vendor == 0xFFFF) continue;

            /* Read class/subclass from PCI config offset 0x08 */
            uint32_t class_reg = pci_config_read(bus, slot, 0, 0x08);
            uint8_t class_code = (class_reg >> 24) & 0xFF;
            uint8_t subclass  = (class_reg >> 16) & 0xFF;

            /* VGA controller: class 0x03, subclass 0x00 */
            if (class_code == 0x03 && subclass == 0x00) {
                return (slot << 3) | 0;  /* Return slot, func=0 */
            }
        }
    }
    return 0xFF;
}