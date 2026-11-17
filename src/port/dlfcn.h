/*
 * dlfcn.h for libdvdread in dvdnav.library. libdvdread looks for
 * libdvdcss with dlopen(); here that is port.c's, which opens the
 * optional cssdecrypt.library if the user has it (not part of this library),
 * else finds nothing and libdvdread reads discs without CSS.
 * Part of dvdnav.library: GPL-2.0-or-later (see src/dvdlib/README).
 */
#ifndef DVDNAV_DLFCN_H
#define DVDNAV_DLFCN_H
#define RTLD_LAZY 0
#define RTLD_NOW  1
void *dvdp_dlopen(const char *name, int mode);
void *dvdp_dlsym(void *handle, const char *name);
int   dvdp_dlclose(void *handle);
const char *dvdp_dlerror(void);
#define dlopen  dvdp_dlopen
#define dlsym   dvdp_dlsym
#define dlclose dvdp_dlclose
#define dlerror dvdp_dlerror
#endif
