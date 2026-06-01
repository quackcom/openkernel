/* openkernel - On-disk OKFS header */
#ifndef OKFS_DISK_H
#define OKFS_DISK_H

#include <stdint.h>
#include <stddef.h>
#include "block.h"

/* On-disk OKFS magic */
#define OKFS_MAGIC  0x4F4B4653   /* "OKFS" */

/* Maximum filename length (must match inode layout) */
#define OKFS_NAME_MAX  16

/* Maximum path length */
#define OKFS_PATH_MAX  256

/* Inode type */
#define OKFS_TYPE_DIR   0
#define OKFS_TYPE_FILE  1

/* Mounted device context */
typedef struct {
    block_device_t *dev;
    uint32_t total_blocks;
    uint32_t bitmap_blocks;
    uint32_t inode_count;
    uint32_t inode_blocks;
    uint32_t data_start;
    int      mounted;
} okfs_t;

/* Initialise an OKFS on the given block device (format it).
   Allocates inode_count inodes.
   Returns 0 on success, -1 on error. */
int  okfs_format(block_device_t *dev, uint32_t inode_count);

/* Mount an already-formatted OKFS filesystem.
   Fills the okfs_t context. */
int  okfs_mount(okfs_t *fs, block_device_t *dev);

/* Create a file at a path (mkdir / touch equivalent).
   type: OKFS_TYPE_DIR or OKFS_TYPE_FILE
   Returns inode number on success, -1 on error. */
int  okfs_create(okfs_t *fs, const char *path, int type);

/* Read the contents of a file into a heap-allocated buffer.
   *size_out is set to the file size.
   Returns a kmalloc'd buffer (caller must kfree), or NULL on error. */
uint8_t *okfs_read(okfs_t *fs, const char *path, uint32_t *size_out);

/* Write data to a file (creates if it doesn't exist, overwrites if it does). */
int  okfs_write(okfs_t *fs, const char *path, const uint8_t *data, uint32_t size);

/* Delete a file or empty directory. */
int  okfs_delete(okfs_t *fs, const char *path);

/* List directory contents; calls emit() for each entry.
   Entries are formatted as "[DIR]  name" or "[FILE] name (N bytes)". */
int  okfs_list(okfs_t *fs, const char *path, void (*emit)(const char *));

/* Print file contents to emit(). */
int  okfs_cat(okfs_t *fs, const char *path, void (*emit)(const char *));

/* Error string helper */
const char *okfs_strerror(int err);

/* Error codes */
#define OKFS_OK            0
#define OKFS_ERR_NOT_FOUND (-1)
#define OKFS_ERR_EXISTS    (-2)
#define OKFS_ERR_NOT_DIR   (-3)
#define OKFS_ERR_NOT_FILE  (-4)
#define OKFS_ERR_NO_SPACE  (-5)
#define OKFS_ERR_IO        (-6)
#define OKFS_ERR_NOT_MOUNTED (-7)
#define OKFS_ERR_BAD_MAGIC (-8)
#define OKFS_ERR_FULL      (-9)
#define OKFS_ERR_NOT_EMPTY (-10)

#endif /* OKFS_DISK_H */
