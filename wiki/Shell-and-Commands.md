# Shell and Commands

openkernel includes a **built-in shell** in `kernel.c` — not a separate user-space program. Commands run in kernel context on the idle thread’s stack.

## Consoles

| Mode | Description |
|------|-------------|
| **VGA text** | Default 80×25, scrollback buffer (100 lines), hardware cursor |
| **Graphics** | Framebuffer UI via `display.c`; `gfx` or Ctrl+T |
| **Pseudo-text in gfx** | Command line drawn with scaled 8×8 font |

## General commands

| Command | Description |
|---------|-------------|
| `help` | List main commands |
| `help --os` | OS commands (`os.version`, `os.bootinfo`) |
| `help --fs` | Filesystem commands |
| `clear` | Clear console (or gfx log in graphics mode) |
| `reboot` | CPU reset via keyboard controller |
| `shutdown` | QEMU shutdown ports (best-effort) |
| `gfx` | Enter graphics mode |
| `layout us` / `uk` / `it` | PS/2 keyboard layout |
| `cmd.print("message")` | Print text (quoted string, spaces allowed) |

## OS commands (`help --os`)

| Command | Description |
|---------|-------------|
| `os.version` | Prints `openkernel Kernel v0.2` (from `KERNEL_VERSION`) |
| `os.bootinfo` | Multiboot magic, flags, RAM, loader name |

## Filesystem commands (`help --fs`)

See [[Filesystem-OKFS]] for full detail.

| Command | Description |
|---------|-------------|
| `ls [path]` | List directory |
| `cd <path>` | Change working directory |
| `pwd` | Print cwd |
| `mkdir <path>` | Create directory |
| `touch <file.txt>` | Create empty text file |
| `rm <path>` | Remove file or **empty** directory |
| `cat <file.txt>` | Print file |
| `write <file.txt> "text"` | Overwrite / create |
| `append <file.txt> "text"` | Append |
| `edit <file.txt>` | Line editor mode |

## Edit mode

After `edit myfile.txt`:

- Prompt: `edit:myfile.txt> `
- Type lines of text
- **`:save`** or **`:w`** — write buffer to file and exit edit mode
- **`:q`** — discard and exit

Only **`.txt`** extensions are allowed for files.

## Keyboard shortcuts

| Key | Text mode | Graphics |
|-----|-----------|----------|
| Enter | Run command | Run / confirm |
| Backspace | Delete before cursor | Same |
| Delete | Delete at cursor | Same |
| ↑ / ↓ | Scroll history | — |
| ← / → | Move cursor in line | Move cursor |
| Ctrl+T | Toggle graphics | Toggle test / pseudo-console |
| Ctrl+Alt+Q | Quit QEMU | Quit QEMU |

## Command dispatch flow

```text
User presses Enter
   │
   ├─ fs_edit_is_active? → fs_edit_handle_line()
   ├─ fs_handle_command()? → fs.c
   └─ execute_console_command() → kernel.c (help, reboot, os.*, …)
```

## Implementation notes

- **No argv parsing library** — string compare (`streq`, prefix checks)  
- **Graphics logging** — `gfx_log[]` ring for pseudo-console output  
- **Thread safety** — `printk` / VGA updates use `cli` around critical sections  

## Related

- [[Component-Kernel-Core]]  
- [[Filesystem-OKFS]]  
- [[Component-Keyboard-and-Timer]]  
