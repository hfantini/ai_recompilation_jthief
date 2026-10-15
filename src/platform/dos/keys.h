#ifndef JT_DOS_KEYS_H
#define JT_DOS_KEYS_H

/* Key presses as the BIOS reports them, and what they stand for. */

#include <stdbool.h>
#include <stdint.h>

/* BIOS scan codes, as INT 16h function 10h reports them. */
enum {
    SCAN_ESCAPE = 0x01,
    SCAN_BACKSPACE = 0x0E,
    SCAN_TAB = 0x0F,
    SCAN_ENTER = 0x1C,
    SCAN_SPACE = 0x39,
    SCAN_F10 = 0x44,
    SCAN_HOME = 0x47,
    SCAN_UP = 0x48,
    SCAN_PAGE_UP = 0x49,
    SCAN_LEFT = 0x4B,
    SCAN_RIGHT = 0x4D,
    SCAN_END = 0x4F,
    SCAN_DOWN = 0x50,
    SCAN_PAGE_DOWN = 0x51,
    SCAN_DELETE = 0x53,
    SCAN_F11 = 0x85,
    SCAN_F12 = 0x86,
    SCAN_KEYPAD_ENTER = 0xE0
};

typedef struct {
    uint8_t scan;
    uint8_t ascii; /* code page 437; 0 for keys without a character */
    bool shift;
    bool ctrl;
    bool alt;
} Key;

/* The letter a key stands for, uppercase, even with Alt held; 0 if none. */
char keyboard_letter(const Key *key);

/* The function key number (1-12) a key is, whatever modifier is held; 0 if none. */
int keyboard_function_number(const Key *key);

#endif
