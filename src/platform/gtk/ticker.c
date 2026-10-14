#include "ticker.h"

#include <glib.h>

#include <stdint.h>
#include <time.h>

#define NANOSECONDS_PER_SECOND 1000000000LL

static GThread *thread;
static TickerCallback callback;
static int64_t period;
static gint running;
static gint pending;

static int64_t to_nanoseconds(const struct timespec *time)
{
    return (int64_t)time->tv_sec * NANOSECONDS_PER_SECOND + time->tv_nsec;
}

static struct timespec from_nanoseconds(int64_t nanoseconds)
{
    struct timespec time;

    time.tv_sec = (time_t)(nanoseconds / NANOSECONDS_PER_SECOND);
    time.tv_nsec = (long)(nanoseconds % NANOSECONDS_PER_SECOND);
    return time;
}

static int64_t now(void)
{
    struct timespec time;

    clock_gettime(CLOCK_MONOTONIC, &time);
    return to_nanoseconds(&time);
}

static gboolean deliver(gpointer unused)
{
    (void)unused;
    callback();
    return G_SOURCE_REMOVE;
}

static gpointer run(gpointer unused)
{
    int64_t next = now() + period;

    (void)unused;
    while (g_atomic_int_get(&running)) {
        struct timespec deadline = from_nanoseconds(next);
        int64_t current;

        /* An absolute deadline keeps the sleeps' own delays from adding up. */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
        current = now();
        if (current < next) {
            continue;
        }
        if (g_atomic_int_compare_and_exchange(&pending, 0, 1)) {
            g_idle_add_full(G_PRIORITY_DEFAULT, deliver, NULL, NULL);
        }
        next += period;
        /* After a stall, resume from now rather than firing a burst. */
        if (current - next > period) {
            next = current + period;
        }
    }
    return NULL;
}

bool ticker_start(TickerCallback on_tick, double seconds)
{
    callback = on_tick;
    period = (int64_t)(seconds * (double)NANOSECONDS_PER_SECOND);
    g_atomic_int_set(&pending, 0);
    g_atomic_int_set(&running, 1);
    thread = g_thread_try_new("ticker", run, NULL, NULL);
    return thread != NULL;
}

void ticker_acknowledge(void)
{
    g_atomic_int_set(&pending, 0);
}

void ticker_stop(void)
{
    if (thread == NULL) {
        return;
    }
    g_atomic_int_set(&running, 0);
    g_thread_join(thread);
    thread = NULL;
}
