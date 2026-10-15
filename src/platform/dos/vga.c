#include "vga.h"

#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include <string.h>
#include <sys/farptr.h>

#define MODE_TEXT 0x03
#define MODE_640X480X16 0x12

#define VIDEO_MEMORY 0xA0000UL
#define COLUMNS (CANVAS_WIDTH / 8)
#define PLANES 4
#define ALL_PLANES 0x0F

#define SEQUENCER_INDEX 0x3C4
#define SEQUENCER_MAP_MASK 0x02
#define DAC_WRITE_INDEX 0x3C8
#define DAC_DATA 0x3C9
#define INPUT_STATUS 0x3DA
#define VERTICAL_RETRACE 0x08

/* What the card shows, to find what changed. */
static uint8_t shown[CANVAS_WIDTH * CANVAS_HEIGHT];

/* Byte k of SPREAD[v] is bit k of pixel value v: one byte per plane. */
static uint32_t SPREAD[16];

static void video_bios(uint16_t ax, uint16_t bx)
{
    __dpmi_regs regs;

    memset(&regs, 0, sizeof(regs));
    regs.x.ax = ax;
    regs.x.bx = bx;
    __dpmi_int(0x10, &regs);
}

/*
 * Mode 12h sends pixel values through the attribute controller to the
 * EGA-compatible DAC entries; mapping each value to itself makes pixel n
 * use DAC entry n.
 */
static void map_attributes_to_dac(void)
{
    uint16_t i;

    for (i = 0; i < FB_VGA_COLORS; i++) {
        video_bios(0x1000, (uint16_t)((i << 8) | i));
    }
}

static void load_palette(void)
{
    int i;

    outportb(DAC_WRITE_INDEX, 0);
    for (i = 0; i < FB_VGA_COLORS; i++) {
        uint32_t color = FB_VGA_PALETTE[i];
        /* The DAC takes 6 bits per channel. */
        outportb(DAC_DATA, (uint8_t)((color >> 18) & 0x3F));
        outportb(DAC_DATA, (uint8_t)((color >> 10) & 0x3F));
        outportb(DAC_DATA, (uint8_t)((color >> 2) & 0x3F));
    }
}

static void build_spread(void)
{
    int value, plane;

    for (value = 0; value < 16; value++) {
        SPREAD[value] = 0;
        for (plane = 0; plane < PLANES; plane++) {
            if (value & (1 << plane)) {
                SPREAD[value] |= 1UL << (plane * 8);
            }
        }
    }
}

void vga_enter(void)
{
    video_bios(MODE_640X480X16, 0);
    map_attributes_to_dac();
    load_palette();
    build_spread();
    /* Setting the mode cleared the screen to color 0. */
    memset(shown, 0, sizeof(shown));
}

void vga_leave(void)
{
    video_bios(MODE_TEXT, 0);
}

static void select_plane(int plane)
{
    outportw(SEQUENCER_INDEX, (uint16_t)(((1 << plane) << 8) | SEQUENCER_MAP_MASK));
}

/* Starting at the beginning of the blank keeps sprites from tearing on a CRT. */
static void wait_for_retrace(void)
{
    while (inportb(INPUT_STATUS) & VERTICAL_RETRACE) {
    }
    while (!(inportb(INPUT_STATUS) & VERTICAL_RETRACE)) {
    }
}

static bool column_changed(const uint8_t *now, const uint8_t *before)
{
    uint32_t a[2], b[2];

    memcpy(a, now, sizeof(a));
    memcpy(b, before, sizeof(b));
    return a[0] != b[0] || a[1] != b[1];
}

/* The four plane bytes of 8 pixels, packed one per byte. */
static uint32_t planar_bytes(const uint8_t *pixels)
{
    uint32_t bytes = 0;
    int i;

    for (i = 0; i < 8; i++) {
        bytes = (bytes << 1) | SPREAD[pixels[i] & 0x0F];
    }
    return bytes;
}

static void write_span(const uint8_t *row, int y, int first, int last)
{
    uint32_t packed[COLUMNS];
    unsigned long address = VIDEO_MEMORY + (unsigned long)y * COLUMNS;
    int plane, column;

    for (column = first; column < last; column++) {
        packed[column] = planar_bytes(row + column * 8);
    }
    for (plane = 0; plane < PLANES; plane++) {
        select_plane(plane);
        for (column = first; column < last; column++) {
            _farnspokeb(address + column, (uint8_t)(packed[column] >> (plane * 8)));
        }
    }
}

void vga_present(const Canvas *canvas, const DirtyRows *dirty)
{
    bool waited = false;
    int y, column;

    _farsetsel(_dos_ds);
    for (y = 0; y < CANVAS_HEIGHT; y++) {
        const uint8_t *row = canvas->pixels + y * CANVAS_WIDTH;
        uint8_t *before = shown + y * CANVAS_WIDTH;
        int last = (dirty->right[y] + 7) / 8;

        for (column = dirty->left[y] / 8; column < last;) {
            int first;

            if (!column_changed(row + column * 8, before + column * 8)) {
                column++;
                continue;
            }
            first = column;
            while (column < last && column_changed(row + column * 8, before + column * 8)) {
                column++;
            }
            if (!waited) {
                wait_for_retrace();
                waited = true;
            }
            write_span(row, y, first, column);
            memcpy(before + first * 8, row + first * 8, (size_t)(column - first) * 8);
        }
    }
    outportw(SEQUENCER_INDEX, (uint16_t)((ALL_PLANES << 8) | SEQUENCER_MAP_MASK));
}
