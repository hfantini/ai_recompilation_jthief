#ifndef JT_DOS_MENU_H
#define JT_DOS_MENU_H

/*
 * The main window's menu bar and its popups, with Windows 3.1 behavior:
 * opened with the mouse, Alt, F10 or Alt plus a mnemonic, browsed with the
 * arrows, and closed with Escape. The game clock keeps running meanwhile.
 */

#include "canvas.h"
#include "font.h"
#include "templates.h"

#include <stdbool.h>

void menu_open(const MenuTemplate *menu, const Font *font);

void menu_paint_bar(Canvas *canvas, Rect bar);

/* The bar item at a screen position, or -1. */
int menu_bar_item_at(int x, int y);

/* The bar item whose mnemonic is `letter`, or -1. */
int menu_bar_item_for(char letter);

/*
 * Runs the menu from bar item `item` until a command is picked, which is
 * returned, or the menu is dismissed, which returns 0. `open` shows the
 * item's popup at once; `by_mouse` means the button is held on the bar.
 */
int menu_track(int item, bool open, bool by_mouse);

#endif
