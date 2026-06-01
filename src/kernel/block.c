/* openkernel - Block device abstraction layer implementation */
#include "block.h"
#include "kernel.h"
#include <stdint.h>
#include <stddef.h>

static block_device_t *devices[BLOCK_MAX_DEVICES];
static int num_devices = 0;

void block_init(void)
{
    num_devices = 0;
    for (int i = 0; i < BLOCK_MAX_DEVICES; i++)
        devices[i] = NULL;
}

int block_register(block_device_t *dev)
{
    if (!dev || num_devices >= BLOCK_MAX_DEVICES) return -1;
    devices[num_devices++] = dev;
    printk("  [OK] Block device '%s' registered (%u sectors)\n",
           dev->name, dev->num_sectors);
    return 0;
}

block_device_t *block_find(const char *name)
{
    for (int i = 0; i < num_devices; i++) {
        if (!devices[i]) continue;
        const char *a = name;
        const char *b = devices[i]->name;
        int match = 1;
        while (*a && *b) { if (*a++ != *b++) { match = 0; break; } }
        if (match && *a == '\0' && *b == '\0') return devices[i];
    }
    return NULL;
}

int block_read_multi(block_device_t *dev, uint32_t sector, uint8_t *buffer, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        if (block_read(dev, sector + i, buffer + i * BLOCK_SECTOR_SIZE) != 0)
            return -1;
    }
    return 0;
}

int block_write_multi(block_device_t *dev, uint32_t sector, const uint8_t *buffer, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        if (block_write(dev, sector + i, buffer + i * BLOCK_SECTOR_SIZE) != 0)
            return -1;
    }
    return 0;
}
