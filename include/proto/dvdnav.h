/* Written in the form sfdc 1.13 makes from dvdnav_lib.sfd. MIT, (c) 2026 Brian King. */

#ifndef PROTO_DVDNAV_H
#define PROTO_DVDNAV_H

#include <clib/dvdnav_protos.h>

#if defined(_CONST_BASES)
# ifndef __CONSTLIBBASEDECL__
# define __CONSTLIBBASEDECL__ const
# endif /* __CONSTLIBBASEDECL__ */
# ifndef __SEGMENTLIBBASEDECL__
# define __SEGMENTLIBBASEDECL__  __attribute__((__section__(".data")))
# endif /* __SEGMENTLIBBASEDECL__ */
#endif /* _CONST_BASES */
#ifdef __amigaos4__
# include <interfaces/dvdnav.h>
# ifndef __NOGLOBALIFACE__
   extern struct DvdNavIFace *IDvdNav;
# endif /* __NOGLOBALIFACE__*/
#endif /* !__amigaos4__ */
#ifndef __NOLIBBASE__
  extern struct Library *
# ifdef __CONSTLIBBASEDECL__
   __CONSTLIBBASEDECL__
# endif /* __CONSTLIBBASEDECL__ */
  DvdNavBase
# ifdef __SEGMENTLIBBASEDECL__
 __SEGMENTLIBBASEDECL__
# endif /* __SEGMENTLIBBASEDECL__ */
;
#endif /* !__NOLIBBASE__ */

#ifndef _NO_INLINE
# if defined(__GNUC__)
#  ifdef __AROS__
#   include <defines/dvdnav.h>
#  else
#   include <inline/dvdnav.h>
#  endif
# else
#  include <pragmas/dvdnav_pragmas.h>
# endif
#endif /* _NO_INLINE */

#endif /* !PROTO_DVDNAV_H */
