/*
 * "$Id: config.h,v 1.5.2.5 2003/11/05 19:11:54 mike Exp $"
 *
 *   Configuration file for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products.
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
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * Compiler stuff...
 */

#undef const
#undef __CHAR_UNSIGNED__


/*
 * Version of software...
 */

#define CUPS_SVERSION	"CUPS v1.2.0b1"


/*
 * Default user and group...
 */

#define CUPS_DEFAULT_USER	"lp"
#define CUPS_DEFAULT_GROUP	"sys"


/*
 * Default IPP port...
 */

#define CUPS_DEFAULT_IPP_PORT 631


/*
 * Maximum number of file descriptors to support.
 */

#define CUPS_MAX_FDS		4096


/*
 * Where are files stored?
 */

#define CUPS_LOCALEDIR	"C:/CUPS/locale"
#define CUPS_SERVERROOT	"C:/CUPS/etc"
#define CUPS_SERVERBIN	"C:/CUPS"
#define CUPS_DOCROOT	"C:/CUPS/share/doc"
#define CUPS_REQUESTS	"C:/CUPS/spool"
#define CUPS_LOGDIR	"C:/CUPS/logs"
#define CUPS_DATADIR    "C:/CUPS/share"
#define CUPS_FONTPATH	"C:/CUPS/share/fonts"


/*
 * What is the format string for strftime?
 */

#define CUPS_STRFTIME_FORMAT	"%c"


/*
 * Do we have various image libraries?
 */

#undef HAVE_LIBPNG
#undef HAVE_LIBZ
#undef HAVE_LIBJPEG
#undef HAVE_LIBTIFF


/*
 * Does this machine store words in big-endian (MSB-first) order?
 */

#undef WORDS_BIGENDIAN


/*
 * Which directory functions and headers do we use?
 */

#undef HAVE_DIRENT_H
#undef HAVE_SYS_DIR_H
#undef HAVE_SYS_NDIR_H
#undef HAVE_NDIR_H

/*
 * Do we have PAM stuff?
 */

#ifndef HAVE_LIBPAM
#define HAVE_LIBPAM 0
#endif /* !HAVE_LIBPAM */

#undef HAVE_PAM_PAM_APPL_H


/*
 * Do we have <shadow.h>?
 */

#undef HAVE_SHADOW_H


/*
 * Do we have <crypt.h>?
 */

#undef HAVE_CRYPT_H


/*
 * How about standard C header files?
 */

#undef HAVE_STDDEF_H
#define HAVE_STDLIB_H


/*
 * Use <string.h>, <strings.h>, and/or <bstring.h>?
 */

#define HAVE_STRING_H
#undef HAVE_STRINGS_H
#undef HAVE_BSTRING_H


/*
 * Do we have the strXXX() functions?
 */

#define HAVE_STRDUP
#define HAVE_STRCASECMP
#define HAVE_STRNCASECMP
#undef HAVE_STRLCAT
#undef HAVE_STRLCPY


/*
 * Do we have the vsyslog() function?
 */

#undef HAVE_VSYSLOG


/*
 * Do we have the (v)snprintf() functions?
 */

#undef HAVE_SNPRINTF
#undef HAVE_VSNPRINTF


/*
 * What signal functions to use?
 */

#undef HAVE_SIGSET
#undef HAVE_SIGACTION

/*
 * What wait functions to use?
 */

#undef HAVE_WAITPID
#undef HAVE_WAIT3


/*
 * Do we have the mallinfo function and malloc.h?
 */

#undef HAVE_MALLINFO
#undef HAVE_MALLOC_H


/*
 * Which encryption libraries do we have?
 */

#undef HAVE_CDSASSL
#undef HAVE_GNUTLS
#undef HAVE_LIBSSL
#undef HAVE_SSL


/*
 * Do we have the OpenSLP library?
 */

#undef HAVE_LIBSLP


/*
 * Do we have libpaper?
 */

#undef HAVE_LIBPAPER


/*
 * Do we have <sys/ioctl.h>?
 */

#undef HAVE_SYS_IOCTL_H


/*
 * Do we have mkstemp() and/or mkstemps()?
 */

#undef HAVE_MKSTEMP
#undef HAVE_MKSTEMPS


/*
 * Does the "tm" structure contain the "tm_gmtoff" member?
 */

#undef HAVE_TM_GMTOFF


/*
 * Do we have rresvport()?
 */

#undef HAVE_RRESVPORT


/*
 * Do we have getifaddrs()?
 */

#undef HAVE_GETIFADDRS


/*
 * Do we have hstrerror()?
 */

#undef HAVE_HSTRERROR


/*
 * Do we have the <sys/sockio.h> header file?
 */

#undef HAVE_SYS_SOCKIO_H


/*
 * Does the sockaddr structure contain an sa_len parameter?
 */

#undef HAVE_STRUCT_SOCKADDR_SA_LEN


/*
 * Various scripting languages...
 */

#undef HAVE_JAVA
#define CUPS_JAVA	"/usr/bin/java"
#undef HAVE_PERL
#define CUPS_PERL	"/usr/bin/perl"
#undef HAVE_PHP
#define CUPS_PHP	"/usr/bin/php"
#undef HAVE_PYTHON
#define CUPS_PYTHON	"/usr/bin/python"


/*
 * End of "$Id: config.h,v 1.5.2.5 2003/11/05 19:11:54 mike Exp $".
 */
