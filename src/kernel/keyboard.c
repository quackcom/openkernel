/* openkernel - PS/2 Keyboard Implementation */
/* Uses PS/2 port 0x60 with IRQ 1 */

#include "keyboard.h"
#include "idt.h"
#include "kernel.h"

/* Forward declarations for VGA functions */
extern void vga_clear_public(void);
extern void vga_scroll(int delta);

/* Rename vga_clear to avoid conflict */
#define vga_clear vga_clear_public

/* Graphics mode toggle flag - set by Ctrl+T in IRQ, consumed in main loop */
volatile int graphics_toggle_requested = 0;

/* Externals from io.asm (formerly keyboard.asm) */
extern uint8_t io_inb(uint16_t port);
extern void io_outb(uint16_t port, uint8_t data);

/* PS/2 keyboard port */
#define KEYBOARD_DATA_PORT  0x60
#define KEYBOARD_STATUS_PORT 0x64


/* Scancode buffer */
#define KEYBUF_SIZE 64
static volatile uint8_t key_buffer[KEYBUF_SIZE];
static volatile int keybuf_head = 0;
static volatile int keybuf_tail = 0;

/* Keyboard modifiers */
static volatile uint8_t modifiers = 0;

static volatile int extended_mode = 0;

/* Debug counter for keyboard interrupts */
static volatile int keyboard_debug_counter = 0;

/* Keyboard Layouts */
typedef enum {
    LAYOUT_US,
    LAYOUT_UK,
    LAYOUT_IT
} layout_t;

static layout_t current_layout = LAYOUT_US;

/* US QWERTY scancode set 1 -> ASCII */
static const char scancode_ascii_lower_us[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', /* 0-9 */
    '9', '0', '-', '=', 0,   0,   'q', 'w', 'e', 'r', /* 10-19 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',0,    /* 20-29 */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 30-39 */
    '\'','`', 0,   '\\','z', 'x', 'c', 'v', 'b', 'n', /* 40-49 */
    'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_us[] = {
    0,   0,   '!', '@', '#', '$', '%', '^', '&', '*', /* 0-9 */
    '(', ')', '_', '+', 0,   0,   'Q', 'W', 'E', 'R', /* 10-19 */
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',0,    /* 20-29 */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', /* 30-39 */
    '"', '~', 0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', /* 40-49 */
    'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* UK QWERTY scancode set 1 -> ASCII */
static const char scancode_ascii_lower_uk[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', /* 0-9 */
    '9', '0', '-', '=', 0,   0,   'q', 'w', 'e', 'r', /* 10-19 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',0,    /* 20-29 */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 30-39 */
    '\'','`', 0,   '#', 'z', 'x', 'c', 'v', 'b', 'n', /* 40-49 */
    'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_uk[] = {
    0,   0,   '!', '"', 163, '$', '%', '^', '&', '*', /* 0-9 (163 = £) */
    '(', ')', '_', '+', 0,   0,   'Q', 'W', 'E', 'R', /* 10-19 */
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',0,    /* 20-29 */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', /* 30-39 */
    '@', '~', 0,   '~', 'Z', 'X', 'C', 'V', 'B', 'N', /* 40-49 */
    'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* Italian QWERTY scancode set 1 -> ASCII */
static const char scancode_ascii_lower_it[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', /* 0-9 */
    '9', '0', '\'', 'i', 0,   0,   'q', 'w', 'e', 'r', /* 10-19 */
    't', 'y', 'u', 'i', 'o', 'p', 'e', '+', '\n',0,    /* 20-29 */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'o', /* 30-39 */
    'a', '\\', 0,  'u', 'z', 'x', 'c', 'v', 'b', 'n', /* 40-49 */
    'm', ',', '.', '-', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_it[] = {
    0,   0,   '!', '"', '#', '$', '%', '&', '/', '(', /* 0-9 */
    ')', '=', '?', '^', 0,   0,   'Q', 'W', 'E', 'R', /* 10-19 */
    'T', 'Y', 'U', 'I', 'O', 'P', 'E', '*', '\n',0,    /* 20-29 */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 'C', /* 30-39 */
    'O', '|', 0,   'S', 'Z', 'X', 'C', 'V', 'B', 'N', /* 40-49 */
    'M', ';', ':', '_', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

void keyboard_set_layout(int layout) {
    if (layout >= 0 && layout <= 2) current_layout = (layout_t)layout;
}

/* Helper for basic string comparison since we don't have string.h yet */
static int kstrcmp_prefix(const char* str, const char* prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++;
        prefix++;
    }
    return 1;
}

void keyboard_detect_layout(const char* cmdline) {
    if (!cmdline) return;
    const char* p = cmdline;
    while (*p) {
        if (kstrcmp_prefix(p, "layout=")) {
            p += 7;
            if (kstrcmp_prefix(p, "it")) keyboard_set_layout(LAYOUT_IT);
            else if (kstrcmp_prefix(p, "uk")) keyboard_set_layout(LAYOUT_UK);
            else if (kstrcmp_prefix(p, "us")) keyboard_set_layout(LAYOUT_US);
            return;
        }
        p++;
    }
}

/* Read a byte from keyboard data port */
static inline uint8_t keyboard_read(void)
{
    return io_inb(KEYBOARD_DATA_PORT);
}

/* Keyboard IRQ handler */
static void keyboard_irq_handler(void)
{
    uint8_t scancode = keyboard_read();
    keyboard_debug_counter++;

    /* DEBUG: Flash a character to prove the hardware triggered an interrupt */
    // printk("!"); 

    /* Handle Extended Scancode prefix (used by dedicated laptop arrow keys) */

    if (scancode == 0xE0) {
        extended_mode = 1;
        return;
    }

    /* 1. Track modifier keys first so we can check them for navigation */
    switch (scancode) {
        case 0x2A: modifiers |= KEYBOARD_LSHIFT; break;
        case 0xAA: modifiers &= ~KEYBOARD_LSHIFT; break;
        case 0x36: modifiers |= KEYBOARD_RSHIFT; break;
        case 0xB6: modifiers &= ~KEYBOARD_RSHIFT; break;
        case 0x38: modifiers |= KEYBOARD_LALT; break;
        case 0xB8: modifiers &= ~KEYBOARD_LALT; break;
        case 0x1D: modifiers |= KEYBOARD_LCTRL; break;
        case 0x9D: modifiers &= ~KEYBOARD_LCTRL; break;
        /* Right Ctrl arrives as extended E0 1D / E0 9D and is tracked via 0x1D/0x9D. */
        case 0x3A: /* Caps lock toggle on press */
            modifiers ^= KEYBOARD_CAPS_LOCK;
            break;
    }
    
    /* 2. Handle Extended arrow keys for scrolling (text mode only) */
    extern int graphics_mode_active;
    if (extended_mode) {
        extended_mode = 0;
        if (!graphics_mode_active) {
            if (scancode == 0x48) { vga_scroll(1); return; }  /* Up Arrow */
            if (scancode == 0x50) { vga_scroll(-1); return; } /* Down Arrow */
        }
        return; /* Ignore other extended keys */
    }

    /* 3. Handle Ctrl+T to toggle graphics test mode (Debounced) */
    static int t_key_pressed = 0;
    if (scancode == 0x14) { /* 'T' pressed */
        if (!t_key_pressed && (modifiers & (KEYBOARD_LCTRL | KEYBOARD_RCTRL))) {
            graphics_toggle_requested = 1;
        }
        t_key_pressed = 1;
        return;
    } else if (scancode == 0x94) { /* 'T' released */
        t_key_pressed = 0;
        return;
    }

    /* 4. Buffer regular key press (ignore releases) */
    if (!(scancode & 0x80)) {
        int next = (keybuf_head + 1) % KEYBUF_SIZE;
        if (next != keybuf_tail) {
            key_buffer[keybuf_head] = scancode;
            keybuf_head = next;
        }
    }
}

/* Initialize keyboard driver */
void keyboard_init(void)
{
    uint8_t ccb;
    int timeout;

    printk("Initializing PS/2 keyboard...\n");

    /* 1. Drain any existing data from the keyboard */
    timeout = 1000;
    while (timeout-- && (io_inb(KEYBOARD_STATUS_PORT) & 0x01)) {
        io_inb(KEYBOARD_DATA_PORT);
    }

    /* 2. Enable the first PS/2 port */
    io_outb(KEYBOARD_STATUS_PORT, 0xAE);

    /* 3. Configure Controller Command Byte (CCB) to enable interrupts on port 1 */
    /* Wait for controller to be ready */
    timeout = 5000;
    while (timeout-- && (io_inb(KEYBOARD_STATUS_PORT) & 0x02));

    /* Read current CCB */
    io_outb(KEYBOARD_STATUS_PORT, 0x20);
    timeout = 5000;
    while (timeout-- && !(io_inb(KEYBOARD_STATUS_PORT) & 0x01));
    ccb = io_inb(KEYBOARD_DATA_PORT);

    /* 
     * CCB Bits:
     *   bit 0: First PS/2 port interrupt (1 = enabled)
     *   bit 1: Second PS/2 port interrupt (1 = enabled)
     *   bit 6: First PS/2 port translation (1 = scancode set 2 -> set 1)
     * 
     * We want: IRQ enabled on port 1, translation on
     */
    ccb |= 0x01;   /* Enable first port interrupt */
    ccb |= 0x40;   /* Enable translation (Set 2 → Set 1 scancodes) */
    ccb &= ~0x02;  /* Disable second port interrupt */

    /* Write updated CCB */
    timeout = 5000;
    while (timeout-- && (io_inb(KEYBOARD_STATUS_PORT) & 0x02));
    io_outb(KEYBOARD_STATUS_PORT, 0x60);
    timeout = 5000;
    while (timeout-- && (io_inb(KEYBOARD_STATUS_PORT) & 0x02));
    io_outb(KEYBOARD_DATA_PORT, ccb);

    printk("  [KB] CCB configured (0x%x)\n", ccb);

    /* 4. Enable keyboard scanning (0xF4) */
    timeout = 5000;
    while (timeout-- && (io_inb(KEYBOARD_STATUS_PORT) & 0x02));
    io_outb(KEYBOARD_DATA_PORT, 0xF4);

    /* Wait for ACK (0xFA) - no need to be overly precise */
    timeout = 10000;
    while (timeout-- && !(io_inb(KEYBOARD_STATUS_PORT) & 0x01));
    if (timeout > 0) {
        uint8_t ack = io_inb(KEYBOARD_DATA_PORT);
        if (ack == 0xFA) {
            printk("  [KB] Scanning enabled (ACK OK)\n");
        } else {
            printk("  [KB] Responded with 0x%x (continuing)\n", ack);
        }
    } else {
        printk("  [KB] No ACK from keyboard (continuing anyway)\n");
    }

    /* 5. Drain any remaining self-test/bat data */
    timeout = 5000;
    while (timeout-- && (io_inb(KEYBOARD_STATUS_PORT) & 0x01)) {
        io_inb(KEYBOARD_DATA_PORT);
    }

    /* Register IRQ handler */
    idt_register_handler(1, keyboard_irq_handler);

    /* Clear buffer and state */
    keybuf_head = 0;
    keybuf_tail = 0;
    modifiers = 0;

    printk("  [OK] Keyboard interrupts registered\n");
}

/* Get current keyboard modifiers */
uint8_t keyboard_get_modifiers(void)
{
    return modifiers;
}

/* Read last scancode (non-blocking) */
uint8_t keyboard_get_scancode(void)
{
    /* Check buffer first */
    if (keybuf_tail != keybuf_head) {
        uint8_t sc = key_buffer[keybuf_tail];
        keybuf_tail = (keybuf_tail + 1) % KEYBUF_SIZE;
        return sc;
    }
    return 0;
}

/* Convert scancode to ASCII character */
char keyboard_scancode_to_ascii(uint8_t scancode)
{
    /* Only handle key presses (bit 7 clear) */
    if (scancode & 0x80) return 0;

    const char* lower_ptr;
    const char* upper_ptr;

    if (current_layout == LAYOUT_US) {
        lower_ptr = scancode_ascii_lower_us;
        upper_ptr = scancode_ascii_upper_us;
    } else if (current_layout == LAYOUT_UK) {
        lower_ptr = scancode_ascii_lower_uk;
        upper_ptr = scancode_ascii_upper_uk;
    } else {
        lower_ptr = scancode_ascii_lower_it;
        upper_ptr = scancode_ascii_upper_it;
    }

    /* Scancodes 0x00 to 0x58 cover the standard keys in our tables */
    if (scancode < 0x59) {
        char c = lower_ptr[scancode];
        if (c == 0) return 0;

        /* Check shift state */
        int shifted = (modifiers & (KEYBOARD_LSHIFT | KEYBOARD_RSHIFT)) != 0;
        int caps = (modifiers & KEYBOARD_CAPS_LOCK) != 0;

        if (shifted || caps) {
            char upper = upper_ptr[scancode];
            if (upper && (shifted || (caps && c >= 'a' && c <= 'z'))) {
                return upper;
            }
        }
        return c;
    }
    return 0;
}
