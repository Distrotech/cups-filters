/*
 * "$Id: string.h,v 1.7.2.9 2002/06/12 11:35:06 mike Exp $"
 *
 *   String definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_STRING_H_
#  define _CUPS_STRING_H_

/*
 * Include necessary headers...
 */

#  include "config.h"

#  include <stdio.h>
#  include <stdarg.h>
#  include <ctype.h>

#  ifdef HAVE_STRING_H
#    include <string.h>
#  endif /* HAVE_STRING_H */

#  ifdef HAVE_STRINGS_H
#    include <strings.h>
#  endif /* HAVE_STRINGS_H */

#  ifdef HAVE_BSTRING_H
#    include <bstring.h>
#  endif /* HAVE_BSTRING_H */


/*
 * Stuff for WIN32 and OS/2...
 */

#  if defined(WIN32) || defined(__EMX__)
#    define strcasecmp	stricmp
#    define strncasecmp	strnicmp
#  endif /* WIN32 || __EMX__ */


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Prototypes...
 */

#  ifndef HAVE_STRDUP
extern char	*cups_strdup(const char *);
#    define strdup cups_strdup
#  endif /* !HAVE_STRDUP */

#  ifndef HAVE_STRCASECMP
extern int	cups_strcasecmp(const char *, const char *);
#    define strcasecmp cups_strcasecmp
#  endif /* !HAVE_STRCASECMP */

#  ifndef HAVE_STRNCASECMP
extern int	cups_strncasecmp(const char *, const char *, size_t n);
#    define strncasecmp cups_strncasecmp
#  endif /* !HAVE_STRNCASECMP */

#  ifndef HAVE_STRLCAT
extern size_t cups_strlcat(char *, const char *, size_t);
#    define strlcat cups_strlcat
#  endif /* !HAVE_STRLCAT */

#  ifndef HAVE_STRLCPY
extern size_t cups_strlcpy(char *, const char *, size_t);
#    define strlcpy cups_strlcpy
#  endif /* !HAVE_STRLCPY */

#  ifndef HAVE_SNPRINTF
extern int	cups_snprintf(char *, size_t, const char *, ...)
#    ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 3, 4)))
#    endif /* __GNUC__ */
;
#    define snprintf cups_snprintf
#  endif /* !HAVE_SNPRINTF */

#  ifndef HAVE_VSNPRINTF
extern int	cups_vsnprintf(char *, size_t, const char *, va_list);
#    define vsnprintf cups_vsnprintf
#  endif /* !HAVE_VSNPRINTF */


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_STRING_H_ */

/*
 * End of "$Id: string.h,v 1.7.2.9 2002/06/12 11:35:06 mike Exp $".
 */
