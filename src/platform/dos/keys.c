#include "keys.h"

#include <stddef.h>

char keyboard_letter(const Key *key)
{
    static const char ROWS[] = "QWERTYUIOP\0\0\0\0ASDFGHJKL\0\0\0\0\0ZXCVBNM";
    enum { FIRST_LETTER_SCAN = 0x10 };
    int offset = key->scan - FIRST_LETTER_SCAN;

    if (key->ascii >= 'a' && key->ascii <= 'z') {
        return (char)(key->ascii - 'a' + 'A');
    }
    if (key->ascii >= 'A' && key->ascii <= 'Z') {
        return (char)key->ascii;
    }
    /* With Alt held there is no character, only the key's position. */
    if (key->ascii == 0 && offset >= 0 && offset < (int)sizeof(ROWS) - 1) {
        return ROWS[offset];
    }
    return 0;
}

int keyboard_function_number(const Key *key)
{
    /* F1-F10 come in four blocks of ten: plain, Shift, Ctrl and Alt. */
    static const uint8_t BLOCKS[] = { 0x3B, 0x54, 0x5E, 0x68 };
    size_t i;

    if (key->ascii != 0) {
        return 0;
    }
    for (i = 0; i < sizeof(BLOCKS); i++) {
        if (key->scan >= BLOCKS[i] && key->scan < BLOCKS[i] + 10) {
            return key->scan - BLOCKS[i] + 1;
        }
    }
    switch (key->scan) {
    case SCAN_F11:
    case 0x87:
    case 0x89:
    case 0x8B:
        return 11;
    case SCAN_F12:
    case 0x88:
    case 0x8A:
    case 0x8C:
        return 12;
    }
    return 0;
}
