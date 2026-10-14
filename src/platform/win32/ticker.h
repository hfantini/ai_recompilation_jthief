#ifndef JT_WIN32_TICKER_H
#define JT_WIN32_TICKER_H

/*
 * The game clock: posts a message to a window at a fixed rate, from its own
 * thread. Like WM_TIMER, ticks keep arriving while a modal dialog runs its
 * own message loop, at most one is pending at a time, and late ticks are
 * not caught up. SetTimer itself is not used because modern Windows rounds
 * it to multiples of ~15.6 ms.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdbool.h>

bool ticker_start(HWND window, UINT message, double seconds);

/* Call when the tick message has been handled, to allow the next one. */
void ticker_acknowledge(void);

void ticker_stop(void);

#endif
