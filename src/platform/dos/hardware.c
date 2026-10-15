#include "hardware.h"

#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include <string.h>
#include <sys/farptr.h>

/* The BIOS data area. */
#define BIOS_KEYBOARD_FLAGS 0x417
#define BIOS_KEYBOARD_HEAD 0x41A
#define BIOS_KEYBOARD_TAIL 0x41C
#define BIOS_CLOCK 0x46C

#define FLAG_RIGHT_SHIFT 0x01
#define FLAG_LEFT_SHIFT 0x02
#define FLAG_CTRL 0x04
#define FLAG_ALT 0x08

/* INT 16h function 10h marks the gray keys with this character. */
#define GRAY_KEY 0xE0

#define PIT_FREQUENCY 1193182UL
#define PIT_COMMAND 0x43
#define PIT_SPEAKER_CHANNEL 0x42
#define PIT_SPEAKER_SQUARE_WAVE 0xB6
#define SPEAKER_PORT 0x61
#define SPEAKER_ENABLE 0x03

/* MessageBeep stand-in, the same tone as the other ports. */
#define BEEP_FREQUENCY 880
#define BEEP_TICKS 2

static bool beeping;
static uint32_t beep_start;

uint32_t clock_ticks(void)
{
    return _farpeekl(_dos_ds, BIOS_CLOCK);
}

static uint8_t keyboard_flags(void)
{
    return _farpeekb(_dos_ds, BIOS_KEYBOARD_FLAGS);
}

/* Peeking at the buffer is much cheaper than calling the BIOS when it is empty. */
static bool keyboard_pending(void)
{
    return _farpeekw(_dos_ds, BIOS_KEYBOARD_HEAD) != _farpeekw(_dos_ds, BIOS_KEYBOARD_TAIL);
}

bool keyboard_read(Key *key)
{
    __dpmi_regs regs;
    uint8_t flags;

    if (!keyboard_pending()) {
        return false;
    }
    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 0x10;
    __dpmi_int(0x16, &regs);

    flags = keyboard_flags();
    key->scan = regs.h.ah;
    key->ascii = regs.h.al == GRAY_KEY ? 0 : regs.h.al;
    key->shift = (flags & (FLAG_LEFT_SHIFT | FLAG_RIGHT_SHIFT)) != 0;
    key->ctrl = (flags & FLAG_CTRL) != 0;
    key->alt = (flags & FLAG_ALT) != 0;
    return true;
}

bool keyboard_alt_held(void)
{
    return (keyboard_flags() & FLAG_ALT) != 0;
}

static void mouse_call(__dpmi_regs *regs)
{
    __dpmi_int(0x33, regs);
}

bool mouse_open(int width, int height)
{
    __dpmi_regs regs;

    memset(&regs, 0, sizeof(regs));
    mouse_call(&regs);
    if (regs.x.ax != 0xFFFF) {
        return false;
    }
    /* Not every driver picks the range of the video mode by itself. */
    memset(&regs, 0, sizeof(regs));
    regs.x.ax = 7;
    regs.x.dx = (uint16_t)(width - 1);
    mouse_call(&regs);
    memset(&regs, 0, sizeof(regs));
    regs.x.ax = 8;
    regs.x.dx = (uint16_t)(height - 1);
    mouse_call(&regs);
    return true;
}

void mouse_read(int *x, int *y, bool *pressed)
{
    __dpmi_regs regs;

    memset(&regs, 0, sizeof(regs));
    regs.x.ax = 3;
    mouse_call(&regs);
    *x = (int16_t)regs.x.cx;
    *y = (int16_t)regs.x.dx;
    *pressed = (regs.x.bx & 1) != 0;
}

void speaker_beep(void)
{
    uint16_t divisor = (uint16_t)(PIT_FREQUENCY / BEEP_FREQUENCY);

    outportb(PIT_COMMAND, PIT_SPEAKER_SQUARE_WAVE);
    outportb(PIT_SPEAKER_CHANNEL, (uint8_t)divisor);
    outportb(PIT_SPEAKER_CHANNEL, (uint8_t)(divisor >> 8));
    outportb(SPEAKER_PORT, inportb(SPEAKER_PORT) | SPEAKER_ENABLE);
    beeping = true;
    beep_start = clock_ticks();
}

void speaker_update(void)
{
    if (beeping && clock_ticks() - beep_start >= BEEP_TICKS) {
        speaker_silence();
    }
}

void speaker_silence(void)
{
    outportb(SPEAKER_PORT, inportb(SPEAKER_PORT) & (uint8_t)~SPEAKER_ENABLE);
    beeping = false;
}
