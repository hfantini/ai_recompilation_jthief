#ifndef JT_DOS_HARDWARE_H
#define JT_DOS_HARDWARE_H

/*
 * The PC hardware the game talks to: the BIOS clock and keyboard, the mouse
 * driver and the PC speaker. Everything is polled; no interrupt handler is
 * installed, so DOS and the rest of the machine are left as they were.
 */

#include "keys.h"

#include <stdbool.h>
#include <stdint.h>

/* The BIOS clock, 1193182 / 65536 times a second: the pace of Windows 3.1 timers. */
uint32_t clock_ticks(void);

bool keyboard_read(Key *key);

bool keyboard_alt_held(void);

/* False when no mouse driver is loaded. */
bool mouse_open(int width, int height);

void mouse_read(int *x, int *y, bool *pressed);

/* A short tone, like MessageBeep through the speaker. */
void speaker_beep(void);

/* Ends the tone once it has played; call it often. */
void speaker_update(void);

void speaker_silence(void);

#endif
