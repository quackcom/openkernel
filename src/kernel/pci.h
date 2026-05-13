/* openkernel - Simple PCI Bus Utilities */
#ifndef PCI_H
#define PCI_H

#include <stdint.h>

/* Read a 32-bit value from PCI config space */
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

/* Write a 32-bit value to PCI config space */
void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);

/* Find a device with given vendor/class. Returns slot<<3 | func, or 0xFF if not found */
uint8_t pci_find_device(uint16_t vendor_id, uint16_t device_id);

/* Find VGA controller (class 0x03, subclass 0x00). Returns slot<<3 | func, or 0xFF */
uint8_t pci_find_vga(void);

#endif /* PCI_H */