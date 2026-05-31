# Component: PCI

**Files:** `src/kernel/pci.c`, `src/kernel/pci.h`

## Purpose

Enumerate devices on the **PCI bus** by reading configuration space — foundation for future storage and network drivers.

## Configuration space access

Classic x86 mechanism:

- Address port: `0xCF8`  
- Data port: `0xCFC`  

Build config dword: enable bit, bus, device, function, register offset.

## API

| Function | Description |
|----------|-------------|
| `pci_init()` | Scan buses 0–255, devices 0–31, functions 0–7 |
| `pci_read_config(bus, dev, fn, offset)` | Read 32-bit config dword |
| `pci_get_device(class, subclass, vendor)` | Find matching device |

## Scan behavior

- Skips invalid vendor `0xFFFF`  
- Logs vendor:device, class codes via `printk` during init (when called from boot path)  
- Stores limited device table in static array  

## IRQ stubs

`isr.asm` defines IRQ 9–15 and PCI-related lines — **no handler registered** yet for ATA (IRQ 14/15).

## Future use

| Device class | Planned driver |
|--------------|----------------|
| Mass storage | ATA/AHCI for disk-backed OKFS |
| Network | NIC + stack (roadmap phase 8) |
| Display | Already use VBE/VGA paths |

## Related

- [[Roadmap]]  
- [[Component-CPU-GDT-IDT-ISR]]  
- [[Filesystem-OKFS]] — persistence needs block device  
