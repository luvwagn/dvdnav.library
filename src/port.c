/*
 * port.c: the C library pieces libdvdread and libdvdnav call, for
 * dvdnav.library (see port/amiport.h, which routes their calls here).
 *
 * An Exec library runs no C startup, so nothing here may need one:
 * memory comes from exec, printing goes to the opener's log callback,
 * and no file is ever opened (the disc comes through the opener's
 * reader). The same file builds on the host for tests/dvd_test.c.
 *
 * (c) 2026 Brian King. Part of dvdnav.library: GPL-2.0-or-later.
 */
#include "port.h"
#include <setjmp.h>

#ifdef __amigaos__
#include <exec/memory.h>
#include <exec/alerts.h>
#include <proto/exec.h>
#endif

/* The opener's context for whatever the DVD code prints or aborts:
 * set by every library call (dvdnav.c), on the caller's task. */
struct dvdp_ctx *dvdp_cur;

/* ---- memory: exec's, with the size kept in front (realloc), counted */
static unsigned long mem_now, mem_peak;

unsigned long dvdp_memory(int peak)
{
    return peak ? mem_peak : mem_now;
}

void *dvdp_malloc(size_t n)
{
    unsigned long *p;
#ifdef __amigaos__
    p = AllocVec(n + 8, MEMF_ANY);
#else
#undef malloc
    p = malloc(n + 8);
#define malloc dvdp_malloc
#endif
    if (!p)
        return NULL;
    p[0] = n;
    if ((mem_now += n) > mem_peak)
        mem_peak = mem_now;
    return (char *)p + 8;                   /* 8-byte aligned past the header */
}

void dvdp_free(void *v)
{
    unsigned long *p;
    if (!v)
        return;
    p = (unsigned long *)((char *)v - 8);
    mem_now -= p[0];
#ifdef __amigaos__
    FreeVec(p);
#else
#undef free
    free(p);
#define free dvdp_free
#endif
}

void *dvdp_calloc(size_t n, size_t size)
{
    size_t total = n * size;
    void *p;
    if (size && total / size != n)
        return NULL;
    if ((p = dvdp_malloc(total)))
        memset(p, 0, total);
    return p;
}

void *dvdp_realloc(void *p, size_t n)
{
    size_t old;
    void *q;
    if (!p)
        return dvdp_malloc(n);
    if (!n) {
        dvdp_free(p);
        return NULL;
    }
    old = ((unsigned long *)((char *)p - 8))[0];
    if (n <= old)
        return p;
    if (!(q = dvdp_malloc(n)))
        return NULL;
    memcpy(q, p, old);
    dvdp_free(p);
    return q;
}

char *dvdp_strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = dvdp_malloc(n);
    if (d)
        memcpy(d, s, n);
    return d;
}

/* ---- formatting: enough for libdvdread and libdvdnav's messages
 * (d i u x X o c s p %, flags - 0 + space #, width and precision, also
 * as *, and the h hh l ll z j t lengths). No floating point. */
struct out {
    char *s;
    size_t n, len;
};

static void put(struct out *o, char c)
{
    if (o->len + 1 < o->n)
        o->s[o->len] = c;
    o->len++;
}

int dvdp_vsnprintf(char *s, size_t n, const char *fmt, va_list ap)
{
    struct out o;
    o.s = s;
    o.n = n;
    o.len = 0;
    while (*fmt) {
        char c = *fmt++, conv, lenmod = 0, sign = 0, pad = ' ';
        int left = 0, alt = 0, width = 0, prec = -1, base = 10, upper = 0, neg = 0, i, len;
        char buf[24];
        const char *str;
        unsigned long long u = 0;
        if (c != '%') {
            put(&o, c);
            continue;
        }
        for (;; fmt++) {                    /* flags */
            if (*fmt == '-')
                left = 1;
            else if (*fmt == '0')
                pad = '0';
            else if (*fmt == '+')
                sign = '+';
            else if (*fmt == ' ') {
                if (!sign)
                    sign = ' ';
            } else if (*fmt == '#')
                alt = 1;
            else
                break;
        }
        if (*fmt == '*') {
            width = va_arg(ap, int);
            if (width < 0) {
                left = 1;
                width = -width;
            }
            fmt++;
        } else
            while (*fmt >= '0' && *fmt <= '9')
                width = width * 10 + *fmt++ - '0';
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            if (*fmt == '*') {
                prec = va_arg(ap, int);
                fmt++;
            } else
                while (*fmt >= '0' && *fmt <= '9')
                    prec = prec * 10 + *fmt++ - '0';
        }
        for (;; fmt++) {                    /* length */
            if (*fmt == 'h')
                lenmod = lenmod == 'h' ? 'H' : 'h';
            else if (*fmt == 'l')
                lenmod = lenmod == 'l' ? 'L' : 'l';
            else if (*fmt == 'z' || *fmt == 't')
                lenmod = 'l';
            else if (*fmt == 'j' || *fmt == 'q')
                lenmod = 'L';
            else
                break;
        }
        conv = *fmt ? *fmt++ : 0;
        switch (conv) {
        case 'd':
        case 'i': {
            long long v = lenmod == 'L' ? va_arg(ap, long long) :
                          lenmod == 'l' ? va_arg(ap, long) : va_arg(ap, int);
            if (lenmod == 'h')
                v = (short)v;
            else if (lenmod == 'H')
                v = (signed char)v;
            if (v < 0) {
                neg = 1;
                u = (unsigned long long)(-(v + 1)) + 1;
            } else
                u = (unsigned long long)v;
            goto number;
        }
        case 'p':
            u = (unsigned long)va_arg(ap, void *);
            base = 16;
            alt = 1;
            goto number;
        case 'o':
            base = 8;
            goto unsig;
        case 'X':
            upper = 1;
            /* fall through */
        case 'x':
            base = 16;
            /* fall through */
        case 'u':
        unsig:
            u = lenmod == 'L' ? va_arg(ap, unsigned long long) :
                lenmod == 'l' ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            if (lenmod == 'h')
                u = (unsigned short)u;
            else if (lenmod == 'H')
                u = (unsigned char)u;
            sign = 0;
        number:
            len = 0;
            do {
                int d = (int)(u % (unsigned)base);
                buf[len++] = (char)(d < 10 ? '0' + d : (upper ? 'A' : 'a') + d - 10);
                u /= (unsigned)base;
            } while (u);
            if (prec >= 0)
                pad = ' ';
            {
                int digits = len > prec ? len : prec;
                int pre = (neg || sign ? 1 : 0) + (alt && base == 16 ? 2 : 0) + (alt && base == 8 ? 1 : 0);
                int fill = width > digits + pre ? width - digits - pre : 0;
                if (!left && pad == ' ')
                    while (fill-- > 0)
                        put(&o, ' ');
                if (neg)
                    put(&o, '-');
                else if (sign)
                    put(&o, sign);
                if (alt && base == 16) {
                    put(&o, '0');
                    put(&o, upper ? 'X' : 'x');
                } else if (alt && base == 8)
                    put(&o, '0');
                if (!left && pad == '0')
                    while (fill-- > 0)
                        put(&o, '0');
                for (i = len; i < digits; i++)
                    put(&o, '0');
                while (len)
                    put(&o, buf[--len]);
                if (left)
                    while (fill-- > 0)
                        put(&o, ' ');
            }
            break;
        case 'c':
            buf[0] = (char)va_arg(ap, int);
            buf[1] = 0;
            str = buf;
            len = 1;
            goto string;
        case 's':
            str = va_arg(ap, const char *);
            if (!str)
                str = "(null)";
            for (len = 0; str[len] && (prec < 0 || len < prec); len++)
                ;
        string:
            if (!left)
                for (i = len; i < width; i++)
                    put(&o, ' ');
            for (i = 0; i < len; i++)
                put(&o, str[i]);
            if (left)
                for (i = len; i < width; i++)
                    put(&o, ' ');
            break;
        case '%':
            put(&o, '%');
            break;
        default:                            /* unknown: shown as it was */
            put(&o, '%');
            if (conv)
                put(&o, conv);
            break;
        }
    }
    if (o.n)
        o.s[o.len < o.n ? o.len : o.n - 1] = 0;
    return (int)o.len;
}

int dvdp_snprintf(char *s, size_t n, const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = dvdp_vsnprintf(s, n, fmt, ap);
    va_end(ap);
    return r;
}

int dvdp_sprintf(char *s, const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = dvdp_vsnprintf(s, 1024, fmt, ap);   /* the DVD code's buffers are small and sized for it */
    va_end(ap);
    return r;
}

/* ---- printing: lines into the opener's log at DVDLOG_DEBUG, gathered
 * until a newline (libdvdnav prints some lines in pieces) */
static char line[256];
static size_t line_len;

static void line_flush(void)
{
    line[line_len] = 0;
    if (line_len && dvdp_cur && dvdp_cur->log)
        dvdp_cur->log(dvdp_cur, 3, line);
    line_len = 0;
}

static void line_add(const char *s, size_t n)
{
    while (n--) {
        char c = *s++;
        if (c == '\n')
            line_flush();
        else if (line_len < sizeof(line) - 1)
            line[line_len++] = c;
    }
}

int dvdp_vfprintf(void *f, const char *fmt, va_list ap)
{
    char buf[256];
    int n = dvdp_vsnprintf(buf, sizeof(buf), fmt, ap);
    (void)f;
    line_add(buf, strlen(buf));
    return n;
}

int dvdp_fprintf(void *f, const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = dvdp_vfprintf(f, fmt, ap);
    va_end(ap);
    return r;
}

int dvdp_printf(const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = dvdp_vfprintf((void *)1, fmt, ap);
    va_end(ap);
    return r;
}

int dvdp_fputs(const char *s, void *f)
{
    (void)f;
    line_add(s, strlen(s));
    return 0;
}

int dvdp_puts(const char *s)
{
    line_add(s, strlen(s));
    line_add("\n", 1);
    return 0;
}

int dvdp_fputc(int c, void *f)
{
    char ch = (char)c;
    (void)f;
    line_add(&ch, 1);
    return c;
}

size_t dvdp_fwrite(const void *p, size_t size, size_t n, void *f)
{
    (void)f;
    line_add(p, size * n);
    return n;
}

void *dvdp_fopen(const char *name, const char *mode)
{
    (void)name;
    (void)mode;
    return NULL;
}

int dvdp_fclose(void *f)
{
    (void)f;
    return 0;
}

int dvdp_fflush(void *f)
{
    (void)f;
    return 0;
}

void dvdp_perror(const char *s)
{
    line_add(s, strlen(s));
    line_add("\n", 1);
}

void dvdp_assert_failed(const char *expr, const char *file, int line_no)
{
    if (dvdp_cur && dvdp_cur->log) {
        char buf[200];
        const char *base = strrchr(file, '/');
        dvdp_snprintf(buf, sizeof(buf), "assertion failed: %s (%s:%d), going on",
                      expr, base ? base + 1 : file, line_no);
        dvdp_cur->log(dvdp_cur, 1, buf);
    }
}

/* A NULL where libdvdread checks for one ("Passed NULL pointers") or a
 * domain libdvdnav cannot be in. The library call in progress ends with
 * DVDE_ERROR (dvdnav.c), and the handle is not used again. */
void dvdp_abort(void)
{
    line_flush();
    if (dvdp_cur && dvdp_cur->jb) {
        if (dvdp_cur->log)
            dvdp_cur->log(dvdp_cur, 0, "internal error (abort): this disc is not played further");
        longjmp(*dvdp_cur->jb, 1);
    }
#ifdef __amigaos__
    Alert(AT_Recovery | 0x41444456);        /* 'ADDV': nothing to return to */
    for (;;)
        Wait(0);
#else
#undef abort
    abort();
#endif
}

/* ---- the rest */
char *dvdp_getenv(const char *name)
{
    (void)name;                             /* DVDREAD_NOKEYS, DVDCSS_*: never set */
    return NULL;
}

static unsigned long rand_state = 1;

int dvdp_rand(void)
{
    rand_state = rand_state * 1103515245UL + 12345UL;
    return (int)((rand_state >> 16) & 0x7fff);
}

void dvdp_srand(unsigned int seed)
{
    rand_state = seed;
}

int dvdp_tolower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

int dvdp_toupper(int c)
{
    return c >= 'a' && c <= 'z' ? c - 32 : c;
}

int dvdp_isspace(int c)
{
    return c == ' ' || (c >= 9 && c <= 13);
}

int dvdp_isprint(int c)
{
    return c >= 32 && c < 127;
}

int dvdp_isalpha(int c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int dvdp_strncasecmp(const char *a, const char *b, size_t n)
{
    while (n--) {
        int x = dvdp_tolower((unsigned char)*a++), y = dvdp_tolower((unsigned char)*b++);
        if (x != y)
            return x - y;
        if (!x)
            break;
    }
    return 0;
}

int dvdp_strcasecmp(const char *a, const char *b)
{
    return dvdp_strncasecmp(a, b, (size_t)-1);
}

int dvdp_gettimeofday(struct timeval *tv, void *tz)
{
    (void)tz;
#ifdef __amigaos__
    {
        unsigned long sec, usec;
        dvdp_clock(&sec, &usec);            /* amitime.c: the E-clock */
        tv->tv_sec = sec;
        tv->tv_usec = usec;
    }
    return 0;
#else
#undef gettimeofday
    return gettimeofday(tv, NULL);
#endif
}

/* No files: libdvdread and libdvdnav only open paths when given one
 * (DVDOpen, dvdnav_open), and the library only uses their stream forms. */
int dvdp_open(const char *name, int flags, ...)
{
    (void)name;
    (void)flags;
    return -1;
}

int dvdp_close(int fd)
{
    (void)fd;
    return -1;
}

off_t dvdp_lseek(int fd, off_t off, int whence)
{
    (void)fd;
    (void)off;
    (void)whence;
    return -1;
}

ssize_t dvdp_read(int fd, void *buf, size_t n)
{
    (void)fd;
    (void)buf;
    (void)n;
    return -1;
}

char *dvdp_realpath(const char *path, char *resolved)
{
    (void)path;
    (void)resolved;
    return NULL;
}

/* libdvdread's dlopen() of libdvdcss: cssdecrypt.library, if the user
 * has put it in the program's Libs or LIBS: (an optional library, not part
 * of dvdnav.library: libdvdcss for CSS-scrambled discs). Held open while a
 * disc that asked for it is (dvdp_css_drop at DvdClose); and a copy left
 * in memory by an earlier dvdnav.library, no one using it, is put out
 * before the first open, so a newer file in Libs is the one loaded (Exec
 * would otherwise give the old one again). Its calls, libdvdcss's own (a
 * register each): -30 CssOpenStream(a0 stream, a1 callbacks), -36
 * CssClose(a0), -42 CssSeek(a0, d0 block, d1 flags), -48 CssRead(a0, a1
 * buffer, d0 blocks, d1 flags); libdvdread calls them through the
 * functions below, as it would libdvdcss's. Without it, discs are read as
 * they are (an unscrambled image or disc plays; a scrambled one does not). */
#ifdef __amigaos__
#include <inline/macros.h>
static struct Library *CssBase;
static int css_users, css_checked;

static void *css_open_stream(void *stream, void *cb)
{
    return LP2(0x1e, APTR, CssOpenStream, APTR, stream, a0, APTR, cb, a1, , CssBase);
}

static void *css_open(const char *path)
{
    (void)path;                             /* (only streams: the opener's reader) */
    return NULL;
}

static int css_close(void *d)
{
    return (int)LP1(0x24, LONG, CssClose, APTR, d, a0, , CssBase);
}

static int css_seek(void *d, int block, int flags)
{
    return (int)LP3(0x2a, LONG, CssSeek, APTR, d, a0, LONG, block, d0, LONG, flags, d1, , CssBase);
}

static int css_read(void *d, void *buf, int blocks, int flags)
{
    return (int)LP4(0x30, LONG, CssRead, APTR, d, a0, APTR, buf, a1, LONG, blocks, d0, LONG, flags, d1, ,
                    CssBase);
}
#endif

void *dvdp_dlopen(const char *name, int mode)
{
    (void)mode;
#ifdef __amigaos__
    if (!strstr(name, "dvdcss"))
        return NULL;
    if (!CssBase) {
        struct Library *l;
        Forbid();                           /* (a stale copy nobody has open, left by an
                                             * earlier dvdnav: out, once) */
        if (!css_checked &&
            (l = (struct Library *)FindName(&SysBase->LibList, (CONST_STRPTR) "cssdecrypt.library")) &&
            !l->lib_OpenCnt)
            RemLibrary(l);
        css_checked = 1;
        Permit();
        if (!(CssBase = OpenLibrary((CONST_STRPTR) "PROGDIR:Libs/cssdecrypt.library", 1)))
            CssBase = OpenLibrary((CONST_STRPTR) "cssdecrypt.library", 1);
        if (CssBase && dvdp_cur && dvdp_cur->log) {
            char buf[100];
            dvdp_snprintf(buf, sizeof(buf), "cssdecrypt.library %d.%d: CSS-scrambled discs read",
                          CssBase->lib_Version, CssBase->lib_Revision);
            dvdp_cur->log(dvdp_cur, 2, buf);
        }
    }
    if (CssBase && dvdp_cur && !dvdp_cur->css) {
        dvdp_cur->css = 1;
        css_users++;
    }
    return CssBase;
#else
    (void)name;
    return NULL;
#endif
}

void *dvdp_dlsym(void *handle, const char *name)
{
#ifdef __amigaos__
    if (!handle || handle != CssBase)
        return NULL;
    if (!strcmp(name, "dvdcss_open_stream"))
        return (void *)css_open_stream;
    if (!strcmp(name, "dvdcss_open"))
        return (void *)css_open;
    if (!strcmp(name, "dvdcss_close"))
        return (void *)css_close;
    if (!strcmp(name, "dvdcss_seek"))
        return (void *)css_seek;
    if (!strcmp(name, "dvdcss_read"))
        return (void *)css_read;
#else
    (void)handle;
    (void)name;
#endif
    return NULL;                            /* (dvdcss_crack, DVD-Audio's: none) */
}

int dvdp_dlclose(void *handle)
{
    (void)handle;                           /* (kept: dvdp_css_end) */
    return 0;
}

void dvdp_css_drop(struct dvdp_ctx *ctx)
{
#ifdef __amigaos__
    if (!ctx || !ctx->css)
        return;
    ctx->css = 0;
    if (css_users > 0 && !--css_users && CssBase) {
        CloseLibrary(CssBase);
        CssBase = NULL;
    }
#else
    (void)ctx;
#endif
}

void dvdp_css_end(void)
{
#ifdef __amigaos__
    if (CssBase)
        CloseLibrary(CssBase);
    CssBase = NULL;
    css_users = 0;
#endif
}

const char *dvdp_dlerror(void)
{
    return "not available";
}

/* libdvdread's file system (file/file_posix.c, not built): used only for
 * paths; every call fails. */
#include "dvdread/dvd_filesystem.h"

static void fs_close(dvd_reader_filesystem_h *fs)
{
    dvdp_free(fs);
}

static int fs_stat(dvd_reader_filesystem_h *fs, const char *path, dvdstat_t *st)
{
    (void)fs;
    (void)path;
    (void)st;
    return -1;
}

static void *fs_dir_open(dvd_reader_filesystem_h *fs, const char *name)
{
    (void)fs;
    (void)name;
    return NULL;
}

static int fs_dir_read(void *dir, dvd_dirent_t *e)
{
    (void)dir;
    (void)e;
    return -1;
}

static void fs_dir_close(void *dir)
{
    (void)dir;
}

static void *fs_file_open(dvd_reader_filesystem_h *fs, const char *name)
{
    (void)fs;
    (void)name;
    return NULL;
}

static ssize_t fs_file_read(void *f, char *buf, size_t n)
{
    (void)f;
    (void)buf;
    (void)n;
    return -1;
}

static off64_t fs_file_seek(void *f, off64_t off, int whence)
{
    (void)f;
    (void)off;
    (void)whence;
    return -1;
}

static int fs_file_close(void *f)
{
    (void)f;
    return -1;
}

dvd_reader_filesystem_h *InitInternalFilesystem(void)
{
    dvd_reader_filesystem_h *fs = dvdp_calloc(1, sizeof(*fs));
    if (!fs)
        return NULL;
    fs->close = fs_close;
    fs->stat = fs_stat;
    fs->dir_open = fs_dir_open;
    fs->dir_read = fs_dir_read;
    fs->dir_close = fs_dir_close;
    fs->file_open = fs_file_open;
    fs->file_read = fs_file_read;
    fs->file_seek = fs_file_seek;
    fs->file_close = fs_file_close;
    return fs;
}
