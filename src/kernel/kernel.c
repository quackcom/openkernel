/* openkernel Kernel Entry Point */

#include "kernel.h"
#include "gdt.h"
#include "idt.h"
#include "timer.h"
#include "keyboard.h"
#include "display.h"
#include "fs.h"
#include "block.h"
#include "okfs_disk.h"
#include "../drivers/ata.h"
#include "../drivers/ramdisk.h"
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

/* Text mode command buffer */
#define CMD_BUFFER_SIZE 128
static char cmd[CMD_BUFFER_SIZE];
static int cmd_len = 0;
static int cmd_cursor = 0;
static int loading_status_active = 0;
static int loading_status_line = -1;
static uint32_t loading_status_frame = 0;
static char loading_status_label[48];
static int loading_status_progress = 0;      /* 0-100 percentuale per la progress bar */

/* Saved boot information for os.bootinfo command */
static uint32_t saved_magic = 0;
static uint32_t saved_flags = 0;
static uint32_t saved_mem_lower = 0;
static uint32_t saved_mem_upper = 0;
static char saved_boot_loader[64] = "";
static int saved_boot_loader_valid = 0;

/* Disk-based filesystem context */
static okfs_t g_disk_fs;
static char g_disk_cwd[FS_MAX_PATH] = "/";

/* Resolve a disk path: if absolute (starts with /), use as-is;
   otherwise prepend current working directory.
   Result is written to out (max OUT_MAX chars). Returns out. */
static const char *disk_resolve_path(const char *path, char *out, size_t out_max)
{
    if (!path || !path[0]) {
        out[0] = '\0';
        return out;
    }
    if (path[0] == '/') {
        size_t i = 0;
        while (path[i] && i < out_max - 1) { out[i] = path[i]; i++; }
        out[i] = '\0';
        return out;
    }
    /* Relative — prepend cwd */
    size_t cwdi = 0;
    while (g_disk_cwd[cwdi] && cwdi < out_max - 2) { out[cwdi] = g_disk_cwd[cwdi]; cwdi++; }
    /* Ensure cwd ends with / */
    if (cwdi == 0 || out[cwdi - 1] != '/') { out[cwdi++] = '/'; }
    size_t pi = 0;
    while (path[pi] && cwdi < out_max - 2) { out[cwdi++] = path[pi++]; }
    out[cwdi] = '\0';
    return out;
}

/* Normalize a path by stripping trailing slash (except for root). */
static void disk_normalize_path(char *path)
{
    size_t len = 0;
    while (path[len]) len++;
    while (len > 1 && path[len - 1] == '/') path[--len] = '\0';
}

/* Disk edit tracking: when d:edit copies a file to in-memory FS for
   editing, this stores the real disk path for save-back. */
#define DISK_EDIT_TEMP "/__disk_edit_temp__"
static int g_disk_edit_active = 0;
static char g_disk_edit_path[FS_MAX_PATH];

static void vga_replace_line_text(int line, const char *text);
static void vga_reserve_prompt_line(void);
static void render_loading_status(void);

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
extern volatile int arrow_left_requested;
extern volatile int arrow_right_requested;
extern volatile int arrow_up_requested;
extern volatile int arrow_down_requested;
extern volatile int delete_requested;
int graphics_mode_active = 0;
static int pseudo_text_mode_active = 0;

#define GFX_LOG_LINES 24
#define GFX_LOG_COLS  64
static char gfx_log[GFX_LOG_LINES][GFX_LOG_COLS];
static int gfx_log_count = 0;
static char pseudo_cmd[64];
static int pseudo_cmd_len = 0;
static int pseudo_cmd_cursor = 0;

static void clear_gfx_log(void)
{
    for (int i = 0; i < GFX_LOG_LINES; i++) {
        gfx_log[i][0] = '\0';
    }
    gfx_log_count = 0;
}

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

static void append_gfx_prompt_line(const char *command)
{
    char line[GFX_LOG_COLS];
    int idx = 0;

    line[idx++] = '>';
    line[idx++] = ' ';

    while (*command && idx < GFX_LOG_COLS - 1) {
        line[idx++] = *command++;
    }

    line[idx] = '\0';
    append_gfx_log(line);
}

#define GFX_CONSOLE_BG      display_rgb(0, 0, 0x40)
#define GFX_PROMPT_Y        560
#define GFX_PROMPT_BAR_Y    558
#define GFX_PROMPT_BAR_H    24
#define GFX_LOG_Y0          64
#define GFX_LOG_LINE_STEP   (DISPLAY_CHAR_H + 4)

static void build_pseudo_prompt(char *prompt, int max_len)
{
    const char *prefix = "> ";
    int p = 0;

    while (*prefix && p < max_len - 1) {
        prompt[p++] = *prefix++;
    }

    for (int i = 0; i < pseudo_cmd_len && p < max_len - 1; i++) {
        if (i == pseudo_cmd_cursor) {
            prompt[p++] = '_';
        }
        prompt[p++] = pseudo_cmd[i];
    }
    if (pseudo_cmd_cursor >= pseudo_cmd_len && p < max_len - 1) {
        prompt[p++] = '_';
    }
    prompt[p] = '\0';
}

static void render_pseudo_text_prompt(void)
{
    const display_mode_t *mode = display_get_mode();
    char prompt[80];

    build_pseudo_prompt(prompt, (int)sizeof(prompt));
    display_fill_rect(0, GFX_PROMPT_BAR_Y, mode->width, GFX_PROMPT_BAR_H, GFX_CONSOLE_BG);
    display_puts_line(prompt, 20, GFX_PROMPT_Y, display_rgb(0xFF, 0xFF, 0x80));
}

static void render_pseudo_text_screen(void)
{
    display_clear(GFX_CONSOLE_BG);
    display_puts("openkernel Command Line", 20, 20, display_rgb(0xFF, 0xFF, 0xFF));
    display_puts("Type 'help' for available commands.", 20, 40, display_rgb(0xA0, 0xA0, 0xA0));

    int lines = (gfx_log_count < GFX_LOG_LINES) ? gfx_log_count : GFX_LOG_LINES;
    int start = gfx_log_count - lines;
    for (int i = 0; i < lines; i++) {
        int idx = (start + i) % GFX_LOG_LINES;
        display_puts_line(gfx_log[idx], 20, GFX_LOG_Y0 + (uint32_t)i * GFX_LOG_LINE_STEP,
                          display_rgb(0xFF, 0xFF, 0xFF));
    }

    render_pseudo_text_prompt();
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
    printk(KERNEL_VERSION_STRING "\n");
    printk("Built: " __DATE__ " " __TIME__ "\n\n");
    printk("=== System Ready ===\n");
    printk("Press Ctrl+T to toggle graphics test mode.\n");
}

static void loading_tick_inline(const char *component, int *frame)
{
    const int total_steps = 5;
    int step = *frame + 1;
    char bar[6];

    if (step > total_steps) step = total_steps;

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

static size_t kstrlen(const char *str)
{
    size_t len = 0;
    while (str[len] != '\0') len++;
    return len;
}

/* Extracts content between parentheses from a command.
   Supports double-quoted strings: cmd.print("hello world")
   Without quotes: extracts up to ')', no spaces
   With quotes: strips the quotes and allows spaces in content
   Returns 1 on success, 0 on malformed syntax. */
static int extract_parenthesis_content(const char *command, char *out, int max_len)
{
    while (*command == ' ') command++;

    while (*command && *command != '(') command++;
    if (*command != '(') return 0;
    command++; /* skip '(' */

    /* Skip whitespace after opening paren */
    while (*command == ' ') command++;

    int len = 0;

    if (*command == '"') {
        /* Quoted string — extract until closing quote, allows spaces */
        command++; /* skip opening " */
        while (*command && *command != '"' && len < max_len - 1) {
            out[len++] = *command++;
        }
        if (*command != '"') return 0; /* unclosed quote */
        command++; /* skip closing " */
    } else {
        /* Unquoted — extract until ')' */
        while (*command && *command != ')' && len < max_len - 1) {
            out[len++] = *command++;
        }
    }

    /* Skip trailing whitespace before closing paren */
    while (*command == ' ') command++;
    if (*command != ')') return 0;

    out[len] = '\0';
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Loading-status animation with progress bar support.               */
/*  I dots animati partono immediatamente (non dipendono dal timer).  */
/*  La progress bar si riempie da 0% a 100% tramite                  */
/*  loading_status_set_progress().                                    */
/* ------------------------------------------------------------------ */
void loading_status_start(const char *label)
{
    int idx = 0;

    loading_status_active = 1;
    loading_status_line = terminal_total_lines - 1;
    loading_status_frame = 0;

    while (label[idx] && idx < (int)sizeof(loading_status_label) - 1) {
        loading_status_label[idx] = label[idx];
        idx++;
    }
    loading_status_label[idx] = '\0';

    /* Show the label immediately (no dots yet) */
    render_loading_status();
}

void loading_status_tick(void)
{
    if (!loading_status_active) {
        return;
    }

    loading_status_frame++;
    render_loading_status();
}

void loading_status_set_progress(int percent)
{
    if (!loading_status_active) {
        return;
    }

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    loading_status_progress = percent;
    render_loading_status();
}

void loading_status_done(void)
{
    if (!loading_status_active) {
        return;
    }

    loading_status_active = 0;

    /* Sovrascrivi la linea con [OK] invece di andare a capo */
    char ok_line[80];
    int pos = 0;
    ok_line[pos++] = '[';
    ok_line[pos++] = 'O';
    ok_line[pos++] = 'K';
    ok_line[pos++] = ']';
    ok_line[pos++] = ' ';
    for (int i = 0; loading_status_label[i] && pos < 79; i++) {
        ok_line[pos++] = loading_status_label[i];
    }
    ok_line[pos] = '\0';
    vga_replace_line_text(loading_status_line, ok_line);
}

static void vga_replace_line_text(int line, const char *text)
{
    int col = 0;

    if (line < 0 || line >= SCROLLBACK_SIZE) {
        return;
    }

    for (int j = 0; j < VGA_COLS; j++) {
        terminal_history[line][j] = ' ' | (VGA_ATTR << 8);
    }

    while (*text && col < VGA_COLS) {
        terminal_history[line][col++] = *text++ | (VGA_ATTR << 8);
    }

    cursor_col = (uint8_t)col;
    vga_refresh();
}

static void render_loading_status(void)
{
    char line[80];
    int pos = 0;

    if (!loading_status_active || loading_status_line < 0) {
        return;
    }

    /* Animazione dots basata sul frame count */
    int dot_count = (loading_status_frame % 3) + 1;

    line[pos++] = '[';
    for (int i = 0; i < dot_count && pos < 79; i++) line[pos++] = '.';
    for (int i = dot_count; i < 3 && pos < 79; i++) line[pos++] = ' ';
    line[pos++] = ']';
    line[pos++] = ' ';

    /* Progress bar: 10 caratteri, si riempie in base a loading_status_progress */
    int bar_filled = (loading_status_progress * 10) / 100;
    line[pos++] = '[';
    for (int i = 0; i < 10 && pos < 79; i++) {
        if (i < bar_filled) {
            line[pos++] = '#';
        } else {
            line[pos++] = '.';
        }
    }
    line[pos++] = ']';
    line[pos++] = ' ';

    /* Percentuale numerica (3 cifre) */
    line[pos++] = ' ';
    line[pos++] = '0' + (loading_status_progress / 100) % 10;
    line[pos++] = '0' + (loading_status_progress / 10) % 10;
    line[pos++] = '0' + loading_status_progress % 10;
    line[pos++] = '%';
    line[pos++] = ' ';

    /* Label */
    for (int i = 0; loading_status_label[i] && pos < 79; i++) line[pos++] = loading_status_label[i];
    line[pos] = '\0';

    vga_replace_line_text(loading_status_line, line);
}

/* Start a fresh scrollback line for the input prompt (do not overwrite command output). */
static void vga_reserve_prompt_line(void)
{
    if (terminal_total_lines >= SCROLLBACK_SIZE) {
        for (int i = 0; i < SCROLLBACK_SIZE - 1; i++) {
            for (int j = 0; j < VGA_COLS; j++) {
                terminal_history[i][j] = terminal_history[i + 1][j];
            }
        }
        terminal_total_lines = SCROLLBACK_SIZE;
    } else {
        terminal_total_lines++;
    }

    for (int j = 0; j < VGA_COLS; j++) {
        terminal_history[terminal_total_lines - 1][j] = ' ' | (VGA_ATTR << 8);
    }

    cursor_col = 0;
    cursor_row = 0;
    scroll_view_offset = 0;
    vga_refresh();
}

static void vga_replace_current_input_line(const char *prompt, const char *input, int cursor_pos)
{
    int line = terminal_total_lines - 1;
    int col = 0;

    if (line < 0) {
        return;
    }

    cursor_row = 0;

    for (int j = 0; j < VGA_COLS; j++) {
        terminal_history[line][j] = ' ' | (VGA_ATTR << 8);
    }

    /* Write prompt */
    while (*prompt && col < VGA_COLS) {
        terminal_history[line][col++] = *prompt++ | (VGA_ATTR << 8);
    }

    int input_start = col;
    int input_len = kstrlen(input);

    /* Write input with cursor highlight */
    for (int i = 0; i < input_len && col < VGA_COLS; i++) {
        if (i == cursor_pos) {
            /* Show character at cursor with inverse video */
            terminal_history[line][col++] = input[i] | (0x70 << 8);
        } else {
            terminal_history[line][col++] = input[i] | (VGA_ATTR << 8);
        }
    }

    /* If cursor is at end of input, show an underscore cursor */
    if (cursor_pos >= input_len && col < VGA_COLS) {
        terminal_history[line][col++] = '_' | (0x70 << 8);
    }

    /* Set hardware cursor position to cursor location */
    cursor_col = (uint8_t)(input_start + cursor_pos);
    vga_refresh();
}

static int get_prompt_prefix_len(void)
{
    if (fs_edit_is_active()) {
        const char *ep = fs_edit_path();
        int len = 5;
        while (*ep) { len++; ep++; }
        len += 2;
        return len;
    }
    return 2;
}

static void show_text_prompt(void)
{
    if (fs_edit_is_active()) {
        char prefix[FS_MAX_PATH + 8];
        size_t p = 0;
        const char *tag = "edit:";
        while (*tag && p < sizeof(prefix) - 1) prefix[p++] = *tag++;
        const char *ep = fs_edit_path();
        while (*ep && p < sizeof(prefix) - 1) prefix[p++] = *ep++;
        if (p < sizeof(prefix) - 2) prefix[p++] = '>';
        prefix[p++] = ' ';
        prefix[p] = '\0';
        vga_replace_current_input_line(prefix, cmd, cmd_cursor);
    } else {
        vga_replace_current_input_line("> ", cmd, cmd_cursor);
    }
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

static void pseudo_console_log(const char *msg)
{
    append_gfx_log(msg);
}

static void text_console_log(const char *msg)
{
    printk("%s\n", msg);
}

static void show_help_main(void (*log_fn)(const char *))
{
    log_fn("Available commands:");
    log_fn("help                - Show this help");
    log_fn("help --os           - Show OS-related commands");
    log_fn("clear               - Clear the console");
    log_fn("reboot              - Restart the system");
    log_fn("shutdown            - Power off the system");
    log_fn("gfx                 - Enter graphics mode");
    log_fn("layout <code>       - Set keyboard layout");
    log_fn("                       us, uk, it, fr, de, es, pt");
    log_fn("                       no, dk, se, fi, nl, be");
    log_fn("                       pl, cz, sk, hu, ro, tr");
    log_fn("cmd.print(\"msg\")    - Print a message");
    log_fn("help --fs           - Show filesystem commands");
    log_fn("help --disk         - Show disk commands");
}

static void show_help_os(void (*log_fn)(const char *))
{
    log_fn("OS commands:");
    log_fn("os.version          - Show kernel version");
    log_fn("os.bootinfo         - Show boot information");
}

static void show_help_disk(void (*log_fn)(const char *))
{
    log_fn("Disk commands (on-disk OKFS via RAM disk / ATA):");
    log_fn("d:cd <path>         - Change working directory");
    log_fn("d:pwd               - Print working directory");
    log_fn("d:ls <path>         - List directory");
    log_fn("d:cat <path>        - Print file");
    log_fn("d:write <path> \"t\"  - Write file");
    log_fn("d:append <path> \"t\" - Append to file");
    log_fn("d:edit <path>       - Edit file with :save/:q");
    log_fn("d:mkdir <path>      - Create directory");
    log_fn("d:rm <path>         - Remove file or empty dir");
    log_fn("d:format            - Format disk (destroys data)");
}

static int execute_console_command(
    const char *command,
    void (*log_fn)(const char *),
    int graphics_console)
{
    if (streq(command, "help")) {
        show_help_main(log_fn);
    } else if (streq(command, "help --os")) {
        show_help_os(log_fn);
    } else if (streq(command, "help --disk")) {
        show_help_disk(log_fn);
    } else if (streq(command, "clear")) {
        if (graphics_console) {
            clear_gfx_log();
        } else {
            vga_clear_public();
            printk("openkernel Command Line\n");
            printk("Type 'help' for available commands.\n");
        }
    } else if (streq(command, "reboot")) {
        log_fn("Rebooting...");
        if (graphics_console) {
            render_pseudo_text_screen();
        }
        reboot_system();
    } else if (streq(command, "shutdown")) {
        log_fn("Shutting down...");
        if (graphics_console) {
            render_pseudo_text_screen();
        }
        shutdown_system();
    } else if (streq(command, "gfx")) {
        if (graphics_console) {
            pseudo_text_mode_active = 0;
            render_graphics_test_screen();
        } else if (display_enter_gfx()) {
            graphics_mode_active = 1;
            pseudo_text_mode_active = 0;
            render_graphics_test_screen();
        } else {
            log_fn("Failed to enter graphics mode");
        }
    } else if (kstrlen(command) >= 7 && command[0] == 'l' && command[1] == 'a' && command[2] == 'y' && command[3] == 'o' && command[4] == 'u' && command[5] == 't' && command[6] == ' ') {
        const char *code = command + 7;
        char layout_param[16];
        int lpi = 0;
        layout_param[lpi++] = 'l'; layout_param[lpi++] = 'a'; layout_param[lpi++] = 'y'; layout_param[lpi++] = 'o'; layout_param[lpi++] = 'u'; layout_param[lpi++] = 't';
        layout_param[lpi++] = '=';
        while (*code && lpi < 15) layout_param[lpi++] = *code++;
        layout_param[lpi] = '\0';
        keyboard_detect_layout(layout_param);
        log_fn("Layout changed");
    } else if (kstrlen(command) >= 9 && command[0] == 'c' && command[1] == 'm' && command[2] == 'd' && command[3] == '.' && command[4] == 'p' && command[5] == 'r' && command[6] == 'i' && command[7] == 'n' && command[8] == 't') {
        /* cmd.print(...) */
        if (kstrlen(command) == 9) {
            /* Just "cmd.print" with no parens */
            log_fn("Error: incomplete statement");
        } else {
            char msg[128];
            if (extract_parenthesis_content(command + 9, msg, sizeof(msg))) {
                if (msg[0] == '\0') {
                    log_fn("Error: incomplete statement");
                } else {
                    log_fn(msg);
                }
            } else {
                log_fn("Error: incomplete statement");
            }
        }
    } else if (streq(command, "os.version")) {
        log_fn(KERNEL_VERSION_STRING);
    } else if (streq(command, "os.bootinfo")) {
        log_fn("=== openkernel Boot Info ===");
        {
            /* Magic: 0xXXXXXXXX  Flags: 0xXXXXXXXX */
            char b[64];
            const char *hex = "0123456789abcdef";
            int bi = 0;
            int si;

            b[bi++] = 'M'; b[bi++] = 'a'; b[bi++] = 'g'; b[bi++] = 'i';
            b[bi++] = 'c'; b[bi++] = ':'; b[bi++] = ' ';
            b[bi++] = '0'; b[bi++] = 'x';
            for (si = 28; si >= 0; si -= 4) b[bi++] = hex[(saved_magic >> si) & 0xF];
            b[bi++] = ' ';
            b[bi++] = 'F'; b[bi++] = 'l'; b[bi++] = 'a'; b[bi++] = 'g';
            b[bi++] = 's'; b[bi++] = ':'; b[bi++] = ' ';
            b[bi++] = '0'; b[bi++] = 'x';
            for (si = 28; si >= 0; si -= 4) b[bi++] = hex[(saved_flags >> si) & 0xF];
            b[bi] = '\0';
            log_fn(b);
        }
        {
            /* Memory: X KB lower, Y KB upper (Z MB) */
            char b[64];
            int bi = 0;
            const char *m = "Memory: ";
            while (*m) b[bi++] = *m++;

            { unsigned long v = saved_mem_lower; char t[16]; int ti = 0;
              if (v == 0) { b[bi++] = '0'; }
              else { while (v > 0 && ti < 15) { t[ti++] = '0' + (v % 10); v /= 10; }
                     while (ti > 0) b[bi++] = t[--ti]; }
            }
            b[bi++] = ' '; b[bi++] = 'K'; b[bi++] = 'B';

            { const char *s = " lower, "; while (*s) b[bi++] = *s++; }

            { unsigned long v = saved_mem_upper; char t[16]; int ti = 0;
              if (v == 0) { b[bi++] = '0'; }
              else { while (v > 0 && ti < 15) { t[ti++] = '0' + (v % 10); v /= 10; }
                     while (ti > 0) b[bi++] = t[--ti]; }
            }
            b[bi++] = ' '; b[bi++] = 'K'; b[bi++] = 'B';

            { const char *s = " upper"; while (*s) b[bi++] = *s++; }

            b[bi] = '\0';
            log_fn(b);
        }
        if (saved_boot_loader_valid) {
            char b[64];
            int bi = 0;
            const char *lb = "Loader: ";
            while (*lb) b[bi++] = *lb++;
            for (int i = 0; saved_boot_loader[i]; i++) b[bi++] = saved_boot_loader[i];
            b[bi] = '\0';
            log_fn(b);
        }
    } else if (kstrlen(command) >= 7 && streq(command, "d:format")) {
        if (g_disk_fs.mounted) {
            log_fn("Formatting disk...");
            block_device_t *dev = g_disk_fs.dev;
            if (okfs_format(dev, 64) == OKFS_OK) {
                if (okfs_mount(&g_disk_fs, dev) == OKFS_OK) {
                    g_disk_cwd[0] = '/'; g_disk_cwd[1] = '\0';
                    log_fn("Disk formatted and remounted.");
                } else {
                    log_fn("Format OK but remount failed.");
                }
            } else {
                log_fn("Format failed.");
            }
        } else {
            log_fn("No disk filesystem mounted.");
        }
    } else if (kstrlen(command) >= 3 && command[0] == 'd' && command[1] == ':') {
        /* Disk command dispatcher */
        const char *sub = command + 2;
        char d_resolved[FS_MAX_PATH];

        if (kstrlen(sub) >= 2 && sub[0] == 'c' && sub[1] == 'd' && (sub[2] == '\0' || sub[2] == ' ')) {
            const char *dest = (sub[2] == ' ') ? sub + 3 : "/";
            while (*dest == ' ') dest++;
            char resolved[FS_MAX_PATH];
            disk_resolve_path(dest, resolved, sizeof(resolved));
            disk_normalize_path(resolved);
            /* Check it exists and is a directory */
            if (okfs_isdir(&g_disk_fs, resolved) != 0) {
                log_fn("Not found");
                goto skip_disk_cmd;
            }
            size_t ci = 0;
            while (resolved[ci] && ci < FS_MAX_PATH - 1) { g_disk_cwd[ci] = resolved[ci]; ci++; }
            g_disk_cwd[ci] = '\0';
            goto skip_disk_cmd;
        } else if (kstrlen(sub) >= 4 && sub[0] == 'p' && sub[1] == 'w' && sub[2] == 'd' && sub[3] == '\0') {
            log_fn(g_disk_cwd);
            goto skip_disk_cmd;
        } else if (kstrlen(sub) >= 4 && sub[0] == 'l' && sub[1] == 's' && sub[2] == ' ') {
            disk_resolve_path(sub + 3, d_resolved, sizeof(d_resolved));
            disk_normalize_path(d_resolved);
            okfs_list(&g_disk_fs, d_resolved, log_fn);
        } else if (kstrlen(sub) >= 5 && sub[0] == 'c' && sub[1] == 'a' && sub[2] == 't' && sub[3] == ' ') {
            disk_resolve_path(sub + 4, d_resolved, sizeof(d_resolved));
            okfs_cat(&g_disk_fs, d_resolved, log_fn);
        } else if (kstrlen(sub) >= 7 && sub[0] == 'm' && sub[1] == 'k' && sub[2] == 'd' && sub[3] == 'i' && sub[4] == 'r' && sub[5] == ' ') {
            disk_resolve_path(sub + 6, d_resolved, sizeof(d_resolved));
            disk_normalize_path(d_resolved);
            int err = okfs_create(&g_disk_fs, d_resolved, OKFS_TYPE_DIR);
            if (err >= 0) log_fn("Directory created.");
            else log_fn(okfs_strerror(err));
        } else if (kstrlen(sub) >= 3 && sub[0] == 'r' && sub[1] == 'm' && sub[2] == ' ') {
            disk_resolve_path(sub + 3, d_resolved, sizeof(d_resolved));
            disk_normalize_path(d_resolved);
            int err = okfs_delete(&g_disk_fs, d_resolved);
            if (err == OKFS_OK) log_fn("Removed.");
            else log_fn(okfs_strerror(err));
        } else if (kstrlen(sub) >= 5 && sub[0] == 'e' && sub[1] == 'd' && sub[2] == 'i' && sub[3] == 't' && sub[4] == ' ') {
            /* d:edit <path> — copy file to in-memory FS for editing */
            disk_resolve_path(sub + 5, d_resolved, sizeof(d_resolved));
            disk_normalize_path(d_resolved);
            uint32_t dsize = 0;
            uint8_t *dcontent = okfs_read(&g_disk_fs, d_resolved, &dsize);
            if (!dcontent) {
                log_fn("File not found on disk");
                goto skip_disk_cmd;
            }
            int err = fs_write_file(DISK_EDIT_TEMP, (const char *)dcontent, dsize);
            kfree(dcontent);
            if (err != FS_OK) {
                log_fn("Failed to stage file for editing");
                goto skip_disk_cmd;
            }
            err = fs_edit_begin(DISK_EDIT_TEMP);
            if (err != FS_OK) {
                log_fn("Failed to enter edit mode");
                fs_rm(DISK_EDIT_TEMP);
                goto skip_disk_cmd;
            }
            int pi = 0;
            while (d_resolved[pi] && pi < FS_MAX_PATH - 1) {
                g_disk_edit_path[pi] = d_resolved[pi];
                pi++;
            }
            g_disk_edit_path[pi] = '\0';
            g_disk_edit_active = 1;
            log_fn("Edit mode. :save writes back to disk.");
            fs_edit_display_content(log_fn);
        } else {
            /* Try write / append by checking for quoted content */
            const char *spc;
            int is_write = 0, is_append = 0;
            if (kstrlen(sub) >= 6 && sub[0] == 'w' && sub[1] == 'r' && sub[2] == 'i' && sub[3] == 't' && sub[4] == 'e' && sub[5] == ' ') {
                is_write = 1;
                spc = sub + 6;
            } else if (kstrlen(sub) >= 7 && sub[0] == 'a' && sub[1] == 'p' && sub[2] == 'p' && sub[3] == 'e' && sub[4] == 'n' && sub[5] == 'd' && sub[6] == ' ') {
                is_append = 1;
                spc = sub + 7;
            } else {
                log_fn("Unknown disk command. Try: d:ls, d:cat, d:write, d:append, d:edit, d:mkdir, d:rm, d:cd, d:pwd, d:format");
                log_fn("  d:write <path> \"text\"  - note: quotes required");
                goto skip_disk_cmd;
            }

            /* Skip spaces */
            while (*spc == ' ') spc++;
            /* Extract path (up to space or quote) */
            char dpath_raw[256];
            int pi = 0;
            if (*spc == '"') {
                log_fn("Usage: d:write <path> \"text\"  (path first, then quoted text)");
                goto skip_disk_cmd;
            }
            while (*spc && *spc != ' ' && *spc != '"' && pi < 254) dpath_raw[pi++] = *spc++;
            dpath_raw[pi] = '\0';
            /* Resolve path against cwd */
            char dpath[256];
            disk_resolve_path(dpath_raw, dpath, sizeof(dpath));
            disk_normalize_path(dpath);

            while (*spc == ' ') spc++;
            /* Extract quoted content */
            if (*spc != '"') {
                log_fn("Usage: d:write <path> \"text\"");
                goto skip_disk_cmd;
            }
            spc++; /* skip opening quote */
            char dcontent[512];
            int ci = 0;
            while (*spc && *spc != '"' && ci < 510) dcontent[ci++] = *spc++;
            dcontent[ci] = '\0';

            if (is_write) {
                int err = okfs_write(&g_disk_fs, dpath, (const uint8_t *)dcontent, ci);
                if (err == OKFS_OK) log_fn("Written.");
                else log_fn(okfs_strerror(err));
            } else {
                /* Append: read existing, combine, write back */
                uint32_t existing_size = 0;
                uint8_t *existing = okfs_read(&g_disk_fs, dpath, &existing_size);
                uint32_t new_len = existing_size + ci;
                uint8_t *new_data = (uint8_t *)kmalloc(new_len + 1);
                if (!new_data) { log_fn("Out of memory"); goto skip_disk_cmd; }
                if (existing) {
                    for (uint32_t i = 0; i < existing_size; i++) new_data[i] = existing[i];
                }
                for (uint32_t i = 0; i < ci; i++) new_data[existing_size + i] = (uint8_t)dcontent[i];
                new_data[new_len] = '\0';
                int err = okfs_write(&g_disk_fs, dpath, new_data, new_len);
                kfree(new_data);
                if (existing) kfree(existing);
                if (err == OKFS_OK) log_fn("Appended.");
                else log_fn(okfs_strerror(err));
            }
        }
skip_disk_cmd:
        ;
    } else if (kstrlen(command) > 0) {
        log_fn("Unknown command");
        log_fn("Type 'help' for available commands");
        return 0;
    }

    return 1;
}

static void pseudo_console_execute(void)
{
    pseudo_cmd[pseudo_cmd_len] = '\0';
    if (pseudo_cmd_len > 0) {
        append_gfx_prompt_line(pseudo_cmd);
        if (fs_edit_is_active()) {
            fs_edit_handle_line(pseudo_cmd, pseudo_console_log);
            /* If edit mode just ended and we were editing a disk file,
               read the result back and write to disk. */
            if (!fs_edit_is_active() && g_disk_edit_active) {
                g_disk_edit_active = 0;
                size_t sz = 0;
                const char *content = fs_read_file(DISK_EDIT_TEMP, &sz);
                if (content) {
                    int err = okfs_write(&g_disk_fs, g_disk_edit_path,
                                         (const uint8_t *)content, sz);
                    if (err == OKFS_OK)
                        pseudo_console_log("Synced to disk.");
                    else
                        pseudo_console_log(okfs_strerror(err));
                    kfree((void *)content);
                }
                fs_rm(DISK_EDIT_TEMP);
                g_disk_edit_path[0] = '\0';
            }
        } else if (!fs_handle_command(pseudo_cmd, pseudo_console_log)) {
            execute_console_command(pseudo_cmd, pseudo_console_log, 1);
        }
    }

    /* If ":e <n>" loaded a line, pre-fill buffer and skip clearing */
    {
        const char *pending = fs_edit_get_pending_line();
        if (pending) {
            int plen = 0;
            while (pending[plen] && plen < (int)sizeof(pseudo_cmd) - 2) {
                pseudo_cmd[plen] = pending[plen];
                plen++;
            }
            pseudo_cmd_len = plen;
            pseudo_cmd_cursor = plen;
            pseudo_cmd[plen] = '\0';
            return;
        }
    }

    pseudo_cmd_len = 0;
    pseudo_cmd_cursor = 0;
    pseudo_cmd[0] = '\0';
}

static void execute_text_command(const char *command)
{
    if (fs_edit_is_active()) {
        fs_edit_handle_line(command, text_console_log);
        /* If edit mode just ended and we were editing a disk file,
           read the result back and write to disk. */
        if (!fs_edit_is_active() && g_disk_edit_active) {
            g_disk_edit_active = 0;
            size_t sz = 0;
            const char *content = fs_read_file(DISK_EDIT_TEMP, &sz);
            if (content) {
                int err = okfs_write(&g_disk_fs, g_disk_edit_path,
                                     (const uint8_t *)content, sz);
                if (err == OKFS_OK)
                    text_console_log("Synced to disk.");
                else
                    text_console_log(okfs_strerror(err));
                kfree((void *)content);
            }
            fs_rm(DISK_EDIT_TEMP);
            g_disk_edit_path[0] = '\0';
        }
        return;
    }
    if (fs_handle_command(command, text_console_log)) {
        return;
    }
    execute_console_command(command, text_console_log, 0);
}

/* Kernel initialization */
void kernel_init(void)
{
    int load_frame = 0;

    vga_clear();

    /* Hide the default hardware cursor (keep only our custom rectangle cursor) */
    io_outb(0x3D4, 0x0A);
    io_outb(0x3D5, 0x20);

    printk(KERNEL_VERSION_STRING "\n");
    printk("Built: " __DATE__ " " __TIME__ "\n\n");

    loading_tick_inline("GDT", &load_frame);
    gdt_init();

    loading_tick_inline("IDT/PIC", &load_frame);
    idt_init();

    loading_tick_inline("ISRs", &load_frame);
    isr_install_all();

    loading_tick_inline("PIT Timer", &load_frame);
    timer_init(100);

    loading_tick_inline("PS/2 Keyboard", &load_frame);
    keyboard_init();
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
    memory_init(mbd->mem_lower, mbd->mem_upper);
    printk("  [OK]\n\n");

    printk("Initializing filesystem...\n");
    fs_init();
    printk("  [OK] OKFS ready\n\n");

    /* ---- Block device subsystem ---- */
    block_init();
    printk("Initializing block devices...\n");

    /* Try ATA (may fail if no drive present) */
    ata_init();

    /* Always set up a RAM disk for testing (1 MB = 2048 sectors) */
    ramdisk_init(2048);

    /* Mount OKFS on the RAM disk */
    {
        int err = okfs_mount(&g_disk_fs, block_find("ram0"));
        if (err == OKFS_ERR_BAD_MAGIC) {
            printk("  Formatting RAM disk...\n");
            if (okfs_format(block_find("ram0"), 64) == OKFS_OK) {
                if (okfs_mount(&g_disk_fs, block_find("ram0")) == OKFS_OK) {
                    printk("  [OK] Disk OKFS mounted on ram0\n");
                }
            }
        } else if (err == OKFS_OK) {
            printk("  [OK] Disk OKFS mounted on ram0\n");
        } else {
            printk("  [--] Failed to mount RAM disk: %s\n", okfs_strerror(err));
        }
    }

    /* Enable interrupts for preemptive multitasking */
    enable_interrupts();

    /* Initialize process management */
    printk("Initializing process management...\n");
    process_init();
    printk("  [OK]\n\n");

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

    /* Save boot info for os.bootinfo command */
    saved_magic = magic;
    saved_flags = mbd->flags;
    saved_mem_lower = mbd->mem_lower;
    saved_mem_upper = mbd->mem_upper;
    if (mbd->boot_loader_name) {
        const char *name = (const char *)mbd->boot_loader_name;
        int i = 0;
        while (name[i] && i < 63) {
            saved_boot_loader[i] = name[i];
            i++;
        }
        saved_boot_loader[i] = '\0';
        saved_boot_loader_valid = 1;
    } else {
        saved_boot_loader_valid = 0;
    }

    /* Transition to clean command-line interface */
    vga_clear_public();
    printk("openkernel Command Line\n");
    printk("Type 'help' for available commands.\n");

    show_text_prompt();

    /* Simple idle loop with Ctrl+T graphics toggle */
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
                    /* Text mode - handle commands with cursor support */
                    if (sc == 0x1C) { /* Enter - execute command */
                        printk("\n");
                        cmd[cmd_len] = '\0';
                        int loaded_pending = 0;
                        if (cmd_len > 0) {
                            int was_edit = (cmd_len >= 5 && cmd[0] == 'e' && cmd[1] == 'd' && cmd[2] == 'i' && cmd[3] == 't' && cmd[4] == ' ');
                            execute_text_command(cmd);
                            /* If ":e <n>" loaded a line, pre-fill cmd buffer */
                            {
                                const char *pending = fs_edit_get_pending_line();
                                if (pending) {
                                    int plen = 0;
                                    while (pending[plen] && plen < (int)sizeof(cmd) - 2) {
                                        cmd[plen] = pending[plen];
                                        plen++;
                                    }
                                    cmd_len = plen;
                                    cmd_cursor = plen;
                                    cmd[plen] = '\0';
                                    loaded_pending = 1;
                                }
                            }
                            if (!loaded_pending && was_edit && fs_edit_is_active()) {
                                fs_edit_display_content(text_console_log);
                            }
                        }
                        if (!loaded_pending) {
                            cmd_len = 0;
                            cmd[0] = '\0';
                            cmd_cursor = 0;
                            vga_reserve_prompt_line();
                            show_text_prompt();
                        } else {
                            show_text_prompt();
                        }
                    } else if (sc == 0x0E) { /* Backspace */
                        if (cmd_cursor > 0) {
                            for (int i = cmd_cursor - 1; i < cmd_len - 1; i++)
                                cmd[i] = cmd[i+1];
                            cmd_len--;
                            cmd_cursor--;
                            cmd[cmd_len] = '\0';
                        }
                        show_text_prompt();
                    } else {
                        char ch = keyboard_scancode_to_ascii(sc);
                        if (ch >= 32 && ch <= 126) {
                            int avail = VGA_COLS - get_prompt_prefix_len();
                            if (avail <= 0) avail = 1;

                            if (fs_edit_is_active() && cmd_len >= avail && cmd_len < (int)sizeof(cmd) - 1) {
                                int break_pos = -1;
                                for (int i = avail - 1; i >= 0; i--) {
                                    if (cmd[i] == ' ') { break_pos = i; break; }
                                }
                                if (break_pos <= 0) break_pos = avail - 1;

                                int saved_len = cmd_len - break_pos - 1;

                                cmd[break_pos] = '\0';
                                printk("\n");
                                execute_text_command(cmd);

                                cmd_len = 0;
                                if (saved_len > 0) {
                                    for (int i = 0; i < saved_len && i < (int)sizeof(cmd) - 2; i++)
                                        cmd[i] = cmd[break_pos + 1 + i];
                                    cmd_len = saved_len;
                                }
                                cmd[cmd_len] = ch;
                                cmd_len++;
                                cmd[cmd_len] = '\0';
                                cmd_cursor = cmd_len;
                                show_text_prompt();
                            } else if (cmd_len < (int)sizeof(cmd) - 1) {
                                for (int i = cmd_len; i > cmd_cursor; i--)
                                    cmd[i] = cmd[i-1];
                                cmd[cmd_cursor] = ch;
                                cmd_len++;
                                cmd_cursor++;
                                cmd[cmd_len] = '\0';
                                show_text_prompt();
                            }
                        }
                    }
                } else if (pseudo_text_mode_active) {
                    int gfx_redraw = 0; /* 0=none, 1=prompt only, 2=full screen */
                    (void)key;
                    if (sc == 0x1C) { /* Enter */
                        int had_input = pseudo_cmd_len > 0;
                        pseudo_console_execute();
                        pseudo_cmd_cursor = 0;
                        gfx_redraw = had_input ? 2 : 1;
                    } else if (sc == 0x0E) { /* Backspace */
                        if (pseudo_cmd_cursor > 0) {
                            for (int i = pseudo_cmd_cursor - 1; i < pseudo_cmd_len - 1; i++)
                                pseudo_cmd[i] = pseudo_cmd[i+1];
                            pseudo_cmd_len--;
                            pseudo_cmd_cursor--;
                            pseudo_cmd[pseudo_cmd_len] = '\0';
                            gfx_redraw = 1;
                        }
                    } else {
                        char ch = keyboard_scancode_to_ascii(sc);
                        if (ch >= 32 && ch <= 126 && pseudo_cmd_len < (int)sizeof(pseudo_cmd) - 1) {
                            /* Insert at cursor position */
                            for (int i = pseudo_cmd_len; i > pseudo_cmd_cursor; i--)
                                pseudo_cmd[i] = pseudo_cmd[i-1];
                            pseudo_cmd[pseudo_cmd_cursor] = ch;
                            pseudo_cmd_len++;
                            pseudo_cmd_cursor++;
                            pseudo_cmd[pseudo_cmd_len] = '\0';
                            gfx_redraw = 1;
                        }
                    }
                    if (gfx_redraw == 2) {
                        render_pseudo_text_screen();
                    } else if (gfx_redraw == 1) {
                        render_pseudo_text_prompt();
                    }
                } else {
                    /* Handle regular graphics mode input */
                    if (sc == 0x1C) { /* Enter - switch to pseudo text mode */
                        pseudo_text_mode_active = 1;
                        pseudo_cmd_len = 0;
                        pseudo_cmd_cursor = 0;
                        pseudo_cmd[0] = '\0';
                        render_pseudo_text_screen();
                    } else if (sc == 0x0E) { /* Backspace */
                        if (pseudo_cmd_cursor > 0) {
                            for (int i = pseudo_cmd_cursor - 1; i < pseudo_cmd_len - 1; i++)
                                pseudo_cmd[i] = pseudo_cmd[i+1];
                            pseudo_cmd_len--;
                            pseudo_cmd_cursor--;
                            pseudo_cmd[pseudo_cmd_len] = '\0';
                        }
                    } else {
                        char ch = keyboard_scancode_to_ascii(sc);
                        if (ch >= 32 && ch <= 126 && pseudo_cmd_len < (int)sizeof(pseudo_cmd) - 1) {
                            /* Insert at cursor position */
                            for (int i = pseudo_cmd_len; i > pseudo_cmd_cursor; i--)
                                pseudo_cmd[i] = pseudo_cmd[i-1];
                            pseudo_cmd[pseudo_cmd_cursor] = ch;
                            pseudo_cmd_len++;
                            pseudo_cmd_cursor++;
                            pseudo_cmd[pseudo_cmd_len] = '\0';
                        }
                    }
                }
            }

            /* Handle arrow key requests after all scancodes processed */
            if (arrow_up_requested) {
                arrow_up_requested = 0;
                if (!graphics_mode_active) {
                    vga_scroll(1);
                }
            }
            if (arrow_down_requested) {
                arrow_down_requested = 0;
                if (!graphics_mode_active) {
                    vga_scroll(-1);
                }
            }
            if (arrow_left_requested) {
                arrow_left_requested = 0;
                if (!graphics_mode_active) {
                    if (cmd_cursor > 0) { cmd_cursor--; show_text_prompt(); }
                } else if (pseudo_text_mode_active) {
                    if (pseudo_cmd_cursor > 0) {
                        pseudo_cmd_cursor--;
                        render_pseudo_text_prompt();
                    }
                }
            }
            if (arrow_right_requested) {
                arrow_right_requested = 0;
                if (!graphics_mode_active) {
                    if (cmd_cursor < cmd_len) { cmd_cursor++; show_text_prompt(); }
                } else if (pseudo_text_mode_active) {
                    if (pseudo_cmd_cursor < pseudo_cmd_len) {
                        pseudo_cmd_cursor++;
                        render_pseudo_text_prompt();
                    }
                }
            }
            if (delete_requested) {
                delete_requested = 0;
                if (!graphics_mode_active) {
                    if (cmd_cursor < cmd_len) {
                        for (int i = cmd_cursor; i < cmd_len - 1; i++)
                            cmd[i] = cmd[i+1];
                        cmd_len--;
                        cmd[cmd_len] = '\0';
                        show_text_prompt();
                    }
                } else if (pseudo_text_mode_active) {
                    if (pseudo_cmd_cursor < pseudo_cmd_len) {
                        for (int i = pseudo_cmd_cursor; i < pseudo_cmd_len - 1; i++)
                            pseudo_cmd[i] = pseudo_cmd[i+1];
                        pseudo_cmd_len--;
                        pseudo_cmd[pseudo_cmd_len] = '\0';
                        render_pseudo_text_prompt();
                    }
                }
            }
        }

        asm volatile ("hlt");
    }
}

/* Halt the CPU */
void halt(void)
{
    asm volatile ("cli\n1:\nhlt\njmp 1b\n");
}

