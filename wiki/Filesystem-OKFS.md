# Filesystem (OKFS)

**OKFS** (openkernel filesystem) is an **in-memory**, hierarchical filesystem for teaching paths, directories, and file I/O. Data is lost on reboot — there is no disk driver yet.

**Source:** `src/kernel/fs.c`, `src/kernel/fs.h`

## Features

- Tree of directories and files  
- Paths: absolute (`/docs/foo.txt`) or relative to **cwd**  
- Only **`.txt`** files (enforced on create/write)  
- Max **128** nodes, **64 KB** per file  
- Shell integration via `fs_handle_command()`  

## Preloaded files

| Path | Content |
|------|---------|
| `/readme.txt` | Welcome and quick tips |
| `/docs/about.txt` | Project blurb |
| `/docs/` | Directory |

## Data model

Each entry is a `fs_node_t`:

| Field | Meaning |
|-------|---------|
| `used` | Slot active |
| `parent` | Parent node index (root = 0) |
| `type` | `FS_TYPE_DIR` or `FS_TYPE_FILE` |
| `name` | Single path component (max 48 chars) |
| `data` | Heap buffer (files only) |
| `size` / `cap` | File length and allocated capacity |

**Root** node (index 0): directory with empty name, path `/`.

## Path resolution

1. If path starts with `/`, walk from root; else from **cwd**.  
2. Split on `/`, apply `.` and `..`.  
3. `fs_resolve()` returns node index or error.  

**cwd** updated by `fs_cd()`; `fs_pwd()` returns full path string.

## API reference

| Function | Description |
|----------|-------------|
| `fs_init()` | Create root, seed files |
| `fs_mkdir(path)` | New directory |
| `fs_touch(path)` | Create empty `.txt` |
| `fs_rm(path)` | Remove file or empty dir |
| `fs_ls(path, emit)` | List children |
| `fs_cat(path, emit)` | Print file line-by-line |
| `fs_write_file(path, data, len)` | Replace contents |
| `fs_append_file(path, data, len)` | Append |
| `fs_cd` / `fs_pwd` | Navigation |

### Error codes

| Code | Meaning |
|------|---------|
| `FS_OK` | Success |
| `FS_ERR_NOT_FOUND` | Missing path |
| `FS_ERR_EXISTS` | Name collision |
| `FS_ERR_NOT_DIR` | Expected directory |
| `FS_ERR_NOT_FILE` | Expected file |
| `FS_ERR_NOT_TXT` | Not a `.txt` name |
| `FS_ERR_NO_SPACE` | Heap or size limit |
| `FS_ERR_NOT_EMPTY` | Directory has children |

## Editor internals

`fs_edit_begin()` loads file into `edit_buf` (kmalloc).  
Each line in edit mode appends text + newline.  
`:save` calls `fs_write_file()` with buffer contents.

## Example session

```text
> ls
[FILE] readme.txt (89 bytes)
[DIR]  docs

> cd docs
> pwd
/docs

> cat about.txt
openkernel educational filesystem.
...

> write notes.txt "First line of my note"
Written.

> edit notes.txt
Edit mode. Type lines, then :save or :q
notes.txt
> Second line
> :save
Saved.
```

## Future: disk-backed OKFS

Planned evolution (see [[Roadmap]]):

1. **Block layer** — 512-byte sectors (RAM disk or ATA)  
2. Same path API — swap backing store from heap to disk  
3. Optional persistence across reboots in QEMU  

## Related

- [[Component-Filesystem]]  
- [[Shell-and-Commands]]  
- [[Component-Memory]] — `kmalloc` backing store  
