/* Written in the form sfdc 1.13 makes from dvdnav_lib.sfd. MIT, (c) 2026 Brian King. */

#ifndef _INLINE_DVDNAV_H
#define _INLINE_DVDNAV_H

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif /* !__INLINE_MACROS_H */

#ifndef DVDNAV_BASE_NAME
#define DVDNAV_BASE_NAME DvdNavBase
#endif /* !DVDNAV_BASE_NAME */

#define DvdOpen(___reader, ___flags) \
      LP2(0x1e, APTR, DvdOpen , struct DvdReader *, ___reader, a0, ULONG, ___flags, d0,\
      , DVDNAV_BASE_NAME)

#define DvdClose(___dvd) \
      LP1NR(0x24, DvdClose , APTR, ___dvd, a0,\
      , DVDNAV_BASE_NAME)

#define DvdNextBlock(___dvd, ___buf, ___event) \
      LP3(0x2a, LONG, DvdNextBlock , APTR, ___dvd, a0, APTR, ___buf, a1, struct DvdEvent *, ___event, a2,\
      , DVDNAV_BASE_NAME)

#define DvdButton(___dvd, ___action, ___button) \
      LP3(0x30, LONG, DvdButton , APTR, ___dvd, a0, LONG, ___action, d0, LONG, ___button, d1,\
      , DVDNAV_BASE_NAME)

#define DvdPoint(___dvd, ___x, ___y, ___activate) \
      LP4(0x36, LONG, DvdPoint , APTR, ___dvd, a0, LONG, ___x, d0, LONG, ___y, d1, LONG, ___activate, d2,\
      , DVDNAV_BASE_NAME)

#define DvdHighlight(___dvd, ___highlight, ___mode) \
      LP3(0x3c, LONG, DvdHighlight , APTR, ___dvd, a0, struct DvdHighlight *, ___highlight, a1, LONG, ___mode, d0,\
      , DVDNAV_BASE_NAME)

#define DvdMenu(___dvd, ___menu) \
      LP2(0x42, LONG, DvdMenu , APTR, ___dvd, a0, LONG, ___menu, d0,\
      , DVDNAV_BASE_NAME)

#define DvdPlay(___dvd, ___title, ___chapter) \
      LP3(0x48, LONG, DvdPlay , APTR, ___dvd, a0, LONG, ___title, d0, LONG, ___chapter, d1,\
      , DVDNAV_BASE_NAME)

#define DvdChapter(___dvd, ___how) \
      LP2(0x4e, LONG, DvdChapter , APTR, ___dvd, a0, LONG, ___how, d0,\
      , DVDNAV_BASE_NAME)

#define DvdSeekTime(___dvd, ___ms) \
      LP2(0x54, LONG, DvdSeekTime , APTR, ___dvd, a0, ULONG, ___ms, d0,\
      , DVDNAV_BASE_NAME)

#define DvdSkip(___dvd, ___what) \
      LP2(0x5a, LONG, DvdSkip , APTR, ___dvd, a0, LONG, ___what, d0,\
      , DVDNAV_BASE_NAME)

#define DvdGetInfo(___dvd, ___info) \
      LP2(0x60, LONG, DvdGetInfo , APTR, ___dvd, a0, struct DvdInfo *, ___info, a1,\
      , DVDNAV_BASE_NAME)

#define DvdStreamInfo(___dvd, ___type, ___stream, ___info) \
      LP4(0x66, LONG, DvdStreamInfo , APTR, ___dvd, a0, LONG, ___type, d0, LONG, ___stream, d1, struct DvdStreamInfo *, ___info, a1,\
      , DVDNAV_BASE_NAME)

#define DvdSetStream(___dvd, ___type, ___stream) \
      LP3(0x6c, LONG, DvdSetStream , APTR, ___dvd, a0, LONG, ___type, d0, LONG, ___stream, d1,\
      , DVDNAV_BASE_NAME)

#define DvdSetLanguage(___dvd, ___type, ___lang) \
      LP3(0x72, LONG, DvdSetLanguage , APTR, ___dvd, a0, LONG, ___type, d0, CONST_STRPTR, ___lang, a1,\
      , DVDNAV_BASE_NAME)

#define DvdSetRegion(___dvd, ___mask) \
      LP2(0x78, LONG, DvdSetRegion , APTR, ___dvd, a0, ULONG, ___mask, d0,\
      , DVDNAV_BASE_NAME)

#define DvdAngle(___dvd, ___angle) \
      LP2(0x7e, LONG, DvdAngle , APTR, ___dvd, a0, LONG, ___angle, d0,\
      , DVDNAV_BASE_NAME)

#define DvdError(___dvd) \
      LP1(0x84, CONST_STRPTR, DvdError , APTR, ___dvd, a0,\
      , DVDNAV_BASE_NAME)

#define DvdVersion() \
      LP0(0x8a, CONST_STRPTR, DvdVersion ,\
      , DVDNAV_BASE_NAME)

#define DvdTitleLength(___dvd, ___title, ___ms) \
      LP3(0x90, LONG, DvdTitleLength , APTR, ___dvd, a0, LONG, ___title, d0, ULONG *, ___ms, a1,\
      , DVDNAV_BASE_NAME)

#define DvdMenus(___dvd) \
      LP1(0x96, ULONG, DvdMenus , APTR, ___dvd, a0,\
      , DVDNAV_BASE_NAME)

#endif /* !_INLINE_DVDNAV_H */
