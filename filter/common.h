/*
 * "$Id$"
 *
 *   Common filter definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include <cups/cups.h>
#include <cups/language.h>
#include <cups/string.h>


/*
 * C++ magic...
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*
 * Globals...
 */

extern int	Orientation,	/* 0 = portrait, 1 = landscape, etc. */
		Duplex,		/* Duplexed? */
		LanguageLevel,	/* Language level of printer */
		ColorDevice;	/* Do color text? */
extern float	PageLeft,	/* Left margin */
		PageRight,	/* Right margin */
		PageBottom,	/* Bottom margin */
		PageTop,	/* Top margin */
		PageWidth,	/* Total page width */
		PageLength;	/* Total page length */


/*
 * Prototypes...
 */

extern ppd_file_t *SetCommonOptions(int num_options, cups_option_t *options,
		                    int change_size);
extern void	UpdatePageVars(void);
extern void	WriteCommon(void);
extern void	WriteLabelProlog(const char *label, float bottom,
		                 float top, float width);
extern void	WriteLabels(int orient);


/*
 * C++ magic...
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */


/*
 * End of "$Id$".
 */
