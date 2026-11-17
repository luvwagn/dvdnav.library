/*
 * dvdnav.library: DVD-Video navigation for any Amiga program.
 *
 * libdvdread 7.1.1 and libdvdnav 7.0.0 (VideoLAN, third_party/) behind a
 * small Amiga interface of its own (include/libraries/dvdnav.h,
 * dvdnav_lib.sfd). The opener reads the disc (a drive's sectors, or an
 * image file) through its DvdReader; this library runs the DVD's virtual
 * machine and hands back the Program Stream a block at a time, with
 * events for still frames, stream and palette changes, highlights and
 * jumps. Decoding and drawing are the opener's.
 *
 * Inside a library, libdvd's C library calls go to port.c (see
 * port/amiport.h): memory from exec, printing into the opener's log, no
 * files. Every call here sets the opener's context first (dvdp_cur), so
 * what libdvd prints reaches the right log, and an abort() inside libdvd
 * returns here as an error instead of stopping the program. One task
 * uses a handle at a time; several handles may be open.
 *
 * The library's data is shared by its openers (libnix's libinit,
 * src/libinit.c). The caller's stack carries libdvd's calls, about
 * 30 KB deep when a title is opened (libdvdread's UDF lookup keeps
 * sector buffers there): give the calling process 48 KB.
 *
 * (c) 2026 Brian King. dvdnav.library is free software under the GNU
 * General Public License, version 3 or (at your option) any later version
 * (LICENSE); libdvdread and libdvdnav inside are GPL-2.0-or-later (their
 * COPYING files in third_party/). Its interface (include/, dvdnav_lib.sfd)
 * is MIT: see README.md.
 */
#include "port.h"
#undef read                                 /* amiport.h's, for libdvd; here the names are ours */
#undef open
#undef close
#undef lseek
#include <dvdnav/dvdnav.h>
#include <dvdread/ifo_read.h>
#include "vm/decoder.h"                     /* (DvdMenus: the VM's disc reader) */
#include "vm/vm.h"
#include "dvdnav_internal.h"
#include <libraries/dvdnav.h>

#ifdef __amigaos__
#include <exec/libraries.h>
#include <proto/exec.h>
#include "stabs.h"

const char LibName[] = "dvdnav.library";
const char LibIdString[] = "dvdnav 1.7 (17.11.2026) libdvdnav 7.0.0, libdvdread 7.1.1";
static const char __attribute__((used)) LibVerTag[] =
    "$VER: dvdnav.library 1.7 (17.11.2026) libdvdnav 7.0.0, libdvdread 7.1.1";
const UWORD LibVersion = 1;
const UWORD LibRevision = 7;   /* CHANGES: 1.0 to 1.6 as amcedvd.library; 1.7: dvdnav.library */

struct ExecBase *SysBase;
int dvdp_timer_open(void);                  /* amitime.c */
void dvdp_timer_close(void);
#define API
#else
#define API                                 /* on the host: plain functions (tests/dvd_test.c) */
#endif

static const char version[] = "dvdnav 1.7, libdvdnav 7.0.0, libdvdread 7.1.1";

struct Dvd {
    struct dvdp_ctx ctx;                    /* first: port.c's view */
    struct DvdReader rd;
    ULONG flags;
    dvdnav_t *nav;
    dvdnav_stream_cb scb;                   /* libdvdread keeps a pointer to it */
    dvdnav_logger_cb lcb;
    jmp_buf jb;
    int dead;                               /* an abort() inside libdvd: only DvdClose now */
    char err[160];
};

static char open_err[160];                  /* DvdError(NULL): why DvdOpen failed */

/* ---- entering and leaving: the opener's context for libdvd's printing
 * and abort() (port.c). A setjmp must be in the function that returns,
 * so these are macros. */
#define ENTER(d, fail)                                  \
    struct dvdp_ctx *prev_ctx = dvdp_cur;               \
    if (!(d) || (d)->dead)                              \
        return fail;                                    \
    dvdp_cur = &(d)->ctx;                               \
    (d)->ctx.jb = &(d)->jb;                             \
    if (setjmp((d)->jb)) {                              \
        (d)->dead = 1;                                  \
        strcpy((d)->err, "internal error in libdvd");   \
        dvdp_cur = prev_ctx;                            \
        return fail;                                    \
    }
#define LEAVE(d) (dvdp_cur = prev_ctx)

static void ctx_log(struct dvdp_ctx *ctx, int level, const char *line)
{
    struct Dvd *d = (struct Dvd *)ctx;
    if (level >= DVDLOG_DEBUG && !(d->flags & DVDF_DEBUG))
        return;
    if (d->rd.log)
        d->rd.log(d->rd.handle, level, (CONST_STRPTR)line);
}

/* libdvdnav's (and through it libdvdread's) messages */
static void nav_log(void *priv, dvdnav_logger_level_t level, const char *fmt, va_list ap)
{
    struct Dvd *d = priv;
    char buf[200];
    int l = level == DVDNAV_LOGGER_LEVEL_ERROR ? DVDLOG_ERROR :
            level == DVDNAV_LOGGER_LEVEL_WARN ? DVDLOG_WARN :
            level == DVDNAV_LOGGER_LEVEL_INFO ? DVDLOG_INFO : DVDLOG_DEBUG;
    dvdp_vsnprintf(buf, sizeof(buf), fmt, ap);
    /* opened as a stream there is no device to read the disc's name from:
     * libdvdnav says so as an error, but it is expected (DvdGetInfo reads
     * the name from the volume instead) */
    if (!strcmp(buf, "Device name string NULL") || strstr(buf, "dvd_read_name failed"))
        l = DVDLOG_DEBUG;
    if (l == DVDLOG_ERROR) {
        strncpy(d->err, buf, sizeof(d->err) - 1);
        d->err[sizeof(d->err) - 1] = 0;
    }
    ctx_log(&d->ctx, l, buf);
}

/* ---- the opener's reader, as libdvdread's stream: bytes there, sectors
 * here. libdvdread only ever asks for whole sectors. */
static int stream_seek(void *priv, uint64_t pos)
{
    struct Dvd *d = priv;
    if (pos & (DVD_BLOCK - 1))
        return -1;
    return d->rd.seek(d->rd.handle, (ULONG)(pos >> 11)) == 0 ? 0 : -1;
}

static int stream_read(void *priv, void *buf, int bytes)
{
    struct Dvd *d = priv;
    LONG n;
    if (bytes <= 0 || (bytes & (DVD_BLOCK - 1)))
        return -1;
    n = d->rd.read(d->rd.handle, buf, bytes >> 11);
    return n < 0 ? -1 : (int)(n << 11);
}

/* ok()'s, and DVD_JUMPED if the VM jumped since its hop channel was h */
static LONG ok_jumped(struct Dvd *d, dvdnav_status_t st, int32_t h);

static LONG ok(struct Dvd *d, dvdnav_status_t st)
{
    if (st != DVDNAV_STATUS_OK) {
        const char *e = dvdnav_err_to_string(d->nav);
        strncpy(d->err, e ? e : "failed", sizeof(d->err) - 1);
        d->err[sizeof(d->err) - 1] = 0;
        return FALSE;
    }
    return TRUE;
}

/* ---- the calls, in the .sfd's order */

API APTR DvdOpen(struct DvdReader *reader, ULONG flags)
{
    struct Dvd *d;
    struct dvdp_ctx *prev_ctx = dvdp_cur;
    open_err[0] = 0;
    if (!reader || !reader->seek || !reader->read) {
        strcpy(open_err, "no reader");
        return NULL;
    }
    if (!(d = dvdp_calloc(1, sizeof(*d)))) {
        strcpy(open_err, "out of memory");
        return NULL;
    }
    d->rd = *reader;
    d->flags = flags;
    d->ctx.log = ctx_log;
    d->ctx.jb = &d->jb;
    d->scb.pf_seek = stream_seek;
    d->scb.pf_read = stream_read;
    d->lcb.pf_log = nav_log;
    dvdp_cur = &d->ctx;
    if (setjmp(d->jb)) {
        strcpy(open_err, "internal error in libdvd while opening");
        dvdp_cur = prev_ctx;
        dvdp_css_drop(&d->ctx);
        dvdp_free(d);                       /* what libdvd held is lost: it may be inconsistent */
        return NULL;
    }
    if (dvdnav_open_stream2(&d->nav, d, &d->lcb, &d->scb) != DVDNAV_STATUS_OK) {
        strcpy(open_err, d->err[0] ? d->err : "not a DVD-Video disc");
        d->nav = NULL;
        dvdp_cur = prev_ctx;
        dvdp_css_drop(&d->ctx);
        dvdp_free(d);
        return NULL;
    }
    dvdnav_set_readahead_flag(d->nav, (flags & DVDF_READAHEAD) ? 1 : 0);
    dvdnav_set_region_mask(d->nav, 0xff);   /* a player of every region */
    /* times and places in the whole title (its PGC), not its current
     * chapter's: DvdSeekTime to a time past the chapter playing failed
     * ("Error when seeking"), and dvdnav_get_current_time is the title's */
    dvdnav_set_PGC_positioning_flag(d->nav, 1);
    dvdnav_menu_language_select(d->nav, "en");
    dvdnav_audio_language_select(d->nav, "en");
    dvdnav_spu_language_select(d->nav, "en");
    dvdp_cur = prev_ctx;
    return d;
}

API VOID DvdClose(APTR dvd)
{
    struct Dvd *d = dvd;
    struct dvdp_ctx *prev_ctx = dvdp_cur;
    if (!d)
        return;
    if (!d->dead && d->nav) {
        dvdp_cur = &d->ctx;
        if (!setjmp(d->jb))
            dvdnav_close(d->nav);
        dvdp_cur = prev_ctx;
    }
    dvdp_css_drop(&d->ctx);                 /* (cssdecrypt.library, if it held it) */
    dvdp_free(d);
}

API LONG DvdNextBlock(APTR dvd, APTR buf, struct DvdEvent *ev)
{
    struct Dvd *d = dvd;
    int32_t event = DVDNAV_NOP, len = 0;
    LONG r;
    ENTER(d, DVDE_ERROR);
    if (dvdnav_get_next_block(d->nav, buf, &event, &len) != DVDNAV_STATUS_OK) {
        ok(d, DVDNAV_STATUS_ERR);
        LEAVE(d);
        return DVDE_ERROR;
    }
    switch (event) {
    case DVDNAV_BLOCK_OK:
        r = DVDE_BLOCK;
        break;
    case DVDNAV_NAV_PACKET:
        r = DVDE_NAV;
        break;
    case DVDNAV_STILL_FRAME: {
        dvdnav_still_event_t e;
        memcpy(&e, buf, sizeof(e));         /* the event comes in buf, maybe unaligned */
        ev->still = e.length;
        r = DVDE_STILL;
        break;
    }
    case DVDNAV_SPU_STREAM_CHANGE: {
        dvdnav_spu_stream_change_event_t e;
        memcpy(&e, buf, sizeof(e));
        ev->spu_wide = e.physical_wide;
        ev->spu_letterbox = e.physical_letterbox;
        ev->spu_panscan = e.physical_pan_scan;
        ev->spu_logical = e.logical;
        r = DVDE_SPU_STREAM;
        break;
    }
    case DVDNAV_AUDIO_STREAM_CHANGE: {
        dvdnav_audio_stream_change_event_t e;
        memcpy(&e, buf, sizeof(e));
        ev->audio = e.physical;
        ev->audio_logical = e.logical;
        r = DVDE_AUDIO_STREAM;
        break;
    }
    case DVDNAV_VTS_CHANGE: {
        dvdnav_vts_change_event_t e;
        memcpy(&e, buf, sizeof(e));
        ev->domain = e.new_domain;
        ev->vts = e.new_vtsN;
        r = DVDE_DOMAIN;
        break;
    }
    case DVDNAV_CELL_CHANGE: {
        dvdnav_cell_change_event_t e;
        memcpy(&e, buf, sizeof(e));
        ev->cell = e.cellN;
        ev->program = e.pgN;
        ev->cell_sectors = (ULONG)e.cell_length;
        ev->cell_start = (ULONG)e.cell_start;
        ev->chain_ms = (ULONG)(e.pgc_length / 90);
        r = DVDE_CELL;
        break;
    }
    case DVDNAV_STOP:
        r = DVDE_STOP;
        break;
    case DVDNAV_HIGHLIGHT: {
        dvdnav_highlight_event_t e;
        memcpy(&e, buf, sizeof(e));
        ev->button = (LONG)e.buttonN;
        ev->show = e.display ? 1 : 0;
        r = DVDE_HIGHLIGHT;
        break;
    }
    case DVDNAV_SPU_CLUT_CHANGE:
        memcpy(ev->clut, buf, sizeof(ev->clut));    /* 16 uint32_t, 0x00YYCrCb */
        r = DVDE_CLUT;
        break;
    case DVDNAV_HOP_CHANNEL:
        r = DVDE_HOP;
        break;
    case DVDNAV_WAIT:
        r = DVDE_WAIT;
        break;
    default:
        r = DVDE_NOP;
        break;
    }
    LEAVE(d);
    return r;
}

static LONG ok_jumped(struct Dvd *d, dvdnav_status_t st, int32_t h)
{
    LONG r = ok(d, st);
    return r && d->nav->vm && d->nav->vm->hop_channel != h ? DVD_JUMPED : r;
}

API LONG DvdButton(APTR dvd, LONG action, LONG button)
{
    struct Dvd *d = dvd;
    pci_t *pci;
    dvdnav_status_t st;
    int32_t h;
    LONG r;
    ENTER(d, FALSE);
    h = d->nav->vm ? d->nav->vm->hop_channel : 0;
    pci = dvdnav_get_current_nav_pci(d->nav);
    if (!pci) {
        strcpy(d->err, "no menu");
        LEAVE(d);
        return FALSE;
    }
    switch (action) {
    case DVDB_UP:
        st = dvdnav_upper_button_select(d->nav, pci);
        break;
    case DVDB_DOWN:
        st = dvdnav_lower_button_select(d->nav, pci);
        break;
    case DVDB_LEFT:
        st = dvdnav_left_button_select(d->nav, pci);
        break;
    case DVDB_RIGHT:
        st = dvdnav_right_button_select(d->nav, pci);
        break;
    case DVDB_ACTIVATE:
        st = dvdnav_button_activate(d->nav, pci);
        break;
    case DVDB_SELECT:
        st = dvdnav_button_select(d->nav, pci, button);
        break;
    case DVDB_PRESS:
        st = dvdnav_button_select_and_activate(d->nav, pci, button);
        break;
    default:
        strcpy(d->err, "unknown button action");
        LEAVE(d);
        return FALSE;
    }
    r = ok_jumped(d, st, h);
    LEAVE(d);
    return r;
}

API LONG DvdPoint(APTR dvd, LONG x, LONG y, LONG activate)
{
    struct Dvd *d = dvd;
    pci_t *pci;
    int32_t h;
    LONG r;
    ENTER(d, FALSE);
    pci = dvdnav_get_current_nav_pci(d->nav);
    if (!pci) {
        strcpy(d->err, "no menu");
        LEAVE(d);
        return FALSE;
    }
    h = d->nav->vm ? d->nav->vm->hop_channel : 0;
    r = ok_jumped(d, activate ? dvdnav_mouse_activate(d->nav, pci, x, y) :
                                dvdnav_mouse_select(d->nav, pci, x, y), h);
    LEAVE(d);
    return r;
}

/* The selected button's rectangle and colours: its number, or 0 when
 * there is no highlight to draw now. */
API LONG DvdHighlight(APTR dvd, struct DvdHighlight *hl, LONG mode)
{
    struct Dvd *d = dvd;
    pci_t *pci;
    int32_t button = 0;
    dvdnav_highlight_area_t a;
    int i;
    ENTER(d, 0);
    memset(hl, 0, sizeof(*hl));
    pci = dvdnav_get_current_nav_pci(d->nav);
    if (!pci || dvdnav_get_current_highlight(d->nav, &button) != DVDNAV_STATUS_OK || button <= 0 ||
        dvdnav_get_highlight_area(pci, button, mode == DVDH_ACTIVATED ? 1 : 0, &a) != DVDNAV_STATUS_OK) {
        LEAVE(d);
        return 0;
    }
    hl->button = button;
    hl->x0 = a.sx;
    hl->y0 = a.sy;
    hl->x1 = a.ex;
    hl->y1 = a.ey;
    hl->pts = a.pts;
    /* BTN_COLI: colours of emphasis 2, emphasis 1, pattern, background
     * (bits 31-16), then their contrasts (15-0); pixel values 3, 2, 1, 0 */
    for (i = 0; i < 4; i++) {
        hl->colour[i] = (UBYTE)((a.palette >> (16 + 4 * i)) & 15);
        hl->alpha[i] = (UBYTE)((a.palette >> (4 * i)) & 15);
    }
    LEAVE(d);
    return button;
}

API LONG DvdMenu(APTR dvd, LONG menu)
{
    struct Dvd *d = dvd;
    int32_t h;
    LONG r;
    ENTER(d, FALSE);
    h = d->nav->vm ? d->nav->vm->hop_channel : 0;
    r = ok_jumped(d, dvdnav_menu_call(d->nav, (DVDMenuID_t)menu), h);
    LEAVE(d);
    return r;
}

API LONG DvdPlay(APTR dvd, LONG title, LONG chapter)
{
    struct Dvd *d = dvd;
    LONG r;
    ENTER(d, FALSE);
    r = ok(d, chapter > 0 ? dvdnav_part_play(d->nav, title, chapter) :
                            dvdnav_title_play(d->nav, title));
    LEAVE(d);
    return r;
}

API LONG DvdChapter(APTR dvd, LONG how)
{
    struct Dvd *d = dvd;
    dvdnav_status_t st;
    ENTER(d, FALSE);
    switch (how) {
    case DVDC_NEXT:
        st = dvdnav_next_pg_search(d->nav);
        break;
    case DVDC_PREV:
        st = dvdnav_prev_pg_search(d->nav);
        break;
    case DVDC_TOP:
        st = dvdnav_top_pg_search(d->nav);
        break;
    case DVDC_UP:
        st = dvdnav_go_up(d->nav);
        break;
    default:
        strcpy(d->err, "unknown chapter move");
        LEAVE(d);
        return FALSE;
    }
    st = ok(d, st);
    LEAVE(d);
    return st;
}

API LONG DvdSeekTime(APTR dvd, ULONG ms)
{
    struct Dvd *d = dvd;
    LONG r;
    ENTER(d, FALSE);
    r = ok(d, dvdnav_time_search(d->nav, (uint64_t)ms * 90));
    LEAVE(d);
    return r;
}

API LONG DvdSkip(APTR dvd, LONG what)
{
    struct Dvd *d = dvd;
    LONG r;
    ENTER(d, FALSE);
    r = ok(d, what == DVDSKIP_WAIT ? dvdnav_wait_skip(d->nav) : dvdnav_still_skip(d->nav));
    LEAVE(d);
    return r;
}

static ULONG forbidden_bits(user_ops_t u)
{
    ULONG f = 0;
    if (u.title_or_time_play) f |= DVDU_TITLE_OR_TIME_PLAY;
    if (u.chapter_search_or_play) f |= DVDU_CHAPTER_PLAY;
    if (u.title_play) f |= DVDU_TITLE_PLAY;
    if (u.stop) f |= DVDU_STOP;
    if (u.go_up) f |= DVDU_GO_UP;
    if (u.time_or_chapter_search) f |= DVDU_TIME_OR_CHAPTER_SEEK;
    if (u.prev_or_top_pg_search) f |= DVDU_PREV_CHAPTER;
    if (u.next_pg_search) f |= DVDU_NEXT_CHAPTER;
    if (u.forward_scan) f |= DVDU_FORWARD_SCAN;
    if (u.backward_scan) f |= DVDU_BACKWARD_SCAN;
    if (u.title_menu_call) f |= DVDU_TITLE_MENU;
    if (u.root_menu_call) f |= DVDU_ROOT_MENU;
    if (u.subpic_menu_call) f |= DVDU_SUBTITLE_MENU;
    if (u.audio_menu_call) f |= DVDU_AUDIO_MENU;
    if (u.angle_menu_call) f |= DVDU_ANGLE_MENU;
    if (u.chapter_menu_call) f |= DVDU_CHAPTER_MENU;
    if (u.resume) f |= DVDU_RESUME;
    if (u.button_select_or_activate) f |= DVDU_BUTTON;
    if (u.still_off) f |= DVDU_STILL_OFF;
    if (u.pause_on) f |= DVDU_PAUSE;
    if (u.audio_stream_change) f |= DVDU_AUDIO_CHANGE;
    if (u.subpic_stream_change) f |= DVDU_SUBTITLE_CHANGE;
    if (u.angle_change) f |= DVDU_ANGLE_CHANGE;
    return f;
}

API LONG DvdGetInfo(APTR dvd, struct DvdInfo *info)
{
    struct Dvd *d = dvd;
    int32_t n = 0, title = 0, part = 0, angle = 0, angles = 0;
    uint32_t pos = 0, len = 0, w = 0, h = 0;
    const char *name = NULL;
    int64_t t;
    ENTER(d, FALSE);
    memset(info, 0, sizeof(*info));
    info->domain = dvdnav_is_domain_vts(d->nav) ? DVDD_TITLE :
                   dvdnav_is_domain_vtsm(d->nav) ? DVDD_MENU :
                   dvdnav_is_domain_vmgm(d->nav) ? DVDD_ROOTMENU :
                   dvdnav_is_domain_fp(d->nav) ? DVDD_FIRSTPLAY : 0;
    if (dvdnav_get_number_of_titles(d->nav, &n) == DVDNAV_STATUS_OK)
        info->titles = n;
    if (dvdnav_current_title_info(d->nav, &title, &part) == DVDNAV_STATUS_OK && title > 0) {
        info->title = title;
        info->chapter = part;
        if (dvdnav_get_number_of_parts(d->nav, title, &n) == DVDNAV_STATUS_OK)
            info->chapters = n;
    }
    if (dvdnav_get_angle_info(d->nav, &angle, &angles) == DVDNAV_STATUS_OK) {
        info->angle = angle;
        info->angles = angles;
    }
    if (dvdnav_get_position(d->nav, &pos, &len) == DVDNAV_STATUS_OK) {
        info->sector = pos;
        info->sectors = len;
    }
    t = dvdnav_get_current_time(d->nav);
    info->ms = t > 0 ? (ULONG)(t / 90) : 0;
    if (dvdnav_get_video_resolution(d->nav, &w, &h) == DVDNAV_STATUS_OK) {
        info->width = (LONG)w;
        info->height = (LONG)h;
    }
    info->wide = dvdnav_get_video_aspect(d->nav) != 0;
    info->audio = dvdnav_get_active_audio_stream(d->nav);
    info->spu = dvdnav_get_active_spu_stream(d->nav);
    info->audio_count = dvdnav_get_number_of_streams(d->nav, DVD_AUDIO_STREAM);
    info->spu_count = dvdnav_get_number_of_streams(d->nav, DVD_SUBTITLE_STREAM);
    if (dvdnav_get_title_string(d->nav, &name) == DVDNAV_STATUS_OK && name && name[0]) {
        strncpy(info->name, name, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = 0;
    } else {
        char *vol = dvdnav_get_volid_string(d->nav);  /* UDF or ISO 9660 volume name */
        if (vol) {
            strncpy(info->name, vol, sizeof(info->name) - 1);
            info->name[sizeof(info->name) - 1] = 0;
            dvdp_free(vol);
        }
    }
    info->forbidden = forbidden_bits(dvdnav_get_restrictions(d->nav));
    {
        /* (1.6) no previous chapter to go to: the first of its program
         * chain, which has none before it (dvdnav_prev_pg_search would
         * fail) */
        vm_t *vm = d->nav->vm;
        if (vm && vm->state.pgc && vm->state.pgN <= 1 && !vm->state.pgc->prev_pgc_nr)
            info->forbidden |= DVDU_PREV_CHAPTER;
    }
    LEAVE(d);
    return TRUE;
}

API LONG DvdStreamInfo(APTR dvd, LONG type, LONG stream, struct DvdStreamInfo *si)
{
    struct Dvd *d = dvd;
    uint16_t lang;
    int p;
    ENTER(d, FALSE);
    memset(si, 0, sizeof(*si));
    si->physical = -1;
    if (stream < 0 || stream > 31 || (type == DVDS_AUDIO && stream > 7)) {
        LEAVE(d);
        return FALSE;
    }
    /* (libdvdnav's "*_logical_stream" calls take the DVD's own stream
     * number and give the MPEG one, whatever their header says) */
    if (type == DVDS_AUDIO) {
        lang = dvdnav_audio_stream_to_lang(d->nav, (uint8_t)stream);
        si->format = dvdnav_audio_stream_format(d->nav, (uint8_t)stream);
        si->channels = dvdnav_audio_stream_channels(d->nav, (uint8_t)stream);
        if (si->format == 0xffff)
            si->format = -1;
        if (si->channels == 0xffff)
            si->channels = 0;
        p = si->format >= 0 ? dvdnav_get_audio_logical_stream(d->nav, (uint8_t)stream) : -1;
        si->physical = p >= 0 ? (p & 7) : -1;
    } else {
        lang = dvdnav_spu_stream_to_lang(d->nav, (uint8_t)stream);
        p = lang != 0xffff ? dvdnav_get_spu_logical_stream(d->nav, (uint8_t)stream) : -1;
        si->physical = p >= 0 ? (p & 31) : -1;
    }
    si->present = si->physical >= 0;
    if (lang != 0xffff && lang > 0x00ff) {
        si->lang[0] = (char)(lang >> 8);
        si->lang[1] = (char)lang;
    }
    LEAVE(d);
    return si->present;
}

/* stream -1 for subpictures: hide them */
API LONG DvdSetStream(APTR dvd, LONG type, LONG stream)
{
    struct Dvd *d = dvd;
    LONG r;
    ENTER(d, FALSE);
    if (type == DVDS_SPU && stream < 0)
        r = ok(d, dvdnav_toggle_spu_stream(d->nav, 0));
    else {
        r = ok(d, dvdnav_set_active_stream(d->nav, (uint8_t)stream,
                                          type == DVDS_AUDIO ? DVD_AUDIO_STREAM : DVD_SUBTITLE_STREAM));
        if (r && type == DVDS_SPU)
            dvdnav_toggle_spu_stream(d->nav, 1);
    }
    LEAVE(d);
    return r;
}

API LONG DvdSetLanguage(APTR dvd, LONG type, CONST_STRPTR lang)
{
    struct Dvd *d = dvd;
    char code[3];
    LONG r;
    ENTER(d, FALSE);
    if (!lang || !lang[0] || !lang[1]) {
        strcpy(d->err, "a language is two letters");
        LEAVE(d);
        return FALSE;
    }
    code[0] = (char)dvdp_tolower(lang[0]);
    code[1] = (char)dvdp_tolower(lang[1]);
    code[2] = 0;
    r = ok(d, type == DVDS_MENU ? dvdnav_menu_language_select(d->nav, code) :
              type == DVDS_AUDIO ? dvdnav_audio_language_select(d->nav, code) :
                                   dvdnav_spu_language_select(d->nav, code));
    LEAVE(d);
    return r;
}

API LONG DvdSetRegion(APTR dvd, ULONG mask)
{
    struct Dvd *d = dvd;
    LONG r;
    ENTER(d, FALSE);
    r = ok(d, dvdnav_set_region_mask(d->nav, (int32_t)(mask & 0xff)));
    LEAVE(d);
    return r;
}

API LONG DvdAngle(APTR dvd, LONG angle)
{
    struct Dvd *d = dvd;
    LONG r;
    ENTER(d, FALSE);
    r = ok(d, dvdnav_angle_change(d->nav, angle));
    LEAVE(d);
    return r;
}

API CONST_STRPTR DvdError(APTR dvd)
{
    struct Dvd *d = dvd;
    return (CONST_STRPTR)(d ? d->err : open_err);
}

/* A title's chapters (0: no such title) and playing time, from its title
 * set's IFO, without playing it: what the main film is, before choosing. */
API LONG DvdTitleLength(APTR dvd, LONG title, ULONG *ms)
{
    struct Dvd *d = dvd;
    uint64_t *times = NULL, duration = 0;
    uint32_t parts;
    ENTER(d, 0);
    *ms = 0;
    parts = dvdnav_describe_title_chapters(d->nav, title, &times, &duration);
    dvdp_free(times);
    if (parts)
        *ms = (ULONG)(duration / 90);
    else
        strcpy(d->err, "no such title");
    LEAVE(d);
    return (LONG)parts;
}

/* Which menus the disc has (DVDMB_ bits), from the menu tables of its
 * video manager and title sets (each "menu exists" flag of its first
 * language): 0 for none, a disc that only plays. */
static ULONG menu_bits(const ifo_handle_t *ifo, int vmg)
{
    ULONG b = 0;
    int i;
    if (!ifo || !ifo->pgci_ut || !ifo->pgci_ut->lu)
        return 0;
    for (i = 0; i < ifo->pgci_ut->nr_of_lus; i++) {
        UBYTE e = ifo->pgci_ut->lu[i].exists;
        if (vmg)
            b |= e & 0x80 ? DVDMB_TITLE : 0;
        else
            b |= (e & 0x80 ? DVDMB_ROOT : 0) | (e & 0x08 ? DVDMB_CHAPTERS : 0) |
                 (e & 0x70 ? DVDMB_OTHER : 0);
    }
    return b;
}

API ULONG DvdMenus(APTR dvd)
{
    struct Dvd *d = dvd;
    dvd_reader_t *r;
    ifo_handle_t *vmg;
    ULONG b = 0;
    int t, n;
    ENTER(d, 0);
    if (!(r = vm_get_dvd_reader(d->nav->vm)) || !(vmg = ifoOpen(r, 0))) {
        strcpy(d->err, "no video manager");
        LEAVE(d);
        return 0;
    }
    b = menu_bits(vmg, 1);
    n = vmg->vmgi_mat ? vmg->vmgi_mat->vmg_nr_of_title_sets : 0;
    for (t = 1; t <= n && t <= 99; t++) {
        ifo_handle_t *v = ifoOpen(r, t);
        if (v) {
            b |= menu_bits(v, 0);
            ifoClose(v);
        }
    }
    ifoClose(vmg);
    LEAVE(d);
    return b;
}

API CONST_STRPTR DvdVersion(void)
{
    return (CONST_STRPTR)version;
}

#ifdef __amigaos__
/* ---- the library */
int __UserLibInit(struct Library *lib)
{
    (void)lib;
    SysBase = *(struct ExecBase *volatile *)4L;
    dvdp_timer_open();                      /* without it the clock reads 0: still fine */
    return 0;
}

void __UserLibCleanup(void)
{
    dvdp_css_end();
    dvdp_timer_close();
}

/* the jump table, in the .sfd's order (from bias 30) */
ADDTABL_2(DvdOpen, a0, d0);
ADDTABL_1(DvdClose, a0);
ADDTABL_3(DvdNextBlock, a0, a1, a2);
ADDTABL_3(DvdButton, a0, d0, d1);
ADDTABL_4(DvdPoint, a0, d0, d1, d2);
ADDTABL_3(DvdHighlight, a0, a1, d0);
ADDTABL_2(DvdMenu, a0, d0);
ADDTABL_3(DvdPlay, a0, d0, d1);
ADDTABL_2(DvdChapter, a0, d0);
ADDTABL_2(DvdSeekTime, a0, d0);
ADDTABL_2(DvdSkip, a0, d0);
ADDTABL_2(DvdGetInfo, a0, a1);
ADDTABL_4(DvdStreamInfo, a0, d0, d1, a1);
ADDTABL_3(DvdSetStream, a0, d0, d1);
ADDTABL_3(DvdSetLanguage, a0, d0, a1);
ADDTABL_2(DvdSetRegion, a0, d0);
ADDTABL_2(DvdAngle, a0, d0);
ADDTABL_1(DvdError, a0);
ADDTABL_0(DvdVersion);
ADDTABL_3(DvdTitleLength, a0, d0, a1);
ADDTABL_1(DvdMenus, a0);
ADDTABL_END();
#endif
