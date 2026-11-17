/*
 * assert.h for libdvdread and libdvdnav in dvdnav.library: a failed
 * assertion is a line in the opener's log, and the code goes on, as
 * libdvdnav does when built without assertions. A library must not stop
 * the program that opened it. assert(0), where libdvdnav finds itself
 * somewhere it cannot be (and would fall off the end of a function),
 * ends the library call with an error instead (port.c's abort).
 *
 * No include guard, like any assert.h.
 * Part of dvdnav.library: GPL-2.0-or-later (see src/dvdlib/README).
 */
#undef assert
void dvdp_assert_failed(const char *expr, const char *file, int line);
void dvdp_abort(void) __attribute__((noreturn));
#define assert(e) ((e) ? (void)0 : \
                   (__builtin_constant_p(e) ? dvdp_abort() : dvdp_assert_failed(#e, __FILE__, __LINE__)))
