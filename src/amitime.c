/*
 * amitime.c: the time for libdvdnav's gettimeofday (port.c), from the
 * E-clock. Apart from port.c because devices/timer.h and newlib's
 * sys/time.h both define struct timeval; built without port/amiport.h.
 *
 * (c) 2026 Brian King. Part of dvdnav.library: GPL-2.0-or-later.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/timer.h>

struct Device *TimerBase;                   /* opened by the library (dvdnav.c) */

/* Seconds and microseconds since some start (the E-clock's): libdvdnav
 * only measures intervals and seeds its random numbers with them. */
void dvdp_clock(unsigned long *sec, unsigned long *usec)
{
    struct EClockVal ev;
    unsigned long long t;
    ULONG hz;
    *sec = *usec = 0;
    if (!TimerBase || !(hz = ReadEClock(&ev)))
        return;
    t = ((unsigned long long)ev.ev_hi << 32) | ev.ev_lo;
    *sec = (unsigned long)(t / hz);
    *usec = (unsigned long)((t % hz) * 1000000ULL / hz);
}

/* timer.device, opened once for the library (no requests are sent: only
 * ReadEClock, which needs the base) */
static struct timerequest *treq;

int dvdp_timer_open(void)
{
    if (!(treq = AllocVec(sizeof(*treq), MEMF_PUBLIC | MEMF_CLEAR)))
        return 0;
    treq->tr_node.io_Message.mn_Length = sizeof(*treq);
    if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)treq, 0)) {
        FreeVec(treq);
        treq = NULL;
        return 0;
    }
    TimerBase = treq->tr_node.io_Device;
    return 1;
}

void dvdp_timer_close(void)
{
    if (treq) {
        CloseDevice((struct IORequest *)treq);
        FreeVec(treq);
        treq = NULL;
    }
    TimerBase = NULL;
}
