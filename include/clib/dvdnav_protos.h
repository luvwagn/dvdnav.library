/* Written in the form sfdc 1.13 makes from dvdnav_lib.sfd. MIT, (c) 2026 Brian King. */

#ifndef CLIB_DVDNAV_PROTOS_H
#define CLIB_DVDNAV_PROTOS_H

#include <exec/types.h>
#include <libraries/dvdnav.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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
/* 1.2 */
ULONG DvdMenus(APTR dvd);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CLIB_DVDNAV_PROTOS_H */
