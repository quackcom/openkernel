# Component: Filesystem (OKFS)

**Files:** `src/kernel/fs.c`, `src/kernel/fs.h`

## Purpose

In-kernel hierarchical filesystem with shell commands and interactive `.txt` editing. See [[Filesystem-OKFS]] for user-facing guide.

## Architecture

```text
Shell (kernel.c)
      │
      ▼
fs_handle_command()  ──► parse verb + path
      │
      ▼
fs_* API (mkdir, cat, write, …)
      │
      ▼
nodes[128] tree + kmalloc file buffers
```

## Internal structures

- **`fs_node_t`** — directory or file node in static array  
- **Root** index 0, name `""`, path `/`  
- **cwd** index + `cwd_path[256]` string cache  

## Path operations

| Helper | Role |
|--------|------|
| `fs_resolve(start, path, want_dir)` | Walk components |
| `fs_resolve_parent(path, &name, &parent)` | Split parent + basename |
| `fs_find_child(parent, name)` | Lookup one level |
| `fs_create_entry` | Allocate slot, validate `.txt` for files |

## File I/O

- `fs_set_file_data` — grow buffer with `kmalloc`, copy bytes, support append  
- Max size `FS_MAX_FILE_SIZE` (64 KB)  

## Editor state (static)

- `edit_active`, `edit_path[]`, `edit_buf`, `edit_len`  
- `fs_edit_handle_line` — `:save`, `:q`, or append line  

## Shell integration

Returns **1** from `fs_handle_command` if command recognized (even on error — caller prints message).

Commands not handled return **0** so `kernel.c` can try built-ins.

## Error reporting

`fs_strerror(code)` → human string  
`emit_err()` in shell wrapper prefixes `"Error: "`  

## Limits and validation

| Limit | Value |
|-------|-------|
| Nodes | 128 |
| Name length | 48 |
| Path length | 256 |
| File size | 64 KB |
| Write command text | 4096 (`FS_CMD_TEXT_MAX`) |

Invalid names: empty, `.`, `..`, slashes inside name, non-`.txt` files.

## Extension points

1. **Disk blocks** — replace `fs_set_file_data` backing with sector I/O  
2. **Permissions** — uid/gid per node  
3. **Non-text types** — new magics or extensions  

## Related

- [[Filesystem-OKFS]]  
- [[Component-Memory]]  
- [[Shell-and-Commands]]  
