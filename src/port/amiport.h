/*
 * amiport.h: what libdvdread and libdvdnav need of a C library, inside
 * an Exec library (dvdnav.library). Forced into every file of theirs and
 * of the library (-include), before anything else.
 *
 * A library runs no C startup, so libnix's stdio, its malloc and the rest
 * that want one are not used: port.c provides what the DVD code calls.
 * Printing goes to the opener's log callback; files and directories are
 * never opened (the disc is read through the opener's reader only).
 *
 * The same header is used on the host (tests/dvd_host.c), so the code
 * tested there is the code built for the Amiga.
 *
 * Part of dvdnav.library: GPL-2.0-or-later (see src/dvdlib/README).
 */
#ifndef AMIPORT_H
#define AMIPORT_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>         /* ours (port/assert.h): before the macros below */

#ifdef __amigaos__
/* newlib's headers have no off64_t, and libdvdread's dvd_filesystem.h
 * defines it only for glibc, Windows and the BSDs */
typedef long long off64_t;
#endif

/* ---- printing: into the opener's log, never to a file */
int  dvdp_vsnprintf(char *s, size_t n, const char *fmt, va_list ap);
int  dvdp_snprintf(char *s, size_t n, const char *fmt, ...);
int  dvdp_sprintf(char *s, const char *fmt, ...);
int  dvdp_vfprintf(void *f, const char *fmt, va_list ap);
int  dvdp_fprintf(void *f, const char *fmt, ...);
int  dvdp_printf(const char *fmt, ...);
int  dvdp_puts(const char *s);
int  dvdp_fputc(int c, void *f);
int  dvdp_fputs(const char *s, void *f);
size_t dvdp_fwrite(const void *p, size_t size, size_t n, void *f);
void *dvdp_fopen(const char *name, const char *mode);
int  dvdp_fclose(void *f);
int  dvdp_fflush(void *f);
void dvdp_perror(const char *s);

#undef vsnprintf
#undef snprintf
#undef sprintf
#undef vfprintf
#undef fprintf
#undef printf
#undef puts
#undef fputc
#undef fputs
#undef fwrite
#undef fopen
#undef fclose
#undef fflush
#undef perror
#undef putchar
#undef stdout
#undef stderr
#define vsnprintf dvdp_vsnprintf
#define snprintf  dvdp_snprintf
#define sprintf   dvdp_sprintf
#define vfprintf  dvdp_vfprintf
#define fprintf   dvdp_fprintf
#define printf    dvdp_printf
#define puts      dvdp_puts
#define fputc     dvdp_fputc
#define fputs     dvdp_fputs
#define fwrite    dvdp_fwrite
#define fopen     dvdp_fopen
#define fclose    dvdp_fclose
#define fflush    dvdp_fflush
#define perror    dvdp_perror
#define putchar(c) dvdp_fputc((c), (void *)1)
#define stdout    ((void *)1)
#define stderr    ((void *)2)

/* ---- the rest of libc the DVD code calls, port.c's own */
void *dvdp_malloc(size_t n);
void *dvdp_calloc(size_t n, size_t size);
void *dvdp_realloc(void *p, size_t n);
void  dvdp_free(void *p);
char *dvdp_strdup(const char *s);
char *dvdp_getenv(const char *name);
int   dvdp_rand(void);
void  dvdp_srand(unsigned int seed);
void  dvdp_abort(void) __attribute__((noreturn));
int   dvdp_tolower(int c);
int   dvdp_toupper(int c);
int   dvdp_isspace(int c);
int   dvdp_isprint(int c);
int   dvdp_isalpha(int c);
int   dvdp_strcasecmp(const char *a, const char *b);
int   dvdp_strncasecmp(const char *a, const char *b, size_t n);
int   dvdp_gettimeofday(struct timeval *tv, void *tz);
int   dvdp_open(const char *name, int flags, ...);
int   dvdp_close(int fd);
off_t dvdp_lseek(int fd, off_t off, int whence);
ssize_t dvdp_read(int fd, void *buf, size_t n);
char *dvdp_realpath(const char *path, char *resolved);

#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup
#undef getenv
#undef rand
#undef srand
#undef abort
#undef tolower
#undef toupper
#undef isspace
#undef isprint
#undef isalpha
#undef strcasecmp
#undef strncasecmp
#define malloc      dvdp_malloc
#define calloc      dvdp_calloc
#define realloc     dvdp_realloc
#define free        dvdp_free
#define strdup      dvdp_strdup
#define getenv      dvdp_getenv
#define rand        dvdp_rand
#define srand       dvdp_srand
#define abort       dvdp_abort
#define tolower     dvdp_tolower
#define toupper     dvdp_toupper
#define isspace     dvdp_isspace
#define isprint     dvdp_isprint
#define isalpha     dvdp_isalpha
#define strcasecmp  dvdp_strcasecmp
#define strncasecmp dvdp_strncasecmp
#define gettimeofday dvdp_gettimeofday
#define open        dvdp_open
#define close       dvdp_close
#define lseek       dvdp_lseek
#define read        dvdp_read
#define realpath    dvdp_realpath
#undef RAND_MAX
#define RAND_MAX    0x7fff

#endif /* AMIPORT_H */
