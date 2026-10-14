#ifndef JT_GTK_TICKER_H
#define JT_GTK_TICKER_H

/*
 * The game clock, with the semantics of the Win32 port's ticker: a thread
 * hands ticks to the GLib main loop at a fixed rate, at most one is pending
 * at a time, and late ticks are not caught up. GLib timeouts are not used
 * because they count whole milliseconds from each dispatch, so 54.9 ms
 * periods would drift.
 */

#include <stdbool.h>

typedef void (*TickerCallback)(void);

/* `on_tick` runs on the main loop. */
bool ticker_start(TickerCallback on_tick, double seconds);

/* Called by the tick handler, so the next tick may be queued. */
void ticker_acknowledge(void);

void ticker_stop(void);

#endif
