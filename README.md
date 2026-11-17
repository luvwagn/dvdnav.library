# dvdnav.library

DVD-Video navigation for any Amiga program: VideoLAN's **libdvdread** and
**libdvdnav**, the libraries VLC, mpv and Kodi play DVDs with, built as an
Exec library behind a small Amiga interface. The program reads the disc
(a drive's sectors, or an image file) through a reader it gives the
library; the library reads the disc's file system and IFOs, runs the DVD's
virtual machine (menus, buttons, titles, chapters, angles, still frames)
and hands back the MPEG-2 Program Stream one 2048-byte block at a time,
with events for what the player must do (flush, still, palette, streams,
highlight, jumps). Decoding and drawing are the program's.

Made for Ambiance, an Amiga Media Center application, which opens it at
run time to play DVDs and DVD images. It was called `amcedvd.library`
there (1.0 to 1.6), when Ambiance was AmigaMCE.

- 68020 or better; no FPU needed.
- libdvdread 7.1.1 and libdvdnav 7.0.0 (`third_party/`), as released,
  changed only as `third_party/CHANGES.upstream` says: a 64-bit seek (a DVD
  passes 2 GB; `off_t` is 32 bits on AmigaOS) and integer random numbers
  (no floating point in the library).
- Its interface (`include/libraries/dvdnav.h`, `dvdnav_lib.sfd`) is the
  library's own, not libdvdnav's C API: a program needs no libdvdnav or
  libdvdread header.
- An optional `cssdecrypt.library` (libdvdcss, for CSS-scrambled discs) is
  opened where libdvdread would load libdvdcss, from the program's `Libs`
  or `LIBS:`, if it is there. It is not part of this library or its
  archive; without it, discs are read as they are.

## Using it

```c
#include <proto/dvdnav.h>
struct Library *DvdNavBase = OpenLibrary(DVDNAVLIB_NAME, DVDNAVLIB_VERSION);

struct DvdReader rd = { handle, my_seek, my_read, my_log };  /* the disc, a sector at a time */
APTR dvd = DvdOpen(&rd, 0);
struct DvdEvent ev;
UBYTE block[DVD_BLOCK];
LONG e;
while ((e = DvdNextBlock(dvd, block, &ev)) != DVDE_STOP && e != DVDE_ERROR) {
    if (e == DVDE_BLOCK)
        ;   /* a 2048-byte Program Stream pack: to the decoder */
    else if (e == DVDE_STILL)
        DvdSkip(dvd, DVDSKIP_STILL);    /* (or after the still's time) */
    /* DVDE_HIGHLIGHT, DVDE_CLUT, DVDE_AUDIO_STREAM...: include/libraries/dvdnav.h */
}
DvdClose(dvd);
```

Menus: `DvdButton` (up, down, left, right, activate), `DvdPoint` (the
mouse), `DvdHighlight` (a button's rectangle and colours), `DvdMenu`.
Playing: `DvdPlay`, `DvdChapter`, `DvdSeekTime`, `DvdAngle`, `DvdSetStream`,
`DvdSetLanguage`, `DvdSetRegion`. About the disc: `DvdGetInfo`,
`DvdStreamInfo`, `DvdTitleLength`, `DvdMenus`. `include/libraries/dvdnav.h`
describes each call, structure and event.

The library runs on the caller's stack, about 30 KB deep when a title is
opened (libdvdread's UDF lookup keeps sector buffers there): call it from a
process with 48 KB. One task uses a handle at a time; several handles may
be open. Memory comes from exec; libdvd's messages go to the reader's
`log` callback.

## Licence

GPL-3.0-or-later (`LICENSE`): libdvdread and libdvdnav are GPL-2.0-or-later
(their `COPYING` files in `third_party/`). The interface
(`include/`, `dvdnav_lib.sfd`) is MIT and contains nothing of libdvdread's
or libdvdnav's, so any program may be built against it; a program that
opens the library at run time is not bound by its licence. The Aminet
archive carries the library as built; this repository is its source.

## Building

```
make            build/dvdnav.library
make test       tests/dvd_test.c on a small DVD-Video image (tests/mkdvd.sh
                made it: a root menu, a still with two buttons, two
                titles): on this computer linked with the library's
                sources, then through dvdnav.library on a 68020 under vamos
make lha        build/dvdnav.lha for Aminet
```

Needs [AmigaPorts' m68k-amigaos-gcc](https://github.com/AmigaPorts/m68k-amigaos-gcc)
16.2 (rc24 or later; the continuation of bebbo's amiga-gcc) with libnix, a
C compiler and Python 3 on the build computer, and for the tests amitools'
vamos.

## Known

- libdvdnav 7.0.0 reads past a program chain's cells on some deliberately
  corrupted IFOs (found by fuzzing the test disc); on an Amiga that reads
  stray memory rather than stopping. Real discs with read errors fail to
  open or end with `DVDE_ERROR` instead.
- The read-ahead cache is off unless `DVDF_READAHEAD` is given (it takes up
  to 1 MB at a time).

## Credits

libdvdread and libdvdnav: the VideoLAN project and their authors
(`third_party/libdvdread/AUTHORS`, `third_party/libdvdnav/AUTHORS`), from
the MPlayer and Ogle projects before them.
