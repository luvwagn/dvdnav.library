#ifndef LIBRARIES_DVDNAV_H
#define LIBRARIES_DVDNAV_H
/*
 * dvdnav.library: DVD-Video navigation for any Amiga program
 * (libdvdread and libdvdnav inside; https://github.com/luvwagn/dvdnav.library).
 *
 * The library reads a DVD (through the caller's own sector reader), runs
 * the DVD's virtual machine (menus, buttons, titles, chapters, angles,
 * still frames) and hands back the MPEG-2 Program Stream one 2048-byte
 * block at a time, for a decoder elsewhere (the ZZ9000's media session).
 *
 * The library runs on the caller's stack, about 30 KB deep when a title
 * is opened: call it from a process with 48 KB. One task uses a handle at
 * a time.
 *
 * This header is the library's own Amiga interface, not libdvdnav's C
 * API: no libdvdnav or libdvdread header is needed to use the library,
 * and none is included here.
 *
 * (c) 2026 Brian King. This header, dvdnav_lib.sfd and the clib, inline
 * and proto headers made from it are under the MIT licence, so a program
 * under any licence can include them. The library itself (libdvdread and
 * libdvdnav inside) is GPL-3.0-or-later: see README.md.
 */

#include <exec/types.h>

#define DVDNAVLIB_NAME    "dvdnav.library"
#define DVDNAVLIB_VERSION 1   /* (DVDNAVLIB_: libdvdnav's own version.h has DVDNAV_VERSION) */

#define DVD_BLOCK 2048                      /* bytes in a DVD sector */

/*
 * The caller's reader. The library calls these on the caller's own task,
 * from inside its calls (DvdOpen, DvdNextBlock, DvdPlay...), with their
 * arguments on the stack (GCC's usual calling convention); no register
 * or small-data setup is needed.
 *
 *   seek: go to a sector. 0 for done, -1 for an error.
 *   read: read that many 2048-byte sectors into buf, from where the last
 *         seek or read left off. The number read (0 at the end of the
 *         disc or image), or -1 for an error.
 *   log:  optional (NULL for none): a line from the library, without a
 *         newline, at a DVDLOG_ level.
 */
struct DvdReader {
    APTR handle;                            /* passed back to each call */
    LONG (*seek)(APTR handle, ULONG sector);
    LONG (*read)(APTR handle, APTR buf, LONG sectors);
    void (*log)(APTR handle, LONG level, CONST_STRPTR line);
};

#define DVDLOG_ERROR 0
#define DVDLOG_WARN  1
#define DVDLOG_INFO  2
#define DVDLOG_DEBUG 3

/* DvdOpen flags */
#define DVDF_READAHEAD 1    /* libdvdnav's read-ahead cache: up to 1 MB per chunk, so off by default */
#define DVDF_DEBUG     2    /* pass DVDLOG_DEBUG lines to log() too */

/* DvdNextBlock's events */
#define DVDE_ERROR       -1 /* DvdError() says why */
#define DVDE_BLOCK        0 /* buf holds one Program Stream block: to the decoder */
#define DVDE_NOP          1 /* nothing: call again */
#define DVDE_STILL        2 /* ev->still: show the last picture for that many seconds
                             * (DVD_STILL_FOREVER: until DvdSkip(DVDSKIP_STILL)),
                             * then DvdSkip(DVDSKIP_STILL). Only once the decoder has
                             * shown everything it was given. */
#define DVDE_SPU_STREAM   3 /* ev->spu_*: the subpicture streams to decode now */
#define DVDE_AUDIO_STREAM 4 /* ev->audio: the audio stream to decode now */
#define DVDE_DOMAIN       5 /* ev->domain, ev->vts: a new title set or domain (menu/title) */
#define DVDE_CELL         6 /* ev->cell...: a new cell; times and lengths */
#define DVDE_NAV          7 /* buf holds a NAV pack (private stream 2): the decoder may skip it */
#define DVDE_STOP         8 /* the end: nothing more will play */
#define DVDE_HIGHLIGHT    9 /* ev->button, ev->show: the highlighted button changed; DvdHighlight() */
#define DVDE_CLUT        10 /* ev->clut: the subpicture palette changed */
#define DVDE_HOP         12 /* a jump: flush the decoder, drop what is queued */
#define DVDE_WAIT        13 /* let the decoder play out what it has, then DvdSkip(DVDSKIP_WAIT) */

#define DVD_STILL_FOREVER 255

/* domains (ev->domain, DvdInfo.domain) */
#define DVDD_FIRSTPLAY 1
#define DVDD_TITLE     2    /* a title playing */
#define DVDD_ROOTMENU  4    /* the disc's own menus (video manager) */
#define DVDD_MENU      8    /* a title set's menus */

struct DvdEvent {
    LONG  still;            /* DVDE_STILL: seconds, or DVD_STILL_FOREVER */
    LONG  audio;            /* DVDE_AUDIO_STREAM: MPEG stream (0-7: AC-3 0x80+n, MPEG 0xC0+n,
                             * LPCM 0xA0+n, DTS 0x88+n by format), -1 none */
    LONG  audio_logical;    /*   the DVD's own stream number */
    LONG  spu_wide;         /* DVDE_SPU_STREAM: subpicture stream (0-31, 0x20+n in
                             * private stream 1) for 16:9, letterboxed and pan & scan */
    LONG  spu_letterbox;
    LONG  spu_panscan;
    LONG  spu_logical;      /*   the DVD's own stream number; bit 7 set: shown only when forced */
    LONG  domain;           /* DVDE_DOMAIN: DVDD_ */
    LONG  vts;              /*   title set */
    LONG  cell;             /* DVDE_CELL: cell number in the program chain */
    LONG  program;          /*   program (chapter) number */
    ULONG cell_sectors;     /*   the cell's length */
    ULONG cell_start;       /*   its start, in sectors from the chain's start */
    ULONG chain_ms;         /*   the program chain's playing time */
    LONG  button;           /* DVDE_HIGHLIGHT: the button now selected (1-36), 0 none */
    LONG  show;             /*   1 show the highlight, 0 hide it */
    ULONG clut[16];         /* DVDE_CLUT: 0x00YYCrCb, as on the disc */
};

/* DvdButton, DvdPoint and DvdMenu return FALSE (DvdError says why), TRUE,
 * or (1.5) DVD_JUMPED: the disc jumps elsewhere, as the DVDE_HOP that the
 * next DvdNextBlock gives will say (the player may drop what it holds at
 * once) */
#define DVD_JUMPED    2

/* DvdButton actions */
#define DVDB_UP       1
#define DVDB_DOWN     2
#define DVDB_LEFT     3
#define DVDB_RIGHT    4
#define DVDB_ACTIVATE 5     /* the selected button */
#define DVDB_SELECT   6     /* button n */
#define DVDB_PRESS    7     /* button n, selected and activated */

/* DvdMenu menus */
#define DVDM_ESCAPE    0    /* from a title: its root menu; from a menu: back to the title */
#define DVDM_TITLE     2    /* the disc's title menu */
#define DVDM_ROOT      3
#define DVDM_SUBTITLES 4
#define DVDM_AUDIO     5
#define DVDM_ANGLE     6
#define DVDM_CHAPTERS  7

/* DvdChapter */
#define DVDC_NEXT  1
#define DVDC_PREV  2
#define DVDC_TOP   3        /* the start of the current chapter */
#define DVDC_UP    4        /* the program chain's "go up" (a menu's parent) */

/* DvdSkip */
#define DVDSKIP_STILL 1
#define DVDSKIP_WAIT  2

/* DvdHighlight modes */
#define DVDH_SELECTED  0    /* the colours of a selected button */
#define DVDH_ACTIVATED 1    /* the colours as it is pressed */

struct DvdHighlight {
    LONG  button;           /* 1-36; 0 when there is none */
    UWORD x0, y0, x1, y1;   /* its rectangle, in the video's pixels (720 x 576 or 480), inclusive */
    UBYTE colour[4];        /* subpicture pixel value 0-3 -> CLUT index 0-15 */
    UBYTE alpha[4];         /*   and its opacity, 0 clear - 15 solid */
    ULONG pts;              /* from this presentation time (90 kHz) */
};

/* DvdStreamInfo, DvdSetStream, DvdSetLanguage types */
#define DVDS_AUDIO 0
#define DVDS_SPU   1
#define DVDS_MENU  2        /* DvdSetLanguage only: the menus' language */

/* audio formats */
#define DVDA_AC3   0
#define DVDA_MPEG1 2
#define DVDA_MPEG2 3
#define DVDA_LPCM  4
#define DVDA_DTS   6
#define DVDA_SDDS  7

struct DvdStreamInfo {
    LONG  present;          /* FALSE: no such stream in this title */
    char  lang[3];          /* "en", or "" when not given */
    UBYTE pad;
    LONG  format;           /* audio: DVDA_ */
    LONG  channels;         /* audio */
    LONG  physical;         /* the MPEG stream number, as in DvdEvent */
};

struct DvdInfo {
    LONG  domain;           /* DVDD_ */
    LONG  titles;           /* on the disc */
    LONG  title;            /* playing, 0 in a menu */
    LONG  chapters;         /* in that title */
    LONG  chapter;
    LONG  angles, angle;
    ULONG sector, sectors;  /* position and length within the title */
    ULONG ms;               /* playing time so far in the title */
    LONG  width, height;    /* the video's size */
    LONG  wide;             /* TRUE: 16:9 */
    LONG  audio, spu;       /* active streams (the DVD's numbers), -1 none */
    LONG  audio_count, spu_count;
    char  name[48];         /* the disc's title, from its volume, may be empty */
    ULONG forbidden;        /* what the disc forbids now: DVDU_ bits (1.6: and
                             * DVDU_PREV_CHAPTER where there is no chapter before) */
};

/* DvdInfo.forbidden: the user operations the disc does not allow at this point */
#define DVDU_TITLE_OR_TIME_PLAY   0x00000001
#define DVDU_CHAPTER_PLAY         0x00000002
#define DVDU_TITLE_PLAY           0x00000004
#define DVDU_STOP                 0x00000008
#define DVDU_GO_UP                0x00000010
#define DVDU_TIME_OR_CHAPTER_SEEK 0x00000020
#define DVDU_PREV_CHAPTER         0x00000040
#define DVDU_NEXT_CHAPTER         0x00000080
#define DVDU_FORWARD_SCAN         0x00000100
#define DVDU_BACKWARD_SCAN        0x00000200
#define DVDU_TITLE_MENU           0x00000400
#define DVDU_ROOT_MENU            0x00000800
#define DVDU_SUBTITLE_MENU        0x00001000
#define DVDU_AUDIO_MENU           0x00002000
#define DVDU_ANGLE_MENU           0x00004000
#define DVDU_CHAPTER_MENU         0x00008000
#define DVDU_RESUME               0x00010000
#define DVDU_BUTTON               0x00020000
#define DVDU_STILL_OFF            0x00040000
#define DVDU_PAUSE                0x00080000
#define DVDU_AUDIO_CHANGE         0x00100000
#define DVDU_SUBTITLE_CHANGE      0x00200000
#define DVDU_ANGLE_CHANGE         0x00400000

/* DvdMenus (1.2): the menus the disc has, from its IFOs' menu tables
 * (0: none, as a disc of programmes or a film alone) */
#define DVDMB_TITLE   0x01  /* the disc's title menu (video manager) */
#define DVDMB_ROOT    0x02  /* a title set's root menu */
#define DVDMB_CHAPTERS 0x04 /* a chapter menu */
#define DVDMB_OTHER   0x08  /* sound, subtitle or angle menus */

#endif /* LIBRARIES_DVDNAV_H */
