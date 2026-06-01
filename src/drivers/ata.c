/* openkernel - ATA PIO driver (primary master)
 * Implements the block device interface.
 * I/O ports: 0x1F0-0x1F7 (command), 0x3F6 (control)
 * IRQ: 14
 *
 * Uses polling (no IRQ) for simplicity.
 */

#include "ata.h"
#include "../kernel/block.h"
#include "../kernel/kernel.h"
#include <stdint.h>
#include <stddef.h>

/* I/O ports for primary ATA channel */
#define ATA_DATA_PORT       0x1F0   /* 16-bit data */
#define ATA_ERROR_PORT      0x1F1   /* read error */
#define ATA_FEATURES_PORT   0x1F1   /* write features */
#define ATA_SECTOR_COUNT    0x1F2
#define ATA_LBA_LO          0x1F3
#define ATA_LBA_MID         0x1F4
#define ATA_LBA_HI          0x1F5
#define ATA_DRIVE_SELECT    0x1F6
#define ATA_COMMAND_PORT    0x1F7   /* write command */
#define ATA_STATUS_PORT     0x1F7   /* read status */
#define ATA_CONTROL_PORT    0x3F6

/* Status register bits */
#define ATA_SR_BSY  0x80   /* Busy */
#define ATA_SR_DRDY 0x40   /* Drive ready */
#define ATA_SR_DRQ  0x08   /* Data request */
#define ATA_SR_ERR  0x01   /* Error */

/* Commands */
#define ATA_CMD_READ_PIO     0x20
#define ATA_CMD_WRITE_PIO    0x30
#define ATA_CMD_IDENTIFY     0xEC

/* Master drive: 0xA0 */
#define ATA_DRIVE_MASTER 0xA0
#define ATA_DRIVE_SLAVE  0xB0

/* Timeouts (busy-loop iterations) */
#define ATA_TIMEOUT_BUSY  10000000
#define ATA_TIMEOUT_DRQ   10000000

/* External I/O helpers from keyboard.asm */
extern uint8_t  io_inb(uint16_t port);
extern void     io_outb(uint16_t port, uint8_t data);

/* Read one 16-bit word from the data port */
static inline uint16_t ata_read_word(void)
{
    return (uint16_t)(io_inb(ATA_DATA_PORT) | (io_inb(ATA_DATA_PORT + 1) << 8));
}

/* Write one 16-bit word to the data port */
static inline void ata_write_word(uint16_t val)
{
    io_outb(ATA_DATA_PORT,     (uint8_t)(val & 0xFF));
    io_outb(ATA_DATA_PORT + 1, (uint8_t)(val >> 8));
}

/* Wait for BSY to clear, then optionally wait for DRQ.
   Returns 0 on success, -1 on timeout or error. */
static int ata_wait(int wait_for_drq)
{
    uint32_t timeout = ATA_TIMEOUT_BUSY;
    uint8_t  status;

    /* Wait for BSY to clear */
    do {
        status = io_inb(ATA_STATUS_PORT);
        if (timeout-- == 0) return -1;
    } while (status & ATA_SR_BSY);

    if (status & ATA_SR_ERR) return -1;

    if (wait_for_drq) {
        timeout = ATA_TIMEOUT_DRQ;
        do {
            status = io_inb(ATA_STATUS_PORT);
            if (timeout-- == 0) return -1;
        } while (!(status & ATA_SR_DRQ));
        if (status & ATA_SR_ERR) return -1;
    }

    return 0;
}

/* ---- block device read/write callbacks ---- */

struct ata_priv {
    uint32_t num_sectors;
};

static int ata_read_sectors(block_device_t *dev, uint32_t sector, uint8_t *buffer)
{
    (void)dev;

    /* Select drive and LBA */
    io_outb(ATA_DRIVE_SELECT, ATA_DRIVE_MASTER | ((sector >> 24) & 0x0F));
    io_outb(ATA_CONTROL_PORT, 0x00);   /* No interrupts */
    io_outb(ATA_SECTOR_COUNT, 1);       /* Read 1 sector */
    io_outb(ATA_LBA_LO,       (uint8_t)(sector & 0xFF));
    io_outb(ATA_LBA_MID,      (uint8_t)((sector >> 8) & 0xFF));
    io_outb(ATA_LBA_HI,       (uint8_t)((sector >> 16) & 0xFF));

    /* Send READ SECTORS command */
    io_outb(ATA_COMMAND_PORT, ATA_CMD_READ_PIO);

    /* Wait for data ready */
    if (ata_wait(1) != 0) return -1;

    /* Read 256 words (512 bytes) */
    for (int i = 0; i < 256; i++) {
        uint16_t w = ata_read_word();
        buffer[i * 2]     = (uint8_t)(w & 0xFF);
        buffer[i * 2 + 1] = (uint8_t)(w >> 8);
    }

    return 0;
}

static int ata_write_sectors(block_device_t *dev, uint32_t sector, const uint8_t *buffer)
{
    (void)dev;

    /* Select drive and LBA */
    io_outb(ATA_DRIVE_SELECT, ATA_DRIVE_MASTER | ((sector >> 24) & 0x0F));
    io_outb(ATA_CONTROL_PORT, 0x00);
    io_outb(ATA_SECTOR_COUNT, 1);
    io_outb(ATA_LBA_LO,       (uint8_t)(sector & 0xFF));
    io_outb(ATA_LBA_MID,      (uint8_t)((sector >> 8) & 0xFF));
    io_outb(ATA_LBA_HI,       (uint8_t)((sector >> 16) & 0xFF));

    /* Send WRITE SECTORS command */
    io_outb(ATA_COMMAND_PORT, ATA_CMD_WRITE_PIO);

    /* Wait for DRQ */
    if (ata_wait(1) != 0) return -1;

    /* Write 256 words (512 bytes) */
    for (int i = 0; i < 256; i++) {
        uint16_t w = (uint16_t)buffer[i * 2] | ((uint16_t)buffer[i * 2 + 1] << 8);
        ata_write_word(w);
    }

    /* Flush cache (wait for BSY to clear) */
    if (ata_wait(0) != 0) return -1;

    return 0;
}

static block_device_t ata_device;

int ata_init(void)
{
    uint16_t buf[256];

    printk("  Probing ATA primary master...\n");

    /* Select master drive */
    io_outb(ATA_DRIVE_SELECT, ATA_DRIVE_MASTER);
    io_outb(ATA_CONTROL_PORT, 0x00);

    /* Small delay */
    for (volatile int i = 0; i < 4; i++) io_inb(ATA_STATUS_PORT);

    /* Send IDENTIFY command */
    io_outb(ATA_SECTOR_COUNT, 0);
    io_outb(ATA_LBA_LO, 0);
    io_outb(ATA_LBA_MID, 0);
    io_outb(ATA_LBA_HI, 0);
    io_outb(ATA_COMMAND_PORT, ATA_CMD_IDENTIFY);

    /* Check if device exists */
    uint8_t status = io_inb(ATA_STATUS_PORT);
    if (status == 0x00) {
        printk("  [--] No ATA device detected (status=0x00)\n");
        return -1;
    }

    /* Wait for BSY to clear */
    if (ata_wait(0) != 0) {
        printk("  [--] ATA device timeout during IDENTIFY\n");
        return -1;
    }

    /* Read identification data (256 words) */
    for (int i = 0; i < 256; i++)
        buf[i] = ata_read_word();

    /* Check if device is ATA (not ATAPI) */
    /* Word 0: bit 15 = 0 for ATA, 1 for ATAPI */
    if (buf[0] & 0x8000) {
        printk("  [--] ATAPI device detected, not supported\n");
        return -1;
    }

    /* Get number of sectors (words 60-61, LBA28) */
    uint32_t num_sectors = buf[60] | ((uint32_t)buf[61] << 16);

    if (num_sectors == 0) {
        printk("  [--] ATA device reports 0 sectors\n");
        return -1;
    }

    printk("  [OK] ATA primary master: %u sectors (%u MB)\n",
           num_sectors, num_sectors / 2048);

    /* Fill in the block device structure */
    int i = 0;
    const char *n = "ata0";
    while (*n && i < 15) { ata_device.name[i++] = *n++; }
    ata_device.name[i] = '\0';
    ata_device.num_sectors    = num_sectors;
    ata_device.read           = ata_read_sectors;
    ata_device.write          = ata_write_sectors;
    ata_device.priv           = NULL;

    block_register(&ata_device);
    return 0;
}
