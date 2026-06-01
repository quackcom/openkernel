/* openkernel - ATA PIO driver header */
#ifndef ATA_H
#define ATA_H

#include <stdint.h>

/* Initialise primary master ATA device.
   Returns 0 on success, -1 if no drive detected. */
int ata_init(void);

#endif /* ATA_H */
