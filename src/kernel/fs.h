/* openkernel - OKFS in-memory filesystem */
#ifndef FS_H
#define FS_H

#include <stddef.h>
#include <stdint.h>

#define FS_MAX_PATH      256
#define FS_MAX_NAME      48
#define FS_MAX_FILE_SIZE (64 * 1024)

#define FS_OK            0
#define FS_ERR_NOT_FOUND (-1)
#define FS_ERR_EXISTS    (-2)
#define FS_ERR_NOT_DIR   (-3)
#define FS_ERR_NOT_FILE  (-4)
#define FS_ERR_NOT_TXT   (-5)
#define FS_ERR_NO_SPACE  (-6)
#define FS_ERR_INVALID   (-7)
#define FS_ERR_FULL      (-8)
#define FS_ERR_NOT_EMPTY (-9)

typedef enum {
    FS_TYPE_DIR = 0,
    FS_TYPE_FILE = 1
} fs_type_t;

void fs_init(void);

const char *fs_pwd(void);
int fs_cd(const char *path);
int fs_mkdir(const char *path);
int fs_touch(const char *path);
int fs_rm(const char *path);
int fs_ls(const char *path, void (*emit)(const char *));
int fs_cat(const char *path, void (*emit)(const char *));
int fs_write_file(const char *path, const char *data, size_t len);
int fs_append_file(const char *path, const char *data, size_t len);

const char *fs_strerror(int err);

/* Interactive .txt editor (:save / :q) */
int fs_edit_is_active(void);
const char *fs_edit_path(void);
int fs_edit_begin(const char *path);
int fs_edit_handle_line(const char *line, void (*emit)(const char *));

/* Shell: returns 1 if command was handled */
int fs_handle_command(const char *command, void (*emit)(const char *));
void fs_show_help(void (*emit)(const char *));

#endif /* FS_H */
