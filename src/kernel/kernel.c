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

static void vga_replace_line_text(int line, const char *text);
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

static void render_pseudo_text_screen(void)
{
    const display_mode_t *mode = display_get_mode();
    display_clear(display_rgb(0, 0, 0x40));
    display_puts("openkernel Command Line", 20, 20, display_rgb(0xFF, 0xFF, 0xFF));
    display_puts("Type 'help' for available commands.", 20, 40, display_rgb(0xA0, 0xA0, 0xA0));

    int lines = (gfx_log_count < GFX_LOG_LINES) ? gfx_log_count : GFX_LOG_LINES;
    int start = gfx_log_count - lines;
    for (int i = 0; i < lines; i++) {
        int idx = (start + i) % GFX_LOG_LINES;
        display_puts(gfx_log[idx], 20, 64 + (uint32_t)i * 18, display_rgb(0xFF, 0xFF, 0xFF));
    }

    /* Ensure prompt line is fully clean before drawing */
    display_fill_rect(0, 558, mode->width, 20, display_rgb(0, 0, 0x40));

    /* Build prompt with cursor marker at the correct position */
    char prompt[80];
    const char *prefix = "> ";
    int p = 0;
    while (*prefix && p < 79) prompt[p++] = *prefix++;

    for (int i = 0; i < pseudo_cmd_len && p < 78; i++) {
        if (i == pseudo_cmd_cursor) {
            prompt[p++] = '_'; /* cursor marker */
        }
        prompt[p++] = pseudo_cmd[i];
    }
    if (pseudo_cmd_cursor >= pseudo_cmd_len && p < 79) {
        prompt[p++] = '_'; /* cursor at end */
    }
    prompt[p] = '\0';

    display_puts(prompt, 20, 560, display_rgb(0xFF, 0xFF, 0x80));
    (void)mode;
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

static void vga_replace_current_input_line(const char *prompt, const char *input, int cursor_pos)
{
    int line = terminal_total_lines - 1;
    int col = 0;

    if (line < 0) {
        return;
    }

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

static void show_text_prompt(void)
{
    vga_replace_current_input_line("> ", cmd, cmd_cursor);
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

static int execute_console_command(
    const char *command,
    void (*log_fn)(const char *),
    int graphics_console)
{
    if (streq(command, "help")) {
        log_fn("Available commands:");
        log_fn("help                - Show this help");
        log_fn("clear               - Clear the console");
        log_fn("reboot              - Restart the system");
        log_fn("shutdown            - Power off the system");
        log_fn("gfx                 - Enter graphics mode");
        log_fn("layout us           - Set US keyboard layout");
        log_fn("layout uk           - Set UK keyboard layout");
        log_fn("layout it           - Set Italian keyboard layout");
        log_fn("cmd.print(\"msg\")    - Print a message");
        log_fn("os.bootinfo         - Show boot information");
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
    } else if (streq(command, "layout us")) {
        keyboard_detect_layout("layout=us");
        log_fn("Keyboard layout set to US");
    } else if (streq(command, "layout uk")) {
        keyboard_detect_layout("layout=uk");
        log_fn("Keyboard layout set to UK");
    } else if (streq(command, "layout it")) {
        keyboard_detect_layout("layout=it");
        log_fn("Keyboard layout set to IT");
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
        execute_console_command(pseudo_cmd, pseudo_console_log, 1);
    }

    pseudo_cmd_len = 0;
    pseudo_cmd_cursor = 0;
    pseudo_cmd[0] = '\0';
}

static void execute_text_command(const char *command)
{
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

    printk("openkernel Kernel v0.2\n");
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
                        if (cmd_len > 0) {
                            execute_text_command(cmd);
                        }
                        cmd_len = 0;
                        cmd[0] = '\0';
                        cmd_cursor = 0;
                        show_text_prompt();
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
                        if (ch >= 32 && ch <= 126 && cmd_len < (int)sizeof(cmd) - 1) {
                            /* Insert at cursor position */
                            for (int i = cmd_len; i > cmd_cursor; i--)
                                cmd[i] = cmd[i-1];
                            cmd[cmd_cursor] = ch;
                            cmd_len++;
                            cmd_cursor++;
                            cmd[cmd_len] = '\0';
                            show_text_prompt();
                        }
                    }
                } else if (pseudo_text_mode_active) {
                    (void)key;
                    if (sc == 0x1C) { /* Enter */
                        pseudo_console_execute();
                        pseudo_cmd_cursor = 0;
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
                    render_pseudo_text_screen();
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
                        render_pseudo_text_screen();
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
                            render_pseudo_text_screen();
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
                    if (pseudo_cmd_cursor > 0) { pseudo_cmd_cursor--; render_pseudo_text_screen(); }
                }
            }
            if (arrow_right_requested) {
                arrow_right_requested = 0;
                if (!graphics_mode_active) {
                    if (cmd_cursor < cmd_len) { cmd_cursor++; show_text_prompt(); }
                } else if (pseudo_text_mode_active) {
                    if (pseudo_cmd_cursor < pseudo_cmd_len) { pseudo_cmd_cursor++; render_pseudo_text_screen(); }
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
                        render_pseudo_text_screen();
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

