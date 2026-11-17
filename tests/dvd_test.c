/*
 * dvdnav.library's tests, on a small DVD-Video image made for them
 * (tests/mkdvd.sh: a root menu, a still with two buttons, and two titles,
 * the first with two chapters).
 *
 * The same file runs on the host, linked with the library's sources
 * (make test-dvd-host), and on a 68020 under vamos through the library
 * itself (make test-dvd): dvd_test <image>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libraries/dvdnav.h>

#ifdef __amigaos__
#include <proto/exec.h>
#include <proto/dvdnav.h>
#ifdef DVD_TEST_DVDDISC
#include "dvddisc.h"                        /* (a program's own reader, tested too: AmigaMCE's) */
#endif
struct Library *DvdNavBase;
#else
APTR DvdOpen(struct DvdReader *reader, ULONG flags);
VOID DvdClose(APTR dvd);
LONG DvdNextBlock(APTR dvd, APTR buf, struct DvdEvent *event);
LONG DvdButton(APTR dvd, LONG action, LONG button);
LONG DvdPoint(APTR dvd, LONG x, LONG y, LONG activate);
LONG DvdHighlight(APTR dvd, struct DvdHighlight *highlight, LONG mode);
LONG DvdMenu(APTR dvd, LONG menu);
LONG DvdPlay(APTR dvd, LONG title, LONG chapter);
LONG DvdChapter(APTR dvd, LONG how);
LONG DvdSeekTime(APTR dvd, ULONG ms);
LONG DvdSkip(APTR dvd, LONG what);
LONG DvdGetInfo(APTR dvd, struct DvdInfo *info);
LONG DvdStreamInfo(APTR dvd, LONG type, LONG stream, struct DvdStreamInfo *info);
LONG DvdSetStream(APTR dvd, LONG type, LONG stream);
LONG DvdSetLanguage(APTR dvd, LONG type, CONST_STRPTR lang);
LONG DvdSetRegion(APTR dvd, ULONG mask);
LONG DvdAngle(APTR dvd, LONG angle);
CONST_STRPTR DvdError(APTR dvd);
CONST_STRPTR DvdVersion(void);
LONG DvdTitleLength(APTR dvd, LONG title, ULONG *ms);
ULONG DvdMenus(APTR dvd);
unsigned long dvdp_memory(int peak);
#endif

static int failed, passed, verbose;

#define CHECK(c, ...)                                   \
    do {                                                \
        if (c)                                          \
            passed++;                                   \
        else {                                          \
            failed++;                                   \
            printf("FAIL line %d: ", __LINE__);         \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

/* ---- the reader: an image file, counted */
struct Image {
    FILE *f;
    ULONG pos, sectors;
    ULONG reads, seeks, max_sector;
    int zeros;                  /* a reader of zeros (not a DVD) */
};

static LONG img_seek(APTR h, ULONG sector)
{
    struct Image *im = h;
    im->seeks++;
    if (sector > im->sectors)
        return -1;
    im->pos = sector;
    if (!im->zeros && fseek(im->f, (long)sector * DVD_BLOCK, SEEK_SET))
        return -1;
    return 0;
}

static LONG img_read(APTR h, APTR buf, LONG n)
{
    struct Image *im = h;
    LONG got;
    im->reads++;
    if (im->pos + n > im->sectors)
        n = (LONG)(im->sectors - im->pos);
    if (n <= 0)
        return 0;
    if (im->zeros) {
        memset(buf, 0, (size_t)n * DVD_BLOCK);
        got = n;
    } else
        got = (LONG)fread(buf, DVD_BLOCK, (size_t)n, im->f);
    if (im->pos + got - 1 > im->max_sector)
        im->max_sector = im->pos + got - 1;
    im->pos += got;
    return got;
}

static int log_lines, log_errors;

static void img_log(APTR h, LONG level, CONST_STRPTR line)
{
    (void)h;
    log_lines++;
    if (level == DVDLOG_ERROR)
        log_errors++;
    if (verbose || level <= DVDLOG_WARN)
        printf("  log %ld: %s\n", (long)level, (const char *)line);
}

/* ---- running the disc: events counted until one of 'until' (a bit per
 * event) or the limit; still frames are skipped unless 'until' has them */
struct Run {
    LONG last;
    ULONG count[16];
    ULONG blocks, nav, video_packs, audio_packs, spu_packs;
    struct DvdEvent ev;         /* the last of each kind's fields */
    LONG domain, vts, still, button, show, audio, spu_wide;
    ULONG clut0;
};

static UBYTE block[DVD_BLOCK];

static LONG run(APTR dvd, struct Run *r, ULONG until, ULONG limit)
{
    struct DvdEvent ev;
    memset(r, 0, sizeof(*r));
    r->domain = r->still = r->button = r->audio = r->spu_wide = -1;
    while (limit--) {
        LONG e = DvdNextBlock(dvd, block, &ev);
        r->last = e;
        if (e < 0)
            return e;
        if (e < 16)
            r->count[e]++;
        switch (e) {
        case DVDE_BLOCK: {
            UBYTE id;
            r->blocks++;
            /* pack header, then the first PES packet's stream id */
            if (block[0] == 0 && block[1] == 0 && block[2] == 1 && block[3] == 0xBA) {
                UBYTE *p = block + 14 + (block[13] & 7);
                id = p[3];
                if (id == 0xE0)
                    r->video_packs++;
                else if (id == 0xBD) {
                    UBYTE sub = p[9 + p[8]];
                    if (sub >= 0x20 && sub < 0x40)
                        r->spu_packs++;
                    else
                        r->audio_packs++;
                } else if (id >= 0xC0 && id < 0xE0)
                    r->audio_packs++;
            }
            break;
        }
        case DVDE_NAV:
            r->nav++;
            break;
        case DVDE_DOMAIN:
            r->domain = ev.domain;
            r->vts = ev.vts;
            break;
        case DVDE_STILL:
            r->still = ev.still;
            break;
        case DVDE_HIGHLIGHT:
            r->button = ev.button;
            r->show = ev.show;
            break;
        case DVDE_AUDIO_STREAM:
            r->audio = ev.audio;
            break;
        case DVDE_SPU_STREAM:
            r->spu_wide = ev.spu_wide;
            break;
        case DVDE_CLUT:
            r->clut0 = ev.clut[0];
            break;
        case DVDE_WAIT:
            DvdSkip(dvd, DVDSKIP_WAIT);
            break;
        }
        r->ev = ev;
        if (e < 32 && (until & (1UL << e)))
            return e;
        if (e == DVDE_STILL)
            DvdSkip(dvd, DVDSKIP_STILL);
    }
    return -2;
}

#define UNTIL(e) (1UL << (e))

int main(int argc, char **argv)
{
    struct Image im, zero;
    struct DvdReader rd;
    struct DvdInfo info;
    struct DvdHighlight hl;
    struct DvdStreamInfo si;
    struct Run r;
    APTR dvd;
    LONG e;
    long size;

    if (argc < 2) {
        printf("usage: dvd_test image [v]\n");
        return 20;
    }
    verbose = argc > 2;
#ifdef __amigaos__
    DvdNavBase = OpenLibrary((CONST_STRPTR)"PROGDIR:Libs/dvdnav.library", 1);
    if (!DvdNavBase)
        DvdNavBase = OpenLibrary((CONST_STRPTR)"dvdnav.library", 1);
    if (!DvdNavBase) {
        printf("FAIL: no dvdnav.library\n");
        return 20;
    }
#endif
    printf("%s\n", (const char *)DvdVersion());

    memset(&im, 0, sizeof(im));
    if (!(im.f = fopen(argv[1], "rb"))) {
        printf("FAIL: cannot open %s\n", argv[1]);
        return 20;
    }
    fseek(im.f, 0, SEEK_END);
    size = ftell(im.f);
    im.sectors = (ULONG)(size / DVD_BLOCK);
    rd.handle = &im;
    rd.seek = img_seek;
    rd.read = img_read;
    rd.log = img_log;

    /* not a DVD: no handle, and a reason */
    memset(&zero, 0, sizeof(zero));
    zero.zeros = 1;
    zero.sectors = 1000;
    rd.handle = &zero;
    dvd = DvdOpen(&rd, 0);
    CHECK(!dvd, "a disc of zeros opened");
    CHECK(DvdError(NULL)[0], "no reason given for a failed open");
    printf("not a DVD: %s\n", (const char *)DvdError(NULL));
    if (dvd)
        DvdClose(dvd);
    rd.handle = &im;
    log_errors = 0;                         /* those were the zeros' */
    {
        struct DvdReader none;
        memset(&none, 0, sizeof(none));
        CHECK(!DvdOpen(&none, 0), "opened without a reader");
    }

    dvd = DvdOpen(&rd, 0);
    CHECK(dvd != NULL, "open: %s", (const char *)DvdError(NULL));
    if (!dvd)
        goto done;

    CHECK(DvdGetInfo(dvd, &info), "info before playing");
    CHECK(info.titles == 2, "titles %ld", (long)info.titles);
    printf("disc \"%s\": %ld titles\n", info.name, (long)info.titles);
    CHECK(!strcmp(info.name, "AMCE_TEST"), "disc name \"%s\"", info.name);
    {
        ULONG ms1 = 0, ms2 = 0, ms9 = 1;
        LONG c1 = DvdTitleLength(dvd, 1, &ms1), c2 = DvdTitleLength(dvd, 2, &ms2);
        CHECK(c1 == 2 && ms1 >= 3800 && ms1 <= 4200, "title 1: %ld chapters, %lu ms", (long)c1,
              (unsigned long)ms1);
        CHECK(c2 == 1 && ms2 >= 1800 && ms2 <= 2200, "title 2: %ld chapters, %lu ms", (long)c2,
              (unsigned long)ms2);
        CHECK(DvdTitleLength(dvd, 9, &ms9) == 0 && ms9 == 0, "title 9 has a length");
        log_errors = 0;                     /* (title 9's) */
    }
    /* its menus: the video manager's title menu (the root menu of its
     * title set is only a jump back there: not one) */
    {
        ULONG mb = DvdMenus(dvd);
        CHECK(mb & DVDMB_TITLE, "menus %lx", (unsigned long)mb);
    }

    /* first play goes to the root menu: a still with two buttons */
    e = run(dvd, &r, UNTIL(DVDE_STILL), 5000);
    CHECK(e == DVDE_STILL, "menu: ended with %ld, not a still", (long)e);
    CHECK(r.still == DVD_STILL_FOREVER, "menu still %ld", (long)r.still);
    CHECK(r.domain == DVDD_ROOTMENU, "menu domain %ld", (long)r.domain);
    CHECK(r.count[DVDE_CELL] >= 1, "no cell event");
    CHECK(r.count[DVDE_CLUT] >= 1, "no palette");
    CHECK(r.nav >= 1, "no NAV pack");
    CHECK(r.video_packs >= 1, "no video in the menu");
    CHECK(r.spu_packs >= 1, "no subpicture (buttons) in the menu");
    CHECK(r.button == 1, "highlighted button %ld", (long)r.button);
    printf("menu: %lu blocks (%lu video, %lu subpicture), %lu NAV, palette[0] %08lx\n",
           (unsigned long)r.blocks, (unsigned long)r.video_packs, (unsigned long)r.spu_packs,
           (unsigned long)r.nav, (unsigned long)r.clut0);

    CHECK(DvdGetInfo(dvd, &info), "info in the menu");
    CHECK(info.domain == DVDD_ROOTMENU, "info domain %ld", (long)info.domain);
    CHECK(info.title == 0, "title %ld in a menu", (long)info.title);
    CHECK(info.width == 720 && info.height == 576, "size %ldx%ld", (long)info.width, (long)info.height);
    CHECK(!info.wide, "the menu is 4:3");

    CHECK(DvdHighlight(dvd, &hl, DVDH_SELECTED) == 1, "highlight button %ld", (long)hl.button);
    printf("button 1: (%u,%u)-(%u,%u) colours %u %u %u %u alpha %u %u %u %u\n",
           hl.x0, hl.y0, hl.x1, hl.y1, hl.colour[0], hl.colour[1], hl.colour[2], hl.colour[3],
           hl.alpha[0], hl.alpha[1], hl.alpha[2], hl.alpha[3]);
    CHECK(hl.x0 >= 96 && hl.x0 <= 104 && hl.y0 >= 196 && hl.y0 <= 204 &&
          hl.x1 >= 316 && hl.x1 <= 324 && hl.y1 >= 276 && hl.y1 <= 284,
          "button 1 at (%u,%u)-(%u,%u)", hl.x0, hl.y0, hl.x1, hl.y1);
    /* mkdvd.sh sets the button colours to 0xFEDC9876 (selected) and
     * 0x32107654 (activated): colours of pixels 3 2 1 0, then their alpha */
    CHECK(hl.colour[0] == 0xC && hl.colour[1] == 0xD && hl.colour[2] == 0xE && hl.colour[3] == 0xF &&
          hl.alpha[0] == 6 && hl.alpha[1] == 7 && hl.alpha[2] == 8 && hl.alpha[3] == 9,
          "selected colours");

    /* right: button 2 */
    CHECK(DvdButton(dvd, DVDB_RIGHT, 0), "right: %s", (const char *)DvdError(dvd));
    CHECK(DvdHighlight(dvd, &hl, DVDH_SELECTED) == 2, "after right: button %ld", (long)hl.button);
    CHECK(hl.x0 >= 396 && hl.x0 <= 404, "button 2 at x %u", hl.x0);
    CHECK(DvdButton(dvd, DVDB_LEFT, 0), "left");
    CHECK(DvdHighlight(dvd, &hl, DVDH_ACTIVATED) == 1, "after left: button %ld", (long)hl.button);
    CHECK(hl.colour[0] == 0 && hl.colour[3] == 3 && hl.alpha[0] == 4 && hl.alpha[3] == 7,
          "activated colours %u %u, alpha %u %u", hl.colour[0], hl.colour[3], hl.alpha[0], hl.alpha[3]);
    CHECK(DvdButton(dvd, DVDB_SELECT, 2), "select 2");
    CHECK(DvdHighlight(dvd, &hl, DVDH_SELECTED) == 2, "after select 2: button %ld", (long)hl.button);

    /* activate button 2: title 2, then back to the menu at its end */
    CHECK(DvdButton(dvd, DVDB_ACTIVATE, 0), "activate: %s", (const char *)DvdError(dvd));
    e = run(dvd, &r, UNTIL(DVDE_CELL), 5000);
    CHECK(e == DVDE_CELL, "title 2: %ld", (long)e);
    CHECK(r.count[DVDE_HOP] >= 1, "no flush before title 2");
    CHECK(r.domain == DVDD_TITLE, "title 2's domain %ld", (long)r.domain);
    CHECK(DvdGetInfo(dvd, &info), "info in title 2");
    CHECK(info.title == 2 && info.chapters == 1 && info.chapter == 1,
          "title %ld chapter %ld of %ld", (long)info.title, (long)info.chapter, (long)info.chapters);
    CHECK(info.audio_count == 2, "audio streams %ld", (long)info.audio_count);
    CHECK(DvdStreamInfo(dvd, DVDS_AUDIO, 0, &si), "audio 0");
    CHECK(si.format == DVDA_AC3 && si.channels == 6 && si.physical == 0 && !strcmp(si.lang, "en"),
          "audio 0: format %ld channels %ld physical %ld lang '%s'", (long)si.format,
          (long)si.channels, (long)si.physical, si.lang);
    CHECK(DvdStreamInfo(dvd, DVDS_AUDIO, 1, &si), "audio 1");
    CHECK((si.format == DVDA_MPEG1 || si.format == DVDA_MPEG2) && si.channels == 2 &&
          si.physical == 0 && !strcmp(si.lang, "de"),
          "audio 1: format %ld channels %ld physical %ld lang '%s'", (long)si.format,
          (long)si.channels, (long)si.physical, si.lang);
    CHECK(!DvdStreamInfo(dvd, DVDS_AUDIO, 3, &si), "audio 3 is there");
    e = run(dvd, &r, UNTIL(DVDE_STILL), 20000);
    CHECK(e == DVDE_STILL && r.domain == DVDD_ROOTMENU, "back to the menu: %ld, domain %ld",
          (long)e, (long)r.domain);
    CHECK(r.audio == 0, "title 2's audio stream %ld", (long)r.audio);
    CHECK(r.video_packs > 100 && r.audio_packs > 10, "title 2: %lu video, %lu audio packs",
          (unsigned long)r.video_packs, (unsigned long)r.audio_packs);
    printf("title 2: %lu blocks (%lu video, %lu audio), %lu NAV\n", (unsigned long)r.blocks,
           (unsigned long)r.video_packs, (unsigned long)r.audio_packs, (unsigned long)r.nav);

    /* a click on button 1 (its middle): title 1, two chapters */
    CHECK(DvdPoint(dvd, 210, 240, 1), "click: %s", (const char *)DvdError(dvd));
    e = run(dvd, &r, UNTIL(DVDE_CELL), 5000);
    CHECK(DvdGetInfo(dvd, &info) && info.title == 1 && info.chapters == 2 && info.chapter == 1,
          "after the click: title %ld chapter %ld of %ld", (long)info.title, (long)info.chapter,
          (long)info.chapters);
    run(dvd, &r, 0, 40);                    /* into it a little */
    CHECK(DvdChapter(dvd, DVDC_NEXT), "next chapter: %s", (const char *)DvdError(dvd));
    e = run(dvd, &r, UNTIL(DVDE_CELL), 5000);
    CHECK(DvdGetInfo(dvd, &info) && info.title == 1 && info.chapter == 2,
          "next chapter: title %ld chapter %ld", (long)info.title, (long)info.chapter);
    run(dvd, &r, 0, 40);
    CHECK(DvdGetInfo(dvd, &info) && info.ms > 0 && info.sectors > 0,
          "time %lu ms, %lu of %lu sectors", (unsigned long)info.ms, (unsigned long)info.sector,
          (unsigned long)info.sectors);
    printf("title 1, chapter 2: %lu ms, sector %lu of %lu\n", (unsigned long)info.ms,
           (unsigned long)info.sector, (unsigned long)info.sectors);
    CHECK(DvdChapter(dvd, DVDC_PREV), "previous chapter");
    e = run(dvd, &r, UNTIL(DVDE_CELL), 5000);
    CHECK(DvdGetInfo(dvd, &info) && info.chapter == 1, "previous chapter: %ld", (long)info.chapter);

    /* the menu key, then straight to title 1 chapter 2 */
    CHECK(DvdMenu(dvd, DVDM_TITLE), "title menu: %s", (const char *)DvdError(dvd));
    e = run(dvd, &r, UNTIL(DVDE_STILL), 5000);
    CHECK(e == DVDE_STILL && r.domain == DVDD_ROOTMENU, "title menu: %ld domain %ld", (long)e,
          (long)r.domain);
    CHECK(DvdPlay(dvd, 1, 2), "play 1.2: %s", (const char *)DvdError(dvd));
    e = run(dvd, &r, UNTIL(DVDE_CELL), 5000);
    CHECK(DvdGetInfo(dvd, &info) && info.title == 1 && info.chapter == 2,
          "play 1.2: title %ld chapter %ld", (long)info.title, (long)info.chapter);
    CHECK(DvdPlay(dvd, 2, 0), "play 2");
    e = run(dvd, &r, UNTIL(DVDE_CELL), 5000);
    CHECK(DvdGetInfo(dvd, &info) && info.title == 2, "play 2: title %ld", (long)info.title);
    run(dvd, &r, 0, 20);                    /* into it: no seeking in the menu's still */
    CHECK(DvdSeekTime(dvd, 1000), "seek to 1 s: %s", (const char *)DvdError(dvd));
    run(dvd, &r, 0, 20);
    /* to the VOBU (about half a second) holding 1 s */
    CHECK(DvdGetInfo(dvd, &info) && info.ms >= 400 && info.ms <= 1600, "after the seek: %lu ms",
          (unsigned long)info.ms);
    CHECK(!DvdPlay(dvd, 9, 0), "title 9 played");
    CHECK(DvdError(dvd)[0], "no reason for title 9");
    CHECK(DvdSetLanguage(dvd, DVDS_AUDIO, (CONST_STRPTR)"DE"), "audio language");
    CHECK(!DvdSetLanguage(dvd, DVDS_AUDIO, (CONST_STRPTR)"x"), "a one-letter language");
    CHECK(DvdSetRegion(dvd, 2), "region");
    CHECK(DvdSetStream(dvd, DVDS_SPU, -1), "subtitles off");

    DvdClose(dvd);
    printf("reads %lu, seeks %lu, highest sector %lu of %lu\n", (unsigned long)im.reads,
           (unsigned long)im.seeks, (unsigned long)im.max_sector, (unsigned long)im.sectors);
    CHECK(im.max_sector < im.sectors, "read past the end");
#ifndef __amigaos__
    CHECK(dvdp_memory(0) == 0, "%lu bytes not freed", dvdp_memory(0));
    printf("memory: %lu bytes at the most\n", dvdp_memory(1));
#endif
    CHECK(log_errors == 0, "%d errors logged", log_errors);
    DvdClose(NULL);

#ifdef DVD_TEST_DVDDISC
    /* a program's own reader (AmigaMCE's src/dvddisc.c, built with
     * -DDVD_TEST_DVDDISC) on the same image: to the menu, and every sector
     * the same as stdio's */
    {
        char err[100];
        struct DvdDisc *disc = dvdd_open_image(argv[1], err, sizeof(err));
        static UBYTE a[DVD_BLOCK * 3], b[DVD_BLOCK * 3];
        ULONG s;
        int same = 1;
        CHECK(disc != NULL, "dvddisc: %s", err);
        if (disc) {
            CHECK(dvdd_sectors(disc) == im.sectors, "dvddisc: %lu sectors", (unsigned long)dvdd_sectors(disc));
            dvdd_reader(disc, &rd);
            for (s = 0; s + 3 <= im.sectors && same; s += 97) {
                fseek(im.f, (long)s * DVD_BLOCK, SEEK_SET);
                fread(a, DVD_BLOCK, 3, im.f);
                same = rd.seek(rd.handle, s) == 0 && rd.read(rd.handle, b, 3) == 3 &&
                       !memcmp(a, b, sizeof(a));
            }
            CHECK(same, "dvddisc: sector %lu differs", (unsigned long)s);
            CHECK(rd.seek(rd.handle, im.sectors - 1) == 0 && rd.read(rd.handle, b, 3) == 1,
                  "dvddisc: reading past the end");
            rd.seek(rd.handle, 0);
            rd.log = img_log;
            dvd = DvdOpen(&rd, 0);
            CHECK(dvd != NULL, "dvddisc: open: %s", (const char *)DvdError(NULL));
            if (dvd) {
                e = run(dvd, &r, UNTIL(DVDE_STILL), 5000);
                CHECK(e == DVDE_STILL && r.domain == DVDD_ROOTMENU, "dvddisc: menu %ld", (long)e);
                DvdClose(dvd);
            }
            CHECK(dvdd_errors(disc) == 0, "dvddisc: %lu errors", (unsigned long)dvdd_errors(disc));
            dvdd_close(disc);
        }
        CHECK(!dvdd_open_image("nonexistent.iso", err, sizeof(err)) && err[0], "a missing image opened");
    }
#endif

done:
    fclose(im.f);
#ifdef __amigaos__
    CloseLibrary(DvdNavBase);
#endif
    printf("%d passed, %d failed\n", passed, failed);
    if (!failed)
        printf("dvd tests passed\n");
    return failed ? 20 : 0;
}
