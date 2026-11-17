/*
 * config.h for libdvdread 7.1.1 and libdvdnav 7.0.0 in dvdnav.library,
 * in place of what their meson builds would write.
 * Part of dvdnav.library: GPL-2.0-or-later (see src/dvdlib/README).
 */
#define PACKAGE_VERSION "7.1.1"
#define VERSION "7.0.0"
#define UNUSED __attribute__((unused))
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define WORDS_BIGENDIAN 1
#endif
#define HAVE_GETTIMEOFDAY 1
#define HAVE_DLFCN_H 1
#define HAVE_LIMITS_H 1
#define HAVE_STATIC_ASSERT 1
