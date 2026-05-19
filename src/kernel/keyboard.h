/* openkernel - PS/2 Keyboard Driver */
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

/* Keyboard state flags */
#define KEYBOARD_CAPS_LOCK  0x01
#define KEYBOARD_NUM_LOCK   0x02
#define KEYBOARD_SCROLL_LOCK 0x04
#define KEYBOARD_LSHIFT     0x08
#define KEYBOARD_RSHIFT     0x10
#define KEYBOARD_LALT       0x20
#define KEYBOARD_LCTRL      0x40
#define KEYBOARD_RCTRL      0x80

/* Initialize keyboard driver */
void keyboard_init(void);

/* Automatically identify layout from boot command line */
void keyboard_detect_layout(const char* cmdline);

/* Get current keyboard modifiers */
uint8_t keyboard_get_modifiers(void);

/* Read last scancode (non-blocking, 0 = no key) */
uint8_t keyboard_get_scancode(void);

/* Convert scancode to ASCII character (respects shift/caps) */
char keyboard_scancode_to_ascii(uint8_t scancode);

/* Arrow key request flags - set by IRQ, consumed in main loop */
extern volatile int arrow_left_requested;
extern volatile int arrow_right_requested;
extern volatile int arrow_up_requested;
extern volatile int arrow_down_requested;
extern volatile int delete_requested;

/* Set keyboard layout by ID (0=US, 1=UK, 2=IT) */
void keyboard_set_layout(int layout);

#endif /* KEYBOARD_H */