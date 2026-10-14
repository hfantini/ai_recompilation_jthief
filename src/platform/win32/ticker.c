#include "ticker.h"

#include <mmsystem.h>

static HANDLE thread;
static HWND target;
static UINT tick_message;
static LONGLONG period;
static volatile LONG running;
static volatile LONG pending;

static DWORD WINAPI run(LPVOID unused)
{
    LARGE_INTEGER frequency, now;
    LONGLONG next;

    (void)unused;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    next = now.QuadPart + period;

    while (InterlockedCompareExchange(&running, 1, 1)) {
        QueryPerformanceCounter(&now);
        if (now.QuadPart < next) {
            Sleep((DWORD)((next - now.QuadPart) * 1000 / frequency.QuadPart));
            continue;
        }
        if (InterlockedCompareExchange(&pending, 1, 0) == 0) {
            PostMessage(target, tick_message, 0, 0);
        }
        next += period;
        /* After a stall, resume from now rather than firing a burst. */
        if (now.QuadPart - next > period) {
            next = now.QuadPart + period;
        }
    }
    return 0;
}

bool ticker_start(HWND window, UINT message, double seconds)
{
    LARGE_INTEGER frequency;

    QueryPerformanceFrequency(&frequency);
    target = window;
    tick_message = message;
    period = (LONGLONG)(seconds * (double)frequency.QuadPart);
    pending = 0;
    running = 1;
    /* Sleeps wake up within 1 ms instead of ~15.6 ms. */
    timeBeginPeriod(1);
    thread = CreateThread(NULL, 0, run, NULL, 0, NULL);
    if (thread == NULL) {
        timeEndPeriod(1);
        return false;
    }
    return true;
}

void ticker_acknowledge(void)
{
    InterlockedExchange(&pending, 0);
}

void ticker_stop(void)
{
    if (thread == NULL) {
        return;
    }
    InterlockedExchange(&running, 0);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    thread = NULL;
    timeEndPeriod(1);
}
