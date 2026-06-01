/* openkernel - RAM disk driver header */
#ifndef RAMDISK_H
#define RAMDISK_H

#include <stdint.h>

/* Initialise a RAM disk of the given size (in sectors).
   Memory is allocated from the kernel heap.
   Registers the device as "ram0".
   Returns 0 on success, -1 on failure. */
int ramdisk_init(uint32_t num_sectors);

#endif /* RAMDISK_H */
