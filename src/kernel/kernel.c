/* openkernel Kernel Entry Point */

#include "kernel.h"
#include "gdt.h"
#include "idt.h"
#include "timer.h"
#include "keyboard.h"
#include "display.h"
#include <stdarg.h>

/* VGA Text Mode Constants */
#define VGA_ADDRESS     0xB8000
#define VGA_ROWS        25
#define VGA_COLS        80
#define VGA_ATTR        0x07    /* White text on black background */

/* Global VGA position tracking */
static uint8_t cursor_row = 0;
static uint8_t cursor_col = 0;
static volatile uint16_t *vga_buffer = (volatile uint16_t *)VGA_ADDRESS;

/* External from io.asm */
extern void io_outb(uint16_t port, uint8_t data);
extern void isr_install_all(void);

/* Helper to read the EFLAGS register */
static inline uint32_t asm_get_eflags(void)
{
    uint32_t eflags;
    /* pushfl pushes the EFLAGS register onto the stack, popl retrieves it into eflags */
    asm volatile ("pushfl\n\t"
                  "popl %0"
                  : "=r"(eflags));
    return eflags;
}

/* Terminal Buffer for Scrolling (100 lines) */
#define SCROLLBACK_SIZE 100
static uint16_t terminal_history[SCROLLBACK_SIZE][VGA_COLS];
static int terminal_total_lines = 1;
static int scroll_view_offset = 0; /* 0 = showing the latest lines */

/* Update the hardware cursor (the blinking horizontal bar) */
static void vga_update_hardware_cursor(void)
{
    /* Calculate where the last written character is on screen */
    int start_row = terminal_total_lines - VGA_ROWS - scroll_view_offset;
    if (start_row < 0) start_row = 0;

    int screen_row = (terminal_total_lines - 1) - start_row;
    uint16_t pos;

    /* Clamp to visible screen area */
    if (screen_row >= 0 && screen_row < VGA_ROWS) {
        pos = screen_row * VGA_COLS + cursor_col;
    } else if (screen_row >= VGA_ROWS) {
        /* Last line is scrolled off: put cursor on last visible line */
        pos = (VGA_ROWS - 1) * VGA_COLS + cursor_col;
    } else {
        /* Last line is above visible area: hide cursor */
        pos = VGA_ROWS * VGA_COLS;
    }

    io_outb(0x3D4, 0x0F);
    io_outb(0x3D5, (uint8_t)(pos & 0xFF));
    io_outb(0x3D4, 0x0E);
    io_outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/* Helper: Refresh VGA from the history buffer based on scroll offset */
static void vga_refresh(void)
{
    int start_row = terminal_total_lines - VGA_ROWS - scroll_view_offset;
    if (start_row < 0) start_row = 0;

    for (int r = 0; r < VGA_ROWS; r++) {
        for (int c = 0; c < VGA_COLS; c++) {
            if (start_row + r < terminal_total_lines) {
                vga_buffer[r * VGA_COLS + c] = terminal_history[start_row + r][c];
            } else {
                vga_buffer[r * VGA_COLS + c] = ' ' | (VGA_ATTR << 8);
            }
        }
    }

    /* Sync the hardware cursor with the current position */
    vga_update_hardware_cursor();
}

/* Public: Adjust scroll view offset */
void vga_scroll(int delta)
{
    scroll_view_offset += delta;
    if (scroll_view_offset < 0) scroll_view_offset = 0;
    
    int max_offset = terminal_total_lines - VGA_ROWS;
    if (max_offset < 0) max_offset = 0;
    if (scroll_view_offset > max_offset) scroll_view_offset = max_offset;

    /* Optional: Provide visual feedback if we're at the limit */
    if (delta > 0 && scroll_view_offset == max_offset && max_offset > 0) {
        /* We reached the top of the history */
    }

    vga_refresh();
}

/* Helper: Clear the screen */
static void vga_clear(void)
{
    for (int i = 0; i < SCROLLBACK_SIZE; i++) {
        for (int j = 0; j < VGA_COLS; j++) {
            terminal_history[i][j] = ' ' | (VGA_ATTR << 8);
        }
    }
    terminal_total_lines = 1;
    scroll_view_offset = 0;
    cursor_row = 0;
    cursor_col = 0;
    vga_refresh();
}

/* Public: Clear the screen */
void vga_clear_public(void)
{
    vga_clear();
}

/* Helper: Write a single character to VGA and scrollback buffer */
static void vga_putchar_with_scrollback(char c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else {
        terminal_history[terminal_total_lines - 1][cursor_col] = c | (VGA_ATTR << 8);
        cursor_col++;
        if (cursor_col >= VGA_COLS) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row > 0) { /* Newline or Wrap occurred */
        terminal_total_lines++;
        if (terminal_total_lines > SCROLLBACK_SIZE) {
            /* Shift buffer up to keep history within SCROLLBACK_SIZE */
            for (int i = 0; i < SCROLLBACK_SIZE - 1; i++) {
                for (int j = 0; j < VGA_COLS; j++) terminal_history[i][j] = terminal_history[i+1][j];
            }
            /* Clear the newly created line at the bottom */
            for (int j = 0; j < VGA_COLS; j++) {
                terminal_history[SCROLLBACK_SIZE - 1][j] = ' ' | (VGA_ATTR << 8);
            }
            terminal_total_lines = SCROLLBACK_SIZE;
        }
        cursor_row = 0;
        vga_refresh(); /* Refresh screen only on line break */
    }
}

/* Public: Write a single character */
void putchar(char c) 
{ 
    uint32_t eflags = asm_get_eflags();
    asm volatile("cli");

    vga_putchar_with_scrollback(c); 
    vga_refresh();

    /* Restore interrupt state: only enable if they were enabled before */
    if (eflags & 0x200) asm volatile("sti");
}

/* Public: Write a string */
void puts(const char *str)
{
    uint32_t eflags = asm_get_eflags();
    asm volatile("cli");

    while (*str) vga_putchar_with_scrollback(*str++);
    vga_refresh();

    if (eflags & 0x200) asm volatile("sti");
}

/* Public: Printf-style printing with scrollback */
void printk(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    /* Use a temporary buffer to format the string */
    char buffer[512];
    int buf_idx = 0;

    while (*format && buf_idx < 511) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd': {
                    long value = va_arg(args, int);
                    unsigned long uvalue;
                    if (value < 0 && buf_idx < 511) { 
                        buffer[buf_idx++] = '-'; 
                        uvalue = (unsigned long)-value; 
                    } else {
                        uvalue = (unsigned long)value;
                    }

                    if (uvalue == 0 && buf_idx < 511) { buffer[buf_idx++] = '0'; }
                    else { 
                        char temp[16]; int t_idx = 0;
                        while (uvalue > 0) { temp[t_idx++] = '0' + (uvalue % 10); uvalue /= 10; }
                        while (t_idx > 0 && buf_idx < 511) buffer[buf_idx++] = temp[--t_idx];
                    }
                    break;
                }
                case 'x': {
                    unsigned int value = va_arg(args, unsigned int);
                    const char *hex = "0123456789abcdef";
                    for (int i = 28; i >= 0; i -= 4) {
                        if (buf_idx < 511) buffer[buf_idx++] = hex[(value >> i) & 0xF];
                    }
                    break;
                }
                case 's': {
                    const char *str = va_arg(args, const char *);
                    while (*str && buf_idx < 511) buffer[buf_idx++] = *str++;
                    break;
                }
                case '%': buffer[buf_idx++] = '%'; break;
                default:  buffer[buf_idx++] = '%'; buffer[buf_idx++] = *format; break;
            }
        } else {
            buffer[buf_idx++] = *format;
        }
        format++;
    }
    buffer[buf_idx] = '\0';
    va_end(args);

    /* Always output to VGA text mode */
    char *ptr = buffer;

    uint32_t eflags = asm_get_eflags();
    asm volatile("cli");

    while (*ptr) {
        vga_putchar_with_scrollback(*ptr++);
    }
    vga_refresh(); /* Single refresh after the entire string is processed */

    if (eflags & 0x200) asm volatile("sti");
}

/* Forward declarations for functions defined in other modules */
void memory_init(uint32_t mem_lower, uint32_t mem_upper);
void process_init(void);
void keyboard_detect_layout(const char* cmdline);
extern int graphics_toggle_requested;
int graphics_mode_active = 0;
static int pseudo_text_mode_active = 0;

#define GFX_LOG_LINES 24
#define GFX_LOG_COLS  64
static char gfx_log[GFX_LOG_LINES][GFX_LOG_COLS];
static int gfx_log_count = 0;
static char pseudo_cmd[64];
static int pseudo_cmd_len = 0;

static void append_gfx_log(const char *msg)
{
    int slot = gfx_log_count % GFX_LOG_LINES;
    int i = 0;
    while (msg[i] && i < GFX_LOG_COLS - 1) {
        gfx_log[slot][i] = msg[i];
        i++;
    }
    gfx_log[slot][i] = '\0';
    gfx_log_count++;
}

static void render_pseudo_text_screen(void)
{
    display_clear(display_rgb(0, 0, 0x40));
    display_puts("openkernel Text Console (graphics fallback)", 20, 20, display_rgb(0xFF, 0xFF, 0xFF));
    display_puts("Commands: layout us|uk|it, reboot, shutdown", 20, 44, display_rgb(0xD0, 0xD0, 0xD0));
    display_puts("Press Ctrl+T to return to graphics test mode", 20, 64, display_rgb(0xB0, 0xB0, 0xB0));

    int lines = (gfx_log_count < GFX_LOG_LINES) ? gfx_log_count : GFX_LOG_LINES;
    int start = gfx_log_count - lines;
    for (int i = 0; i < lines; i++) {
        int idx = (start + i) % GFX_LOG_LINES;
        display_puts(gfx_log[idx], 20, 94 + (uint32_t)i * 18, display_rgb(0xFF, 0xFF, 0xFF));
    }

    char prompt[80] = "> ";
    int p = 2;
    for (int i = 0; i < pseudo_cmd_len && p < 77; i++) prompt[p++] = pseudo_cmd[i];
    prompt[p++] = '_'; /* typing bar */
    prompt[p] = '\0';
    display_puts(prompt, 20, 560, display_rgb(0xFF, 0xFF, 0x80));
}

static void render_graphics_test_screen(void)
{
    const display_mode_t *mode = display_get_mode();
    display_clear(display_rgb(0, 0, 0x40));

    display_fill_rect(30, 30, 100, 100, display_rgb(0xFF, 0, 0));
    display_fill_rect(150, 30, 100, 100, display_rgb(0, 0xFF, 0));
    display_fill_rect(270, 30, 100, 100, display_rgb(0, 0, 0xFF));
    display_fill_rect(390, 30, 100, 100, display_rgb(0xFF, 0xFF, 0));
    display_fill_rect(510, 30, 100, 100, display_rgb(0xFF, 0, 0xFF));

    display_draw_rect(30, 160, 100, 100, display_rgb(0xFF, 0xFF, 0xFF));
    display_draw_rect(150, 160, 100, 100, display_rgb(0xFF, 0xFF, 0xFF));
    display_draw_rect(270, 160, 100, 100, display_rgb(0xFF, 0xFF, 0xFF));
    display_draw_rect(390, 160, 100, 100, display_rgb(0xFF, 0xFF, 0xFF));
    display_draw_rect(510, 160, 100, 100, display_rgb(0xFF, 0xFF, 0xFF));

    display_puts("TEST TEXT! TEST TEXT! TEST TEXT!", 30, 320, display_rgb(0xFF, 0xFF, 0xFF));
    display_puts("TEST TEXT! TEST TEXT! TEST TEXT!", 30, 340, display_rgb(0xB0, 0xB0, 0xB0));
    display_puts("TEST TEXT! TEST TEXT! TEST TEXT!", 30, 360, display_rgb(0x80, 0xFF, 0x80));
    (void)mode;
}

static void __attribute__((unused)) show_text_home_screen(void)
{
    vga_clear();
    printk("openkernel Kernel v0.2\n");
    printk("Built: " __DATE__ " " __TIME__ "\n\n");
    printk("=== System Ready ===\n");
    printk("Press Ctrl+T to toggle graphics test mode.\n");
}

static void loading_tick_inline(const char *component, int *frame)
{
    const int total_steps = 5;
    int step = *frame + 1;
    if (step > total_steps) step = total_steps;

    char bar[6];
    for (int i = 0; i < total_steps; i++) {
        bar[i] = (i < step) ? '#' : '-';
    }
    bar[5] = '\0';

    printk("[Loading %d/%d] [%s] %s\n", step, total_steps, bar, component);
    (*frame)++;
}

static int streq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

static void reboot_system(void)
{
    io_outb(0x64, 0xFE);
    while (1) asm volatile("hlt");
}

static void shutdown_system(void)
{
    asm volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0xB004));
    asm volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
    asm volatile ("outw %0, %1" : : "a"((uint16_t)0x3400), "Nd"((uint16_t)0x4004));
    halt();
}

static void pseudo_console_execute(void)
{
    pseudo_cmd[pseudo_cmd_len] = '\0';
    append_gfx_log(pseudo_cmd_len ? pseudo_cmd : "(empty)");

    if (streq(pseudo_cmd, "layout us")) {
        keyboard_detect_layout("layout=us");
        append_gfx_log("OK: keyboard layout set to US");
    } else if (streq(pseudo_cmd, "layout uk")) {
        keyboard_detect_layout("layout=uk");
        append_gfx_log("OK: keyboard layout set to UK");
    } else if (streq(pseudo_cmd, "layout it")) {
        keyboard_detect_layout("layout=it");
        append_gfx_log("OK: keyboard layout set to IT");
    } else if (streq(pseudo_cmd, "reboot")) {
        append_gfx_log("Rebooting...");
        render_pseudo_text_screen();
        reboot_system();
    } else if (streq(pseudo_cmd, "shutdown")) {
        append_gfx_log("Shutting down...");
        render_pseudo_text_screen();
        shutdown_system();
    } else {
        append_gfx_log("Unknown command");
    }

    pseudo_cmd_len = 0;
    pseudo_cmd[0] = '\0';
}

/* Kernel initialization */
void kernel_init(void)
{
    int load_frame = 0;
    vga_clear();
    printk("openkernel Kernel v0.2\n");
    printk("Built: " __DATE__ " " __TIME__ "\n\n");

    loading_tick_inline("GDT", &load_frame);
    printk("Initializing GDT...\n");
    gdt_init();
    printk("  [OK]\n");

    loading_tick_inline("IDT/PIC", &load_frame);
    printk("Initializing IDT/PIC...\n");
    idt_init();
    printk("  [OK]\n");

    loading_tick_inline("ISRs", &load_frame);
    printk("Installing ISRs...\n");
    isr_install_all();
    printk("  [OK]\n");

    loading_tick_inline("PIT Timer", &load_frame);
    printk("Initializing PIT timer @100Hz...\n");
    timer_init(100);
    printk("  [OK]\n");

    loading_tick_inline("PS/2 Keyboard", &load_frame);
    printk("Initializing PS/2 keyboard...\n");
    keyboard_init();
    printk("  [OK]\n");
}

/* Main kernel entry point */
void kernel_main(uint32_t magic, multiboot_info_t *mbd)
{
    if (magic != 0x2BADB002) {
        while (1) { asm("hlt"); }
    }

    kernel_init();

    /* Automatically detect keyboard layout from Multiboot command line */
    if (mbd->flags & (1 << 2)) {
        keyboard_detect_layout((const char*)mbd->cmdline);
    }

    /* Initialize memory management */
    printk("Initializing memory management...\n");
    memory_init(mbd->mem_lower, mbd->mem_upper);
    printk("  [OK]\n\n");

    /* Initialize process management */
    printk("Initializing process management...\n");
    process_init();
    printk("  [OK]\n\n");

    printk("Enabling interrupts and starting scheduler...\n");
    enable_interrupts();

    if (asm_get_eflags() & 0x200) {
        printk("  [OK] System Ready (EFLAGS: 0x%x)\n\n", asm_get_eflags());
    } else {
        printk("  [FAIL] Interrupts did not enable!\n\n");
    }

    /* Boot info */
    printk("=== openkernel Boot Info ===\n");
    printk("Magic: 0x%x  Flags: 0x%x\n", magic, mbd->flags);
    printk("Memory: %d KB lower, %d KB upper (%d MB total)\n",
           mbd->mem_lower, mbd->mem_upper,
           (mbd->mem_upper + mbd->mem_lower) / 1024);
    if (mbd->boot_loader_name)
        printk("Loader: %s\n", (const char *)mbd->boot_loader_name);

    printk("\n=== System Ready ===\n");
    printk("Press Ctrl+T to toggle graphics test mode.\n");

    /* Simple idle loop with Ctrl+T graphics toggle */
    int keys_seen = 0;
    while (1) {
        /* Handle Ctrl+T graphics toggle */
        if (graphics_toggle_requested) {
            graphics_toggle_requested = 0; /* Consume the request */

            if (graphics_mode_active) {
                if (pseudo_text_mode_active) {
                    pseudo_text_mode_active = 0;
                    render_graphics_test_screen();
                } else {
                    pseudo_text_mode_active = 1;
                    render_pseudo_text_screen();
                }
            } else {
                /* Enter graphics mode silently */
                if (display_enter_gfx()) {
                    graphics_mode_active = 1;
                    pseudo_text_mode_active = 0;
                    render_graphics_test_screen();
                }
            }
        }

        /* Process all keys currently in the buffer */
        {
            uint8_t sc;
            while ((sc = keyboard_get_scancode()) != 0) {
                char key = keyboard_scancode_to_ascii(sc);
                if (!graphics_mode_active) {
                    printk("[%d] Scancode: 0x%x -> Char: '%c'\n", ++keys_seen, sc, key ? key : '?');
                } else if (pseudo_text_mode_active) {
                    (void)key;
                    if (sc == 0x1C) { /* Enter */
                        pseudo_console_execute();
                    } else if (sc == 0x0E) { /* Backspace */
                        if (pseudo_cmd_len > 0) pseudo_cmd[--pseudo_cmd_len] = '\0';
                    } else {
                        char ch = keyboard_scancode_to_ascii(sc);
                        if (ch >= 32 && ch <= 126 && pseudo_cmd_len < (int)sizeof(pseudo_cmd) - 1) {
                            pseudo_cmd[pseudo_cmd_len++] = ch;
                            pseudo_cmd[pseudo_cmd_len] = '\0';
                        }
                    }
                    render_pseudo_text_screen();
                }
            }
        }

        if (!graphics_mode_active) {
            vga_update_hardware_cursor();
        }
        asm volatile ("hlt");
    }
}

/* Halt the CPU */
void halt(void)
{
    asm volatile ("cli\n1:\nhlt\njmp 1b\n");
}
