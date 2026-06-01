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

/* Arrow key request flags - set by IRQ, consumed in main loop */
volatile int arrow_left_requested = 0;
volatile int arrow_right_requested = 0;
volatile int arrow_up_requested = 0;
volatile int arrow_down_requested = 0;
volatile int delete_requested = 0;

/* Keyboard Layouts */
typedef enum {
    LAYOUT_US,  /* 0 - United States (QWERTY) */
    LAYOUT_UK,  /* 1 - United Kingdom (QWERTY) */
    LAYOUT_IT,  /* 2 - Italian (QWERTY) */
    LAYOUT_FR,  /* 3 - French (AZERTY) */
    LAYOUT_DE,  /* 4 - German (QWERTZ) */
    LAYOUT_ES,  /* 5 - Spanish (QWERTY) */
    LAYOUT_PT,  /* 6 - Portuguese (QWERTY) */
    LAYOUT_NO,  /* 7 - Norwegian (QWERTY) */
    LAYOUT_DK,  /* 8 - Danish (QWERTY) */
    LAYOUT_SE,  /* 9 - Swedish (QWERTY) */
    LAYOUT_FI,  /* 10 - Finnish (QWERTY) */
    LAYOUT_NL,  /* 11 - Dutch (QWERTY) */
    LAYOUT_BE,  /* 12 - Belgian (AZERTY) */
    LAYOUT_PL,  /* 13 - Polish (QWERTY) */
    LAYOUT_CZ,  /* 14 - Czech (QWERTZ) */
    LAYOUT_SK,  /* 15 - Slovak (QWERTZ) */
    LAYOUT_HU,  /* 16 - Hungarian (QWERTZ) */
    LAYOUT_RO,  /* 17 - Romanian (QWERTY) */
    LAYOUT_TR,  /* 18 - Turkish (QWERTY) */
} layout_t;

static layout_t current_layout = LAYOUT_IT;

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

/* French AZERTY scancode set 1 -> ASCII */
static const char scancode_ascii_lower_fr[] = {
    0,   0,   '&', 130, 34,  39,  '(',  '-', 138, 95,  /* 0-9 (130=é, 138=è, 95=_) */
    135, 133, ')', '=', 0,   0,   'a', 'z', 'e', 'r',  /* 10-19 (135=ç, 133=à) */
    't', 'y', 'u', 'i', 'o', 'p', '^', '$', '\n',0,    /* 20-29 */
    'q', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm',  /* 30-39 */
    151, '^', 0,   '*', 'w', 'x', 'c', 'v', 'b', 'n',  /* 40-49 (151=ù) */
    ',', ';', ':', '!', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_fr[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', 176, '+', 0,   0,   'A', 'Z', 'E', 'R',  /* 10-19 (176=°) */
    'T', 'Y', 'U', 'I', 'O', 'P', 168, 163, '\n',0,     /* 20-29 (168=¨/¿) */
    'Q', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 'M',  /* 30-39 */
    151, 168, 0,   0,   'W', 'X', 'C', 'V', 'B', 'N',  /* 40-49 */
    '?', '.', '/', '#', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* German QWERTZ scancode set 1 -> ASCII */
static const char scancode_ascii_lower_de[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', 225, '\'',0,   0,   'q', 'w', 'e', 'r',  /* 10-19 (225=ß) */
    't', 'z', 'u', 'i', 'o', 'p', 129, '+', '\n',0,    /* 20-29 (129=ü) */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 148,  /* 30-39 (148=ö) */
    132, '^', 0,   '#', 'y', 'x', 'c', 'v', 'b', 'n',  /* 40-49 (132=ä) */
    'm', ',', '.', '-', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_de[] = {
    0,   0,   '!', '"', '#', '$', '%', '&', '/', '(',  /* 0-9 */
    ')', '=', '?', '`', 0,   0,   'Q', 'W', 'E', 'R',  /* 10-19 */
    'T', 'Z', 'U', 'I', 'O', 'P', 154, '*', '\n',0,    /* 20-29 (154=Ü) */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 153,  /* 30-39 (153=Ö) */
    142, '^', 0,   '\'', 'Y', 'X', 'C', 'V', 'B', 'N', /* 40-49 (142=Ä) */
    'M', ';', ':', '_', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* Spanish QWERTY scancode set 1 -> ASCII */
static const char scancode_ascii_lower_es[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', '\'',173, 0,   0,   'q', 'w', 'e', 'r',  /* 10-19 (¡=173) */
    't', 'y', 'u', 'i', 'o', 'p', '`', '+', '\n',0,    /* 20-29 */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 164,  /* 30-39 (164=ñ) */
    160, 176, 0,   135, 'z', 'x', 'c', 'v', 'b', 'n',  /* 40-49 (160=á, 176=°) */
    'm', ',', '.', '-', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_es[] = {
    0,   0,   '!', '"', 167, '$', '%', '&', '/', '(',  /* 0-9 (167=º) */
    ')', '=', '?', 168, 0,   0,   'Q', 'W', 'E', 'R',  /* 10-19 (168=¿) */
    'T', 'Y', 'U', 'I', 'O', 'P', '^', '*', '\n',0,    /* 20-29 */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 165,  /* 30-39 (165=Ñ) */
    162, 167, 0,   128, 'Z', 'X', 'C', 'V', 'B', 'N',  /* 40-49 (162=ó) */
    'M', ';', ':', '_', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* Portuguese QWERTY scancode set 1 -> ASCII */
static const char scancode_ascii_lower_pt[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', '\'',174, 0,   0,   'q', 'w', 'e', 'r',  /* 10-19 (171=«) */
    't', 'y', 'u', 'i', 'o', 'p', '+', '\'', '\n',0,   /* 20-29 */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 135,  /* 30-39 (135=ç) */
    176, 170, 0,  '~', 'z', 'x', 'c', 'v', 'b', 'n',   /* 40-49 (176=°, 170=¬) */
    'm', ',', '.', '-', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_pt[] = {
    0,   0,   '!', '"', '#', '$', '%', '&', '/', '(',  /* 0-9 */
    ')', '=', '?', 175, 0,   0,   'Q', 'W', 'E', 'R',  /* 10-19 (175=») */
    'T', 'Y', 'U', 'I', 'O', 'P', '*', '?', '\n',0,    /* 20-29 */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 135,  /* 30-39 */
    '^', 167, 0,   '^', 'Z', 'X', 'C', 'V', 'B', 'N',  /* 40-49 (167=º) */
    'M', ';', ':', '_', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* Norwegian QWERTY scancode set 1 -> ASCII */
static const char scancode_ascii_lower_no[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', '+', '\\',0,   0,   'q', 'w', 'e', 'r',  /* 10-19 */
    't', 'y', 'u', 'i', 'o', 'p', 134, '^', '\n',0,    /* 20-29 (134=å) */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 148,  /* 30-39 (148=ö) */
    132, '\'', 0,  '*', 'z', 'x', 'c', 'v', 'b', 'n',  /* 40-49 (132=ä) */
    'm', ',', '.', '-', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_no[] = {
    0,   0,   '!', '"', '#', '$', '%', '&', '/', '(',  /* 0-9 */
    ')', '=', '?', '`', 0,   0,   'Q', 'W', 'E', 'R',  /* 10-19 */
    'T', 'Y', 'U', 'I', 'O', 'P', 143, '^', '\n',0,    /* 20-29 (143=Å) */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 153,  /* 30-39 (153=Ö) */
    142, '*', 0,   '*', 'Z', 'X', 'C', 'V', 'B', 'N',  /* 40-49 (142=Ä) */
    'M', ';', ':', '_', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* Danish QWERTY scancode set 1 -> ASCII */
static const char scancode_ascii_lower_dk[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', '+', '\\',0,   0,   'q', 'w', 'e', 'r',  /* 10-19 */
    't', 'y', 'u', 'i', 'o', 'p', 134, '^', '\n',0,    /* 20-29 (134=å) */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 132,  /* 30-39 (132=ä) */
    148, '\'', 0,  '*', 'z', 'x', 'c', 'v', 'b', 'n',  /* 40-49 (148=ö) */
    'm', ',', '.', '-', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_dk[] = {
    0,   0,   '!', '"', '#', '$', '%', '&', '/', '(',  /* 0-9 */
    ')', '=', '?', '`', 0,   0,   'Q', 'W', 'E', 'R',  /* 10-19 */
    'T', 'Y', 'U', 'I', 'O', 'P', 143, '^', '\n',0,    /* 20-29 (143=Å) */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 142,  /* 30-39 (142=Ä) */
    153, '*', 0,   '*', 'Z', 'X', 'C', 'V', 'B', 'N',  /* 40-49 (153=Ö) */
    'M', ';', ':', '_', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* Swedish QWERTY scancode set 1 -> ASCII */
static const char scancode_ascii_lower_se[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', '+', '\'',0,   0,   'q', 'w', 'e', 'r',  /* 10-19 */
    't', 'y', 'u', 'i', 'o', 'p', 134, '^', '\n',0,    /* 20-29 (134=å) */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 148,  /* 30-39 (148=ö) */
    132, '\'', 0,  '*', 'z', 'x', 'c', 'v', 'b', 'n',  /* 40-49 (132=ä) */
    'm', ',', '.', '-', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_se[] = {
    0,   0,   '!', '"', '#', '$', '%', '&', '/', '(',  /* 0-9 */
    ')', '=', '?', '`', 0,   0,   'Q', 'W', 'E', 'R',  /* 10-19 */
    'T', 'Y', 'U', 'I', 'O', 'P', 143, '^', '\n',0,    /* 20-29 (143=Å) */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 153,  /* 30-39 (153=Ö) */
    142, '*', 0,   '*', 'Z', 'X', 'C', 'V', 'B', 'N',  /* 40-49 (142=Ä) */
    'M', ';', ':', '_', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* Finnish QWERTY scancode set 1 -> ASCII */
static const char scancode_ascii_lower_fi[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', '+', '\'',0,   0,   'q', 'w', 'e', 'r',  /* 10-19 */
    't', 'y', 'u', 'i', 'o', 'p', 134, '^', '\n',0,    /* 20-29 (134=å) */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 148,  /* 30-39 (148=ö) */
    132, '\'', 0,  '*', 'z', 'x', 'c', 'v', 'b', 'n',  /* 40-49 (132=ä) */
    'm', ',', '.', '-', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_fi[] = {
    0,   0,   '!', '"', '#', '$', '%', '&', '/', '(',  /* 0-9 */
    ')', '=', '?', '`', 0,   0,   'Q', 'W', 'E', 'R',  /* 10-19 */
    'T', 'Y', 'U', 'I', 'O', 'P', 143, '^', '\n',0,    /* 20-29 (143=Å) */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 153,  /* 30-39 (153=Ö) */
    142, '*', 0,   '*', 'Z', 'X', 'C', 'V', 'B', 'N',  /* 40-49 (142=Ä) */
    'M', ';', ':', '_', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* Dutch QWERTY scancode set 1 -> ASCII */
static const char scancode_ascii_lower_nl[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', '-', '=', 0,   0,   'q', 'w', 'e', 'r',  /* 10-19 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',0,    /* 20-29 */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',  /* 30-39 */
    '\'','`', 0,   '\\','z', 'x', 'c', 'v', 'b', 'n',  /* 40-49 */
    'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_nl[] = {
    0,   0,   '!', '"', '#', '$', '%', '&', '*', '(',  /* 0-9 */
    ')', '_', '+', '=', 0,   0,   'Q', 'W', 'E', 'R',  /* 10-19 */
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',0,    /* 20-29 */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',  /* 30-39 */
    '"', '~', 0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N',  /* 40-49 */
    'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* Belgian AZERTY scancode set 1 -> ASCII */
static const char scancode_ascii_lower_be[] = {
    0,   0,   '&', 130, 34,  39,  '(',  '#', 138, '!',  /* 0-9 (130=é, 138=è) */
    135, 133, ')', '-', 0,   0,   'a', 'z', 'e', 'r',  /* 10-19 (135=ç, 133=à) */
    't', 'y', 'u', 'i', 'o', 'p', '^', '$', '\n',0,    /* 20-29 */
    'q', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm',  /* 30-39 */
    151, '`', 0,   163, 'w', 'x', 'c', 'v', 'b', 'n',  /* 40-49 (151=ù, 156=£) */
    ',', ';', ':', '=', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_be[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', '^', '+', 0,   0,   'A', 'Z', 'E', 'R',  /* 10-19 (176=°) */
    'T', 'Y', 'U', 'I', 'O', 'P', 168, '*', '\n',0,    /* 20-29 (168=¨) */
    'Q', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 'M',  /* 30-39 */
    '%', '~', 0,   '|', 'W', 'X', 'C', 'V', 'B', 'N',  /* 40-49 */
    '?', '.', '/', '+', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* Polish QWERTY scancode set 1 -> ASCII */
static const char scancode_ascii_lower_pl[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', '-', '=', 0,   0,   'q', 'w', 'e', 'r',  /* 10-19 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',0,    /* 20-29 */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',  /* 30-39 */
    '\'','`', 0,   '\\','z', 'x', 'c', 'v', 'b', 'n',  /* 40-49 */
    'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_pl[] = {
    0,   0,   '!', '"', '#', '$', '%', '&', '*', '(',  /* 0-9 */
    ')', '_', '+', '=', 0,   0,   'Q', 'W', 'E', 'R',  /* 10-19 */
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',0,    /* 20-29 */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',  /* 30-39 */
    '"', '~', 0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N',  /* 40-49 */
    'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* Czech QWERTZ scancode set 1 -> ASCII */
static const char scancode_ascii_lower_cz[] = {
    0,   0,   '+', '1', '2', '3', '4', '5', '6', '7',  /* 0-9 */
    '8', '9', '0', '=', 0,   0,   'q', 'w', 'e', 'r',  /* 10-19 */
    't', 'z', 'u', 'i', 'o', 'p', 139, '(', '\n',0,    /* 20-29 (139=ï/ů) */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 148,  /* 30-39 (148=ö/ů) */
    151, '#', 0,   '\'','y', 'x', 'c', 'v', 'b', 'n',  /* 40-49 (151=ù/ů) */
    'm', ',', '.', '-', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_cz[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', '%', '~', 0,   0,   'Q', 'W', 'E', 'R',  /* 10-19 */
    'T', 'Z', 'U', 'I', 'O', 'P', '/', '(', '\n',0,    /* 20-29 */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '"',  /* 30-39 */
    '!', '^', 0,   ')', 'Y', 'X', 'C', 'V', 'B', 'N',  /* 40-49 */
    'M', '?', ':', '_', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* Slovak QWERTZ scancode set 1 -> ASCII */
static const char scancode_ascii_lower_sk[] = {
    0,   0,   '+', '1', '2', '3', '4', '5', '6', '7',  /* 0-9 */
    '8', '9', '0', '=', 0,   0,   'q', 'w', 'e', 'r',  /* 10-19 */
    't', 'z', 'u', 'i', 'o', 'p', 139, '(', '\n',0,    /* 20-29 (139=ï) */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 148,  /* 30-39 (148=ö) */
    151, '#', 0,   '\'','y', 'x', 'c', 'v', 'b', 'n',  /* 40-49 (151=ù) */
    'm', ',', '.', '-', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_sk[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', '%', '~', 0,   0,   'Q', 'W', 'E', 'R',  /* 10-19 */
    'T', 'Z', 'U', 'I', 'O', 'P', '/', '(', '\n',0,    /* 20-29 */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '"',  /* 30-39 */
    '!', '^', 0,   ')', 'Y', 'X', 'C', 'V', 'B', 'N',  /* 40-49 */
    'M', '?', ':', '_', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* Hungarian QWERTZ scancode set 1 -> ASCII */
static const char scancode_ascii_lower_hu[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', 148, 154, 0,   0,   'q', 'w', 'e', 'r',  /* 10-19 (148=ö, 154=Ü) */
    't', 'z', 'u', 'i', 'o', 'p', 148, 154, '\n',0,    /* 20-29 */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 148,  /* 30-39 */
    129, '0', 0,   129, 'y', 'x', 'c', 'v', 'b', 'n',  /* 40-49 (129=ü) */
    'm', ',', '.', '-', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_hu[] = {
    0,   0,   '\'','"', '+', '!', '%', '/', '=', '(',  /* 0-9 */
    ')', '#', 153, 154, 0,   0,   'Q', 'W', 'E', 'R',  /* 10-19 (153=Ö, 154=Ü) */
    'T', 'Z', 'U', 'I', 'O', 'P', 153, 154, '\n',0,    /* 20-29 */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 153,  /* 30-39 */
    154, '$', 0,   154, 'Y', 'X', 'C', 'V', 'B', 'N',  /* 40-49 (154=Ü) */
    'M', ';', ':', '_', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* Romanian QWERTY scancode set 1 -> ASCII */
static const char scancode_ascii_lower_ro[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', '-', '=', 0,   0,   'q', 'w', 'e', 'r',  /* 10-19 */
    't', 'y', 'u', 'i', 'o', 'p', 131, ']', '\n',0,    /* 20-29 (131=â) */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 131,  /* 30-39 */
    132, '`', 0,   '\\','z', 'x', 'c', 'v', 'b', 'n',  /* 40-49 (132=ä/ă) */
    'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

static const char scancode_ascii_upper_ro[] = {
    0,   0,   '!', '"', '#', '$', '%', '&', '*', '(',  /* 0-9 */
    ')', '_', '+', '=', 0,   0,   'Q', 'W', 'E', 'R',  /* 10-19 */
    'T', 'Y', 'U', 'I', 'O', 'P', 131, '}', '\n',0,    /* 20-29 */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 131,  /* 30-39 */
    142, '~', 0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N',  /* 40-49 (142=Ä) */
    'M', ';', ':', '?', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 */
};

/* Turkish QWERTY scancode set 1 -> ASCII */
static const char scancode_ascii_lower_tr[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0-9 */
    '9', '0', '*', '-', 0,   0,   'q', 'w', 'e', 'r',  /* 10-19 */
    't', 'y', 'u', 'i', 'o', 'p', 139, '$', '\n',0,    /* 20-29 (139=ï/ğ) */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',  /* 30-39 */
    '\"',',', 0,   '<', 'z', 'x', 'c', 'v', 'b', 'n',  /* 40-49 */
    'm', 148, 'c', '.', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 (148=ö) */
};

static const char scancode_ascii_upper_tr[] = {
    0,   0,   '!', '\'','^', '+', '%', '&', '/', '(',  /* 0-9 */
    ')', '=', '?', '_', 0,   0,   'Q', 'W', 'E', 'R',  /* 10-19 */
    'T', 'Y', 'U', 'I', 'O', 'P', 139, '!', '\n',0,    /* 20-29 */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',  /* 30-39 */
    '\"','*', 0,   '>', 'Z', 'X', 'C', 'V', 'B', 'N',  /* 40-49 */
    'M', 153, 'C', ':', 0,   '*', 0,   ' ', 0,   0,    /* 50-59 (153=Ö) */
};

void keyboard_set_layout(int layout) {
    if (layout >= 0 && layout <= (int)(LAYOUT_TR)) current_layout = (layout_t)layout;
}

const char* keyboard_layout_name(int layout) {
    switch (layout) {
        case LAYOUT_US: return "US";
        case LAYOUT_UK: return "UK";
        case LAYOUT_IT: return "IT";
        case LAYOUT_FR: return "FR";
        case LAYOUT_DE: return "DE";
        case LAYOUT_ES: return "ES";
        case LAYOUT_PT: return "PT";
        case LAYOUT_NO: return "NO";
        case LAYOUT_DK: return "DK";
        case LAYOUT_SE: return "SE";
        case LAYOUT_FI: return "FI";
        case LAYOUT_NL: return "NL";
        case LAYOUT_BE: return "BE";
        case LAYOUT_PL: return "PL";
        case LAYOUT_CZ: return "CZ";
        case LAYOUT_SK: return "SK";
        case LAYOUT_HU: return "HU";
        case LAYOUT_RO: return "RO";
        case LAYOUT_TR: return "TR";
        default: return "?";
    }
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
            if (kstrcmp_prefix(p, "us")) keyboard_set_layout(LAYOUT_US);
            else if (kstrcmp_prefix(p, "uk")) keyboard_set_layout(LAYOUT_UK);
            else if (kstrcmp_prefix(p, "it")) keyboard_set_layout(LAYOUT_IT);
            else if (kstrcmp_prefix(p, "fr")) keyboard_set_layout(LAYOUT_FR);
            else if (kstrcmp_prefix(p, "de")) keyboard_set_layout(LAYOUT_DE);
            else if (kstrcmp_prefix(p, "es")) keyboard_set_layout(LAYOUT_ES);
            else if (kstrcmp_prefix(p, "pt")) keyboard_set_layout(LAYOUT_PT);
            else if (kstrcmp_prefix(p, "no")) keyboard_set_layout(LAYOUT_NO);
            else if (kstrcmp_prefix(p, "dk")) keyboard_set_layout(LAYOUT_DK);
            else if (kstrcmp_prefix(p, "se")) keyboard_set_layout(LAYOUT_SE);
            else if (kstrcmp_prefix(p, "fi")) keyboard_set_layout(LAYOUT_FI);
            else if (kstrcmp_prefix(p, "nl")) keyboard_set_layout(LAYOUT_NL);
            else if (kstrcmp_prefix(p, "be")) keyboard_set_layout(LAYOUT_BE);
            else if (kstrcmp_prefix(p, "pl")) keyboard_set_layout(LAYOUT_PL);
            else if (kstrcmp_prefix(p, "cz")) keyboard_set_layout(LAYOUT_CZ);
            else if (kstrcmp_prefix(p, "sk")) keyboard_set_layout(LAYOUT_SK);
            else if (kstrcmp_prefix(p, "hu")) keyboard_set_layout(LAYOUT_HU);
            else if (kstrcmp_prefix(p, "ro")) keyboard_set_layout(LAYOUT_RO);
            else if (kstrcmp_prefix(p, "tr")) keyboard_set_layout(LAYOUT_TR);
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
    
    /* 2. Handle Extended scancodes (arrows, etc.) */
    if (extended_mode) {
        extended_mode = 0;
        if (scancode == 0x48) { arrow_up_requested = 1; return; }    /* Up Arrow */
        if (scancode == 0x50) { arrow_down_requested = 1; return; }  /* Down Arrow */
        if (scancode == 0x4B) { arrow_left_requested = 1; return; }  /* Left Arrow */
        if (scancode == 0x4D) { arrow_right_requested = 1; return; } /* Right Arrow */
        if (scancode == 0x53) { delete_requested = 1; return; }      /* Delete (Canc) */
        return; /* Ignore other extended keys */
    }

    /* 3. Handle Ctrl+T to toggle graphics test mode (Debounced) */
    static int t_key_pressed = 0;
    if (scancode == 0x14) { /* 'T' pressed */
        if (!t_key_pressed && (modifiers & (KEYBOARD_LCTRL | KEYBOARD_RCTRL))) {
            graphics_toggle_requested = 1;
            t_key_pressed = 1;
            return;
        }
        /* If not Ctrl+T, continue to process as regular key */
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

    switch (current_layout) {
        case LAYOUT_US: lower_ptr = scancode_ascii_lower_us; upper_ptr = scancode_ascii_upper_us; break;
        case LAYOUT_UK: lower_ptr = scancode_ascii_lower_uk; upper_ptr = scancode_ascii_upper_uk; break;
        case LAYOUT_IT: lower_ptr = scancode_ascii_lower_it; upper_ptr = scancode_ascii_upper_it; break;
        case LAYOUT_FR: lower_ptr = scancode_ascii_lower_fr; upper_ptr = scancode_ascii_upper_fr; break;
        case LAYOUT_DE: lower_ptr = scancode_ascii_lower_de; upper_ptr = scancode_ascii_upper_de; break;
        case LAYOUT_ES: lower_ptr = scancode_ascii_lower_es; upper_ptr = scancode_ascii_upper_es; break;
        case LAYOUT_PT: lower_ptr = scancode_ascii_lower_pt; upper_ptr = scancode_ascii_upper_pt; break;
        case LAYOUT_NO: lower_ptr = scancode_ascii_lower_no; upper_ptr = scancode_ascii_upper_no; break;
        case LAYOUT_DK: lower_ptr = scancode_ascii_lower_dk; upper_ptr = scancode_ascii_upper_dk; break;
        case LAYOUT_SE: lower_ptr = scancode_ascii_lower_se; upper_ptr = scancode_ascii_upper_se; break;
        case LAYOUT_FI: lower_ptr = scancode_ascii_lower_fi; upper_ptr = scancode_ascii_upper_fi; break;
        case LAYOUT_NL: lower_ptr = scancode_ascii_lower_nl; upper_ptr = scancode_ascii_upper_nl; break;
        case LAYOUT_BE: lower_ptr = scancode_ascii_lower_be; upper_ptr = scancode_ascii_upper_be; break;
        case LAYOUT_PL: lower_ptr = scancode_ascii_lower_pl; upper_ptr = scancode_ascii_upper_pl; break;
        case LAYOUT_CZ: lower_ptr = scancode_ascii_lower_cz; upper_ptr = scancode_ascii_upper_cz; break;
        case LAYOUT_SK: lower_ptr = scancode_ascii_lower_sk; upper_ptr = scancode_ascii_upper_sk; break;
        case LAYOUT_HU: lower_ptr = scancode_ascii_lower_hu; upper_ptr = scancode_ascii_upper_hu; break;
        case LAYOUT_RO: lower_ptr = scancode_ascii_lower_ro; upper_ptr = scancode_ascii_upper_ro; break;
        case LAYOUT_TR: lower_ptr = scancode_ascii_lower_tr; upper_ptr = scancode_ascii_upper_tr; break;
        default:        lower_ptr = scancode_ascii_lower_us; upper_ptr = scancode_ascii_upper_us; break;
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
