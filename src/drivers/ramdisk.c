/* openkernel - RAM disk driver
 * Allocates a chunk of heap memory and presents it as a block device.
 * Useful for testing the block layer and on-disk FS without real ATA hardware.
 */

#include "ramdisk.h"
#include "../kernel/block.h"
#include "../kernel/memory.h"
#include "../kernel/kernel.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data;
    uint32_t num_sectors;
} ramdisk_priv_t;

static int ramdisk_read(block_device_t *dev, uint32_t sector, uint8_t *buffer)
{
    ramdisk_priv_t *priv = (ramdisk_priv_t *)dev->priv;
    if (sector >= priv->num_sectors) return -1;

    uint32_t offset = sector * BLOCK_SECTOR_SIZE;
    for (uint32_t i = 0; i < BLOCK_SECTOR_SIZE; i++)
        buffer[i] = priv->data[offset + i];
    return 0;
}

static int ramdisk_write(block_device_t *dev, uint32_t sector, const uint8_t *buffer)
{
    ramdisk_priv_t *priv = (ramdisk_priv_t *)dev->priv;
    if (sector >= priv->num_sectors) return -1;

    uint32_t offset = sector * BLOCK_SECTOR_SIZE;
    for (uint32_t i = 0; i < BLOCK_SECTOR_SIZE; i++)
        priv->data[offset + i] = buffer[i];
    return 0;
}

static block_device_t ramdisk_device;
static ramdisk_priv_t  ramdisk_priv_data;

int ramdisk_init(uint32_t num_sectors)
{
    if (num_sectors == 0) return -1;

    uint32_t total_bytes = num_sectors * BLOCK_SECTOR_SIZE;

    /* Allocate from kernel heap */
    uint8_t *data = (uint8_t *)kmalloc(total_bytes);
    if (!data) {
        printk("  [--] RAM disk: kmalloc(%u) failed\n", total_bytes);
        return -1;
    }

    /* Zero-initialise the RAM disk */
    for (uint32_t i = 0; i < total_bytes; i++)
        data[i] = 0;

    ramdisk_priv_data.data        = data;
    ramdisk_priv_data.num_sectors = num_sectors;

    int i = 0;
    const char *n = "ram0";
    while (*n && i < 15) { ramdisk_device.name[i++] = *n++; }
    ramdisk_device.name[i]     = '\0';
    ramdisk_device.num_sectors = num_sectors;
    ramdisk_device.read        = ramdisk_read;
    ramdisk_device.write       = ramdisk_write;
    ramdisk_device.priv        = &ramdisk_priv_data;

    block_register(&ramdisk_device);
    return 0;
}
