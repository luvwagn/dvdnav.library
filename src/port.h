/*
 * port.h: between dvdnav.c and port.c (the C library for libdvdread and
 * libdvdnav inside dvdnav.library).
 * (c) 2026 Brian King. Part of dvdnav.library: GPL-2.0-or-later.
 */
#ifndef DVDNAV_PORT_H
#define DVDNAV_PORT_H
#include <setjmp.h>

/* the opener's context, set by each library call (dvdnav.c) */
struct dvdp_ctx {
    void (*log)(struct dvdp_ctx *ctx, int level, const char *line); /* DVDLOG_ levels */
    jmp_buf *jb;                    /* abort() returns here: the call fails */
    int css;                        /* this disc holds cssdecrypt.library open (port.c) */
};
extern struct dvdp_ctx *dvdp_cur;

unsigned long dvdp_memory(int peak);
void dvdp_clock(unsigned long *sec, unsigned long *usec);  /* amitime.c */
void dvdp_css_end(void);        /* cssdecrypt.library let go (port.c) */
void dvdp_css_drop(struct dvdp_ctx *ctx);   /* a disc closed: its hold on it let go */
#endif
