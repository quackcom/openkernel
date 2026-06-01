/* openkernel - Block device abstraction layer */
#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <stddef.h>

#define BLOCK_SECTOR_SIZE  512
#define BLOCK_MAX_DEVICES  4

/* Block device operations */
typedef struct block_device block_device_t;

struct block_device {
    char     name[16];
    uint32_t num_sectors;
    int      (*read)(block_device_t *dev, uint32_t sector, uint8_t *buffer);
    int      (*write)(block_device_t *dev, uint32_t sector, const uint8_t *buffer);
    void     *priv;   /* Driver-specific data */
};

/* Register a block device */
int  block_register(block_device_t *dev);

/* Look up a device by name */
block_device_t *block_find(const char *name);

/* Raw read/write helpers (return 0 on success, -1 on error) */
static inline int block_read(block_device_t *dev, uint32_t sector, uint8_t *buffer) {
    if (!dev || !dev->read) return -1;
    return dev->read(dev, sector, buffer);
}

static inline int block_write(block_device_t *dev, uint32_t sector, const uint8_t *buffer) {
    if (!dev || !dev->write) return -1;
    return dev->write(dev, sector, buffer);
}

/* Multi-sector helpers */
int block_read_multi(block_device_t *dev, uint32_t sector, uint8_t *buffer, uint32_t count);
int block_write_multi(block_device_t *dev, uint32_t sector, const uint8_t *buffer, uint32_t count);

/* Initialise the block device registry */
void block_init(void);

#endif /* BLOCK_H */
