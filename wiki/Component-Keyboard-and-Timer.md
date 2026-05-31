# Component: Keyboard and Timer

**Files:** `keyboard.c/h`, `keyboard.asm`, `timer.c/h`

## Purpose

**Timekeeping** and **user input** via standard PC hardware: PIT (IRQ 0) and PS/2 keyboard (IRQ 1).

## Timer (PIT)

### Hardware

- I/O ports: command `0x43`, channel 0 data `0x40`  
- Input clock ~1.193182 MHz  
- Divisor = `1193182 / frequency`  

### API

| Function | Description |
|----------|-------------|
| `timer_init(frequency)` | Program PIT, register IRQ 0 handler (default 100 Hz) |
| `timer_get_ticks()` | Monotonic tick counter |
| `timer_sleep(ms)` | Busy-wait on ticks |

### IRQ handler

Each tick:

1. Increment `tick_count`  
2. `loading_status_tick()` if boot animation active  
3. Every **10 ticks** → `scheduler_schedule()` (preemption)  

## Keyboard (PS/2)

### Initialization (`keyboard_init`)

- Enable keyboard port via 8042 controller  
- Reset and configure scan code set  
- Register IRQ 1 handler  

### Scancode pipeline

1. **ISR** reads scan code from port `0x60`  
2. Pushes to ring buffer  
3. Main loop: `keyboard_get_scancode()` (non-blocking)  
4. `keyboard_scancode_to_ascii()` — layout-aware translation  

### Layouts

| ID | Layout | Set via |
|----|--------|---------|
| 0 | US QWERTY | `layout us` or `layout=us` in cmdline |
| 1 | UK | `layout uk` |
| 2 | Italian | `layout it` |

`keyboard_detect_layout()` parses Multiboot cmdline or shell command.

### Modifier state

Tracks Shift, Ctrl, Alt, Caps/Num/Scroll lock for correct ASCII and shortcuts.

### Special keys (flags for main loop)

| Flag | Key |
|------|-----|
| `arrow_*_requested` | Arrow keys |
| `delete_requested` | Delete |
| `graphics_toggle_requested` | Ctrl+T |

### Assembly helpers (`keyboard.asm`)

Low-level port I/O if used for controller access (alongside C).

## Integration with shell

Main loop polls scancodes — **not** blocking read. This keeps the design simple without a input block queue in the shell itself.

Edit mode and command line share the same scancode path.

## Related

- [[Component-CPU-GDT-IDT-ISR]]  
- [[Shell-and-Commands]]  
- [[Component-Processes-and-Scheduler]]  
