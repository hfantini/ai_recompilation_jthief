#ifndef JT_DOS_VGA_H
#define JT_DOS_VGA_H

/*
 * VGA mode 12h: 640x480 in 16 colors, stored in four bit planes. The DAC
 * holds the Windows VGA palette in Windows order, so a pixel value is the
 * very palette index the original's raster operations worked on.
 */

#include "canvas.h"

void vga_enter(void);

/* Back to the 80x25 text mode DOS started in. */
void vga_leave(void);

/*
 * Copies the dirty parts of the canvas to the screen. Only the bytes that
 * differ from what the card shows are written: on an ISA bus, video memory
 * is slow.
 */
void vga_present(const Canvas *canvas, const DirtyRows *dirty);

#endif
