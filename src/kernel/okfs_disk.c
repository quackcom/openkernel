/* openkernel - OKFS on-disk filesystem implementation.
 *
 * On-disk layout (block size = 512 bytes):
 *   Block 0:         Superblock
 *   Blocks 1..N:     Block bitmap (1 bit per data block)
 *   Blocks N+1..M:   Inode table   (64 inodes, 32 bytes each = 4 sectors)
 *   Blocks M+1..end: Data blocks
 *
 * Each inode is 32 bytes:
 *   name[16]  - null-terminated filename
 *   size      - uint32_t, file size in bytes
 *   start_block - uint32_t, first data block number (contiguous)
 *   type      - uint8_t, 0=dir, 1=file
 *   7 bytes padding
 *
 * Directory content: flat list of 32-byte entries:
 *   inode  - uint32_t (0 = unused)
 *   name[28] - null-terminated entry name
 */

#include "okfs_disk.h"
#include "memory.h"
#include "kernel.h"
#include <stdint.h>
#include <stddef.h>

/* ---- Constants ---- */
#define OKFS_BLOCK_SIZE     BLOCK_SECTOR_SIZE   /* 512 */
#define INODE_SIZE          32
#define DIRENT_SIZE         32
#define INODES_PER_BLOCK    (OKFS_BLOCK_SIZE / INODE_SIZE)  /* 16 */
#define DIRENTS_PER_BLOCK   (OKFS_BLOCK_SIZE / DIRENT_SIZE) /* 16 */

/* Superblock field offsets */
#define SB_MAGIC_OFF        0
#define SB_VERSION_OFF      4
#define SB_TOTAL_BLOCKS_OFF 8
#define SB_BITMAP_BLOCKS_OFF 12
#define SB_INODE_COUNT_OFF  16
#define SB_INODE_BLOCKS_OFF 20
#define SB_DATA_START_OFF   24

/* Inode field offsets */
#define INODE_NAME_OFF      0   /* 16 bytes */
#define INODE_SIZE_OFF      16  /* 4 bytes */
#define INODE_START_OFF     20  /* 4 bytes */
#define INODE_TYPE_OFF      24  /* 1 byte */

/* ---- Helpers ---- */

static inline uint32_t read32(const uint8_t *buf, uint32_t off)
{
    return (uint32_t)buf[off] | ((uint32_t)buf[off+1] << 8)
         | ((uint32_t)buf[off+2] << 16) | ((uint32_t)buf[off+3] << 24);
}

static inline void write32(uint8_t *buf, uint32_t off, uint32_t val)
{
    buf[off]   = (uint8_t)(val & 0xFF);
    buf[off+1] = (uint8_t)((val >> 8) & 0xFF);
    buf[off+2] = (uint8_t)((val >> 16) & 0xFF);
    buf[off+3] = (uint8_t)((val >> 24) & 0xFF);
}

static int okfs_name_cmp(const char *a, const char *b)
{
    while (*a && *b) { if (*a != *b) return *a - *b; a++; b++; }
    if (*a == '\0' && *b == '\0') return 0;
    return *a - *b;
}

static uint32_t okfs_strlen(const char *s)
{
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

static void okfs_strcpy(char *dst, const char *src, uint32_t max)
{
    uint32_t i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void skip_spaces(const char **p)
{
    while (**p == ' ') (*p)++;
}

/* ---- Superblock I/O ---- */

static int sb_read(block_device_t *dev, uint8_t *buf)
{
    return block_read(dev, 0, buf);
}

static int sb_write(block_device_t *dev, const uint8_t *buf)
{
    return block_write(dev, 0, buf);
}

/* ---- Bitmap operations ---- */

/* Get the block number of the bitmap sector containing the given data block. */
static uint32_t bitmap_sector(okfs_t *fs, uint32_t data_block)
{
    /* Bitmap starts at block 1 */
    uint32_t bit_index = data_block;
    uint32_t bits_per_sector = OKFS_BLOCK_SIZE * 8;
    return 1 + (bit_index / bits_per_sector);
}

/* Get the bit index within that bitmap sector. */
static uint32_t bitmap_bit(okfs_t *fs, uint32_t data_block)
{
    (void)fs;
    uint32_t bits_per_sector = OKFS_BLOCK_SIZE * 8;
    return data_block % bits_per_sector;
}

static int bitmap_test(okfs_t *fs, uint32_t data_block)
{
    uint8_t buf[OKFS_BLOCK_SIZE];
    if (block_read(fs->dev, bitmap_sector(fs, data_block), buf) != 0) return -1;
    uint32_t bit = bitmap_bit(fs, data_block);
    return (buf[bit / 8] >> (bit % 8)) & 1;
}

static int bitmap_set(okfs_t *fs, uint32_t data_block, int value)
{
    uint8_t buf[OKFS_BLOCK_SIZE];
    uint32_t sec = bitmap_sector(fs, data_block);
    if (block_read(fs->dev, sec, buf) != 0) return -1;
    uint32_t bit = bitmap_bit(fs, data_block);
    if (value)
        buf[bit / 8] |= (1 << (bit % 8));
    else
        buf[bit / 8] &= ~(1 << (bit % 8));
    return block_write(fs->dev, sec, buf);
}

/* Find a contiguous run of count blocks.
   Returns the starting block number, or 0xFFFFFFFF on failure. */
static uint32_t bitmap_find(okfs_t *fs, uint32_t count)
{
    uint32_t start = 0;
    uint32_t run = 0;
    for (uint32_t b = 0; b < fs->total_blocks; b++) {
        int used = bitmap_test(fs, b);
        if (used == -1) return 0xFFFFFFFF;
        if (!used) {
            if (run == 0) start = b;
            run++;
            if (run >= count) return start;
        } else {
            run = 0;
        }
    }
    return 0xFFFFFFFF;  /* Not enough contiguous space */
}

/* ---- Inode I/O ---- */

static uint32_t inode_sector(okfs_t *fs, uint32_t inum)
{
    return 1 + fs->bitmap_blocks + (inum / INODES_PER_BLOCK);
}

static uint32_t inode_offset(uint32_t inum)
{
    return (inum % INODES_PER_BLOCK) * INODE_SIZE;
}

static int inode_read(okfs_t *fs, uint32_t inum, uint8_t *buf)
{
    uint8_t sector[OKFS_BLOCK_SIZE];
    if (block_read(fs->dev, inode_sector(fs, inum), sector) != 0) return -1;
    uint32_t off = inode_offset(inum);
    for (uint32_t i = 0; i < INODE_SIZE; i++) buf[i] = sector[off + i];
    return 0;
}

static int inode_write(okfs_t *fs, uint32_t inum, const uint8_t *buf)
{
    uint8_t sector[OKFS_BLOCK_SIZE];
    if (block_read(fs->dev, inode_sector(fs, inum), sector) != 0) return -1;
    uint32_t off = inode_offset(inum);
    for (uint32_t i = 0; i < INODE_SIZE; i++) sector[off + i] = buf[i];
    return block_write(fs->dev, inode_sector(fs, inum), sector);
}

/* Encode an inode into a raw buffer */
static void inode_encode(uint8_t *buf, const char *name, uint32_t size,
                          uint32_t start_block, uint8_t type)
{
    for (uint32_t i = 0; i < INODE_SIZE; i++) buf[i] = 0;
    uint32_t i = 0;
    while (name[i] && i < OKFS_NAME_MAX - 1) {
        buf[INODE_NAME_OFF + i] = (uint8_t)name[i];
        i++;
    }
    write32(buf, INODE_SIZE_OFF, size);
    write32(buf, INODE_START_OFF, start_block);
    buf[INODE_TYPE_OFF] = type;
}

/* Decode fields from a raw inode buffer */
static void inode_decode(const uint8_t *buf, char *name, uint32_t *size,
                          uint32_t *start_block, uint8_t *type)
{
    uint32_t i;
    for (i = 0; i < OKFS_NAME_MAX - 1; i++) {
        name[i] = (char)buf[INODE_NAME_OFF + i];
        if (name[i] == '\0') break;
    }
    name[i] = '\0';
    *size        = read32(buf, INODE_SIZE_OFF);
    *start_block = read32(buf, INODE_START_OFF);
    *type        = buf[INODE_TYPE_OFF];
}

/* Find a free inode slot. Returns inode number or 0xFFFFFFFF. */
static uint32_t inode_alloc(okfs_t *fs)
{
    uint8_t buf[INODE_SIZE];
    for (uint32_t i = 1; i < fs->inode_count; i++) {  /* skip 0 (root) */
        if (inode_read(fs, i, buf) != 0) return 0xFFFFFFFF;
        if (buf[INODE_NAME_OFF] == '\0') return i;  /* unused */
    }
    return 0xFFFFFFFF;
}

/* ---- Directory entry operations ---- */

/* Find a directory entry within a directory's data blocks. */
static int dirent_find(okfs_t *fs, uint32_t dir_inum, const char *name,
                        uint32_t *out_inum)
{
    uint8_t inode_buf[INODE_SIZE];
    if (inode_read(fs, dir_inum, inode_buf) != 0) return -1;
    uint32_t dir_size = read32(inode_buf, INODE_SIZE_OFF);
    uint32_t start    = read32(inode_buf, INODE_START_OFF);

    if (dir_size == 0) return -1;  /* empty dir */

    uint32_t total_entries = dir_size / DIRENT_SIZE;
    uint8_t block_buf[OKFS_BLOCK_SIZE];

    for (uint32_t e = 0; e < total_entries; e++) {
        uint32_t blk = start + (e * DIRENT_SIZE) / OKFS_BLOCK_SIZE;
        uint32_t off = (e * DIRENT_SIZE) % OKFS_BLOCK_SIZE;
        if (block_read(fs->dev, blk, block_buf) != 0) return -1;
        uint32_t inum = read32(block_buf, off);
        if (inum == 0) continue;

        /* Extract entry name (at off+4, up to 28 chars) */
        char ename[28];
        uint32_t ni;
        for (ni = 0; ni < 27; ni++) {
            ename[ni] = (char)block_buf[off + 4 + ni];
            if (ename[ni] == '\0') break;
        }
        ename[ni] = '\0';

        if (okfs_name_cmp(name, ename) == 0) {
            *out_inum = inum;
            return 0;
        }
    }
    return -1;
}

/* Add a directory entry to a directory's data.
   Directory data blocks are always contiguous. */
static int dirent_add(okfs_t *fs, uint32_t dir_inum, const char *name,
                       uint32_t child_inum)
{
    uint8_t inode_buf[INODE_SIZE];
    if (inode_read(fs, dir_inum, inode_buf) != 0) return -1;
    uint32_t dir_size = read32(inode_buf, INODE_SIZE_OFF);
    uint32_t start    = read32(inode_buf, INODE_START_OFF);

    uint32_t new_size = dir_size + DIRENT_SIZE;
    uint32_t needed_blocks = (new_size + OKFS_BLOCK_SIZE - 1) / OKFS_BLOCK_SIZE;
    uint32_t current_blocks = (dir_size + OKFS_BLOCK_SIZE - 1) / OKFS_BLOCK_SIZE;

    if (needed_blocks > current_blocks) {
        if (start == 0) {
            /* First allocation: find 2 contiguous blocks so we have room */
            start = bitmap_find(fs, 2);
            if (start == 0xFFFFFFFF) return -1;
            bitmap_set(fs, start, 1);
            bitmap_set(fs, start + 1, 1);
            /* Zero them out */
            uint8_t zero[OKFS_BLOCK_SIZE];
            for (uint32_t i = 0; i < OKFS_BLOCK_SIZE; i++) zero[i] = 0;
            block_write(fs->dev, fs->data_start + start, zero);
            block_write(fs->dev, fs->data_start + start + 1, zero);
        } else {
            /* Extend: verify the next contiguous block is free */
            uint32_t next = start + current_blocks;
            int used = bitmap_test(fs, next);
            if (used) return -1;  /* Can't extend — no contiguous space */
            bitmap_set(fs, next, 1);
            uint8_t zero[OKFS_BLOCK_SIZE];
            for (uint32_t i = 0; i < OKFS_BLOCK_SIZE; i++) zero[i] = 0;
            block_write(fs->dev, fs->data_start + next, zero);
        }
    }

    /* Write the new entry at the end of directory data */
    uint32_t entry_byte_off = dir_size;
    uint32_t blk = start + entry_byte_off / OKFS_BLOCK_SIZE;
    uint32_t off = entry_byte_off % OKFS_BLOCK_SIZE;

    uint8_t block_buf[OKFS_BLOCK_SIZE];
    if (block_read(fs->dev, fs->data_start + blk, block_buf) != 0) return -1;

    write32(block_buf, off, child_inum);
    uint32_t ni = 0;
    while (name[ni] && ni < 27) { block_buf[off + 4 + ni] = (uint8_t)name[ni]; ni++; }
    block_buf[off + 4 + ni] = '\0';

    if (block_write(fs->dev, fs->data_start + blk, block_buf) != 0) return -1;

    /* Update directory size in inode */
    write32(inode_buf, INODE_SIZE_OFF, new_size);
    write32(inode_buf, INODE_START_OFF, start);
    return inode_write(fs, dir_inum, inode_buf);
}

/* ---- Path resolution ---- */

/* Split a path into parent directory name and leaf name.
 * e.g. "/docs/about.txt" → parent="/docs", leaf="about.txt"
 * e.g. "readme.txt" → parent=NULL, leaf="readme.txt"
 * Returns 0 on success, -1 on error. */
static int split_path(const char *path, char *parent_buf, uint32_t parent_max,
                       char *leaf_buf, uint32_t leaf_max)
{
    if (!path || !path[0]) return -1;

    /* Find the last slash */
    int last_slash = -1;
    int i;
    for (i = 0; path[i]; i++) {
        if (path[i] == '/') last_slash = i;
    }

    if (last_slash < 0) {
        /* No slash - leaf is the whole path */
        okfs_strcpy(leaf_buf, path, leaf_max);
        parent_buf[0] = '\0';
    } else {
        /* Has slash - copy everything before last slash as parent */
        if (last_slash == 0) {
            parent_buf[0] = '/';
            parent_buf[1] = '\0';
        } else {
            for (int j = 0; j < last_slash && j < (int)parent_max - 1; j++)
                parent_buf[j] = path[j];
            parent_buf[last_slash] = '\0';
        }
        okfs_strcpy(leaf_buf, path + last_slash + 1, leaf_max);
    }

    return 0;
}

/* Resolve a path to an inode number, starting from root (inode 0).
 * For now we only support absolute paths from root.
 * Returns inode number, or 0xFFFFFFFF on error. */
static uint32_t resolve_path(okfs_t *fs, const char *path)
{
    if (!path || !path[0]) return 0xFFFFFFFF;

    /* Skip leading slash */
    if (*path == '/') path++;
    if (!*path) return 0;  /* Root */

    uint32_t current = 0;  /* Start at root */
    char component[OKFS_NAME_MAX];
    uint32_t ci = 0;
    uint8_t inode_buf[INODE_SIZE];

    while (1) {
        /* Extract next path component */
        ci = 0;
        while (*path && *path != '/' && ci < OKFS_NAME_MAX - 1) {
            component[ci++] = *path++;
        }
        component[ci] = '\0';
        if (ci == 0) break;

        /* Look up component in current directory */
        uint32_t child_inum;
        if (dirent_find(fs, current, component, &child_inum) != 0)
            return 0xFFFFFFFF;

        current = child_inum;

        if (*path == '/') path++;
        else break;  /* End of path */
    }

    return current;
}

/* ---- Public API ---- */

int okfs_format(block_device_t *dev, uint32_t inode_count)
{
    if (!dev || inode_count == 0) return -1;

    /* Clamp inode count to fit in blocks */
    uint32_t inode_blocks = (inode_count * INODE_SIZE + OKFS_BLOCK_SIZE - 1) / OKFS_BLOCK_SIZE;
    inode_count = inode_blocks * INODES_PER_BLOCK;

    uint32_t total_blocks = dev->num_sectors;
    uint32_t bitmap_blocks = (total_blocks + OKFS_BLOCK_SIZE * 8 - 1)
                              / (OKFS_BLOCK_SIZE * 8);
    uint32_t data_start = 1 + bitmap_blocks + inode_blocks;

    if (data_start >= total_blocks) return -1;

    /* Write superblock */
    uint8_t sb[OKFS_BLOCK_SIZE];
    for (uint32_t i = 0; i < OKFS_BLOCK_SIZE; i++) sb[i] = 0;
    write32(sb, SB_MAGIC_OFF,        OKFS_MAGIC);
    write32(sb, SB_VERSION_OFF,      1);
    write32(sb, SB_TOTAL_BLOCKS_OFF, total_blocks);
    write32(sb, SB_BITMAP_BLOCKS_OFF, bitmap_blocks);
    write32(sb, SB_INODE_COUNT_OFF,  inode_count);
    write32(sb, SB_INODE_BLOCKS_OFF, inode_blocks);
    write32(sb, SB_DATA_START_OFF,   data_start);

    if (block_write(dev, 0, sb) != 0) return -1;

    /* Clear all bitmap and inode sectors */
    uint8_t zero[OKFS_BLOCK_SIZE];
    for (uint32_t i = 0; i < OKFS_BLOCK_SIZE; i++) zero[i] = 0;
    for (uint32_t b = 1; b < data_start; b++) {
        if (block_write(dev, b, zero) != 0) return -1;
    }

    /* Build a temporary okfs_t so we can use bitmap_set/inode_write */
    okfs_t tmp_fs;
    tmp_fs.dev            = dev;
    tmp_fs.total_blocks   = total_blocks;
    tmp_fs.bitmap_blocks  = bitmap_blocks;
    tmp_fs.inode_count    = inode_count;
    tmp_fs.inode_blocks   = inode_blocks;
    tmp_fs.data_start     = data_start;
    tmp_fs.mounted        = 1;

    /* Mark metadata blocks (0 .. data_start-1) as used in the bitmap */
    for (uint32_t b = 0; b < data_start; b++) {
        bitmap_set(&tmp_fs, b, 1);
    }

    /* Create root directory inode with 2 pre-allocated blocks */
    uint32_t root_start = bitmap_find(&tmp_fs, 2);
    if (root_start == 0xFFFFFFFF) return -1;
    bitmap_set(&tmp_fs, root_start, 1);
    bitmap_set(&tmp_fs, root_start + 1, 1);
    uint8_t zero_block[OKFS_BLOCK_SIZE];
    for (uint32_t i = 0; i < OKFS_BLOCK_SIZE; i++) zero_block[i] = 0;
    block_write(dev, data_start + root_start, zero_block);
    block_write(dev, data_start + root_start + 1, zero_block);

    uint8_t root_inode[INODE_SIZE];
    inode_encode(root_inode, "", 0, root_start, OKFS_TYPE_DIR);
    if (inode_write(&tmp_fs, 0, root_inode) != 0) return -1;

    return OKFS_OK;
}

int okfs_mount(okfs_t *fs, block_device_t *dev)
{
    if (!fs || !dev) return -1;

    uint8_t sb[OKFS_BLOCK_SIZE];
    if (block_read(dev, 0, sb) != 0) return OKFS_ERR_IO;

    uint32_t magic = read32(sb, SB_MAGIC_OFF);
    if (magic != OKFS_MAGIC) return OKFS_ERR_BAD_MAGIC;

    fs->dev            = dev;
    fs->total_blocks   = read32(sb, SB_TOTAL_BLOCKS_OFF);
    fs->bitmap_blocks  = read32(sb, SB_BITMAP_BLOCKS_OFF);
    fs->inode_count    = read32(sb, SB_INODE_COUNT_OFF);
    fs->inode_blocks   = read32(sb, SB_INODE_BLOCKS_OFF);
    fs->data_start     = read32(sb, SB_DATA_START_OFF);
    fs->mounted        = 1;

    return OKFS_OK;
}

int okfs_create(okfs_t *fs, const char *path, int type)
{
    if (!fs || !fs->mounted) return OKFS_ERR_NOT_MOUNTED;

    char parent[OKFS_PATH_MAX];
    char leaf[OKFS_NAME_MAX];
    if (split_path(path, parent, sizeof(parent), leaf, sizeof(leaf)) != 0)
        return OKFS_ERR_NOT_FOUND;

    uint32_t parent_inum = 0;
    if (parent[0] != '\0') {
        parent_inum = resolve_path(fs, parent);
        if (parent_inum == 0xFFFFFFFF) return OKFS_ERR_NOT_FOUND;
    }

    /* Check if entry already exists */
    uint32_t existing;
    if (dirent_find(fs, parent_inum, leaf, &existing) == 0)
        return OKFS_ERR_EXISTS;

    /* Allocate inode */
    uint32_t inum = inode_alloc(fs);
    if (inum == 0xFFFFFFFF) return OKFS_ERR_FULL;

    /* Write inode */
    uint8_t buf[INODE_SIZE];
    if (type == OKFS_TYPE_DIR) {
        /* Pre-allocate 2 blocks for the new directory */
        uint32_t dir_start = bitmap_find(fs, 2);
        if (dir_start == 0xFFFFFFFF) return OKFS_ERR_NO_SPACE;
        bitmap_set(fs, dir_start, 1);
        bitmap_set(fs, dir_start + 1, 1);
        uint8_t zero[OKFS_BLOCK_SIZE];
        for (uint32_t z = 0; z < OKFS_BLOCK_SIZE; z++) zero[z] = 0;
        block_write(fs->dev, fs->data_start + dir_start, zero);
        block_write(fs->dev, fs->data_start + dir_start + 1, zero);
        inode_encode(buf, leaf, 0, dir_start, (uint8_t)type);
    } else {
        inode_encode(buf, leaf, 0, 0, (uint8_t)type);
    }
    if (inode_write(fs, inum, buf) != 0) return OKFS_ERR_IO;

    /* Add directory entry to parent */
    if (dirent_add(fs, parent_inum, leaf, inum) != 0) {
        /* Rollback: clear inode */
        uint8_t zero[INODE_SIZE];
        for (uint32_t i = 0; i < INODE_SIZE; i++) zero[i] = 0;
        inode_write(fs, inum, zero);
        return OKFS_ERR_IO;
    }

    return (int)inum;
}

uint8_t *okfs_read(okfs_t *fs, const char *path, uint32_t *size_out)
{
    if (size_out) *size_out = 0;
    if (!fs || !fs->mounted) return NULL;

    uint32_t inum = resolve_path(fs, path);
    if (inum == 0xFFFFFFFF) return NULL;

    uint8_t inode_buf[INODE_SIZE];
    if (inode_read(fs, inum, inode_buf) != 0) return NULL;

    uint32_t file_size = read32(inode_buf, INODE_SIZE_OFF);
    uint32_t start     = read32(inode_buf, INODE_START_OFF);

    if (file_size == 0) {
        if (size_out) *size_out = 0;
        uint8_t *empty = (uint8_t *)kmalloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    uint8_t *data = (uint8_t *)kmalloc(file_size + 1);
    if (!data) return NULL;

    uint32_t offset = 0;
    uint32_t block = start;
    uint8_t temp[OKFS_BLOCK_SIZE];

    while (offset < file_size) {
        if (block_read(fs->dev, fs->data_start + block, temp) != 0) {
            kfree(data);
            return NULL;
        }
        uint32_t chunk = file_size - offset;
        if (chunk > OKFS_BLOCK_SIZE) chunk = OKFS_BLOCK_SIZE;
        for (uint32_t i = 0; i < chunk; i++)
            data[offset + i] = temp[i];
        offset += chunk;
        block++;  /* Contiguous blocks */
    }

    data[file_size] = '\0';
    if (size_out) *size_out = file_size;
    return data;
}

int okfs_write(okfs_t *fs, const char *path, const uint8_t *data, uint32_t size)
{
    if (!fs || !fs->mounted) return OKFS_ERR_NOT_MOUNTED;
    if (size == 0) size = 0;  /* Allow empty files */

    uint32_t inum = resolve_path(fs, path);
    int created = 0;

    if (inum == 0xFFFFFFFF) {
        /* Create the file */
        int ret = okfs_create(fs, path, OKFS_TYPE_FILE);
        if (ret < 0) return ret;
        inum = (uint32_t)ret;
        created = 1;
    }

    uint8_t inode_buf[INODE_SIZE];
    if (inode_read(fs, inum, inode_buf) != 0) return OKFS_ERR_IO;
    uint8_t type = inode_buf[INODE_TYPE_OFF];
    if (type != OKFS_TYPE_FILE) return OKFS_ERR_NOT_FILE;

    uint32_t old_start = read32(inode_buf, INODE_START_OFF);
    uint32_t old_size  = read32(inode_buf, INODE_SIZE_OFF);

    /* Calculate needed blocks */
    uint32_t needed_blocks = (size + OKFS_BLOCK_SIZE - 1) / OKFS_BLOCK_SIZE;
    if (needed_blocks == 0) needed_blocks = 0;

    uint32_t old_blocks = (old_size + OKFS_BLOCK_SIZE - 1) / OKFS_BLOCK_SIZE;

    /* Free old blocks if they exist */
    for (uint32_t b = old_start; b < old_start + old_blocks; b++) {
        bitmap_set(fs, b, 0);
    }

    if (needed_blocks == 0) {
        write32(inode_buf, INODE_SIZE_OFF, 0);
        write32(inode_buf, INODE_START_OFF, 0);
        return inode_write(fs, inum, inode_buf);
    }

    /* Allocate new contiguous blocks */
    uint32_t new_start = bitmap_find(fs, needed_blocks);
    if (new_start == 0xFFFFFFFF) {
        if (created) {
            /* Rollback creation */
            uint8_t zero[INODE_SIZE];
            for (uint32_t i = 0; i < INODE_SIZE; i++) zero[i] = 0;
            inode_write(fs, inum, zero);
        }
        return OKFS_ERR_NO_SPACE;
    }

    /* Mark blocks as used */
    for (uint32_t b = new_start; b < new_start + needed_blocks; b++) {
        bitmap_set(fs, b, 1);
    }

    /* Write data */
    uint8_t temp[OKFS_BLOCK_SIZE];
    uint32_t offset = 0;
    uint32_t block = new_start;
    while (offset < size) {
        uint32_t chunk = size - offset;
        if (chunk > OKFS_BLOCK_SIZE) chunk = OKFS_BLOCK_SIZE;
        for (uint32_t i = 0; i < chunk; i++) temp[i] = data[offset + i];
        if (chunk < OKFS_BLOCK_SIZE) {
            for (uint32_t i = chunk; i < OKFS_BLOCK_SIZE; i++) temp[i] = 0;
        }
        if (block_write(fs->dev, fs->data_start + block, temp) != 0)
            return OKFS_ERR_IO;
        offset += chunk;
        block++;
    }

    /* Update inode */
    write32(inode_buf, INODE_SIZE_OFF, size);
    write32(inode_buf, INODE_START_OFF, new_start);
    return inode_write(fs, inum, inode_buf);
}

int okfs_delete(okfs_t *fs, const char *path)
{
    if (!fs || !fs->mounted) return OKFS_ERR_NOT_MOUNTED;

    char parent[OKFS_PATH_MAX];
    char leaf[OKFS_NAME_MAX];
    if (split_path(path, parent, sizeof(parent), leaf, sizeof(leaf)) != 0)
        return OKFS_ERR_NOT_FOUND;

    uint32_t parent_inum = 0;
    if (parent[0] != '\0') {
        parent_inum = resolve_path(fs, parent);
        if (parent_inum == 0xFFFFFFFF) return OKFS_ERR_NOT_FOUND;
    }

    uint32_t inum;
    if (dirent_find(fs, parent_inum, leaf, &inum) != 0)
        return OKFS_ERR_NOT_FOUND;

    /* Read inode */
    uint8_t inode_buf[INODE_SIZE];
    if (inode_read(fs, inum, inode_buf) != 0) return OKFS_ERR_IO;
    uint8_t type = inode_buf[INODE_TYPE_OFF];

    if (type == OKFS_TYPE_DIR) {
        /* Check if empty */
        uint32_t dir_size = read32(inode_buf, INODE_SIZE_OFF);
        if (dir_size > 0) return OKFS_ERR_NOT_EMPTY;
    }

    /* Free data blocks */
    uint32_t file_size = read32(inode_buf, INODE_SIZE_OFF);
    uint32_t start     = read32(inode_buf, INODE_START_OFF);
    uint32_t blocks    = (file_size + OKFS_BLOCK_SIZE - 1) / OKFS_BLOCK_SIZE;
    for (uint32_t b = start; b < start + blocks; b++) {
        bitmap_set(fs, b, 0);
    }

    /* Clear inode (mark as unused) */
    uint8_t zero[INODE_SIZE];
    for (uint32_t i = 0; i < INODE_SIZE; i++) zero[i] = 0;
    inode_write(fs, inum, zero);

    /* We don't remove the directory entry for simplicity (it's harmless) */
    return OKFS_OK;
}

int okfs_list(okfs_t *fs, const char *path, void (*emit)(const char *))
{
    if (!fs || !fs->mounted) return OKFS_ERR_NOT_MOUNTED;

    uint32_t inum;
    if (!path || !path[0] || (path[0] == '/' && path[1] == '\0')) {
        inum = 0;  /* Root */
    } else {
        inum = resolve_path(fs, path);
        if (inum == 0xFFFFFFFF) return OKFS_ERR_NOT_FOUND;
    }

    uint8_t inode_buf[INODE_SIZE];
    if (inode_read(fs, inum, inode_buf) != 0) return OKFS_ERR_IO;
    uint8_t type = inode_buf[INODE_TYPE_OFF];
    if (type != OKFS_TYPE_DIR) return OKFS_ERR_NOT_DIR;

    uint32_t dir_size = read32(inode_buf, INODE_SIZE_OFF);
    uint32_t start    = read32(inode_buf, INODE_START_OFF);

    if (dir_size == 0) {
        emit("(empty)");
        return OKFS_OK;
    }

    uint32_t total_entries = dir_size / DIRENT_SIZE;
    uint8_t block_buf[OKFS_BLOCK_SIZE];
    int found = 0;

    for (uint32_t e = 0; e < total_entries; e++) {
        uint32_t blk = start + (e * DIRENT_SIZE) / OKFS_BLOCK_SIZE;
        uint32_t off = (e * DIRENT_SIZE) % OKFS_BLOCK_SIZE;
        if (block_read(fs->dev, fs->data_start + blk, block_buf) != 0)
            return OKFS_ERR_IO;

        uint32_t entry_inum = read32(block_buf, off);
        if (entry_inum == 0) continue;

        /* Read child inode for type/size */
        uint8_t child_buf[INODE_SIZE];
        if (inode_read(fs, entry_inum, child_buf) != 0) continue;

        uint8_t  child_type = child_buf[INODE_TYPE_OFF];
        uint32_t child_size = read32(child_buf, INODE_SIZE_OFF);
        char     child_name[OKFS_NAME_MAX];
        uint32_t ni;
        for (ni = 0; ni < OKFS_NAME_MAX - 1; ni++) {
            child_name[ni] = (char)child_buf[INODE_NAME_OFF + ni];
            if (child_name[ni] == '\0') break;
        }
        child_name[ni] = '\0';

        found = 1;
        char line[96];
        uint32_t p = 0;
        if (child_type == OKFS_TYPE_DIR) {
            const char *pfx = "[DIR]  ";
            while (*pfx && p < 90) line[p++] = *pfx++;
            for (uint32_t i = 0; child_name[i] && p < 90; i++) line[p++] = child_name[i];
        } else {
            const char *pfx = "[FILE] ";
            while (*pfx && p < 90) line[p++] = *pfx++;
            for (uint32_t i = 0; child_name[i] && p < 90; i++) line[p++] = child_name[i];
            /* Append size */
            line[p++] = ' '; line[p++] = '(';
            /* Simple number conversion */
            char num[16];
            uint32_t num_i = 0;
            uint32_t v = child_size;
            if (v == 0) num[num_i++] = '0';
            else { while (v > 0 && num_i < 15) { num[num_i++] = '0' + (v % 10); v /= 10; } }
            while (num_i > 0) line[p++] = num[--num_i];
            const char *tail = " bytes)";
            while (*tail && p < 95) line[p++] = *tail++;
        }
        line[p] = '\0';
        emit(line);
    }

    if (!found) emit("(empty)");
    return OKFS_OK;
}

int okfs_cat(okfs_t *fs, const char *path, void (*emit)(const char *))
{
    if (!fs || !fs->mounted) return OKFS_ERR_NOT_MOUNTED;

    uint32_t inum = resolve_path(fs, path);
    if (inum == 0xFFFFFFFF) return OKFS_ERR_NOT_FOUND;

    uint8_t inode_buf[INODE_SIZE];
    if (inode_read(fs, inum, inode_buf) != 0) return OKFS_ERR_IO;
    uint8_t type = inode_buf[INODE_TYPE_OFF];
    if (type != OKFS_TYPE_FILE) return OKFS_ERR_NOT_FILE;

    uint32_t size;
    uint8_t *data = okfs_read(fs, path, &size);
    if (!data) return OKFS_ERR_IO;

    if (size == 0) {
        emit("(empty file)");
        kfree(data);
        return OKFS_OK;
    }

    /* Emit line by line */
    char lbuf[128];
    uint32_t lp = 0;
    for (uint32_t i = 0; i < size; i++) {
        char c = (char)data[i];
        if (c == '\n' || lp >= 127) {
            lbuf[lp] = '\0';
            emit(lbuf);
            lp = 0;
            if (c == '\n') continue;
        }
        lbuf[lp++] = c;
    }
    if (lp > 0) { lbuf[lp] = '\0'; emit(lbuf); }

    kfree(data);
    return OKFS_OK;
}

const char *okfs_strerror(int err)
{
    switch (err) {
        case OKFS_OK:           return "OK";
        case OKFS_ERR_NOT_FOUND: return "Not found";
        case OKFS_ERR_EXISTS:   return "Already exists";
        case OKFS_ERR_NOT_DIR:  return "Not a directory";
        case OKFS_ERR_NOT_FILE: return "Not a file";
        case OKFS_ERR_NO_SPACE: return "No space on disk";
        case OKFS_ERR_IO:       return "I/O error";
        case OKFS_ERR_NOT_MOUNTED: return "Filesystem not mounted";
        case OKFS_ERR_BAD_MAGIC: return "Bad OKFS magic (wrong format?)";
        case OKFS_ERR_FULL:     return "Inode table full";
        case OKFS_ERR_NOT_EMPTY: return "Directory not empty";
        default:                return "Unknown error";
    }
}
