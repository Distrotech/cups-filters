/*
 * "$Id: mark.c,v 1.2 1998/06/12 20:33:20 mike Exp $"
 *
 *   Option marking routines for the PostScript Printer Description (PPD) file
 *   library.
 *
 *   Copyright 1997-1998 by Easy Software Products.
 *
 *       Easy Software Products
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   PostScript is a trademark of Adobe Systems, Inc.
 *
 *   This library is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU Library General Public License as published
 *   by the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 *   USA.
 *
 * Contents:
 *
 *   ppdMarkDefaults() - Mark all default options in the PPD file.
 *   ppdMarkOption()   - Mark an option in a PPD file.
 *
 * Revision History:
 *
 *   $Log: mark.c,v $
 *   Revision 1.2  1998/06/12 20:33:20  mike
 *   First working version.
 *
 *   Revision 1.1  1998/06/11 18:35:02  mike
 *   Initial revision
 */

/*
 * Include necessary headers...
 */

#include "ppd.h"


/*
 * 'ppdMarkDefaults()' - Mark all default options in the PPD file.
 */

void
ppdMarkDefaults(ppd_file_t *ppd)	/* I - PPD file record */
{
}


/*
 * 'ppdMarkOption()' - Mark an option in a PPD file.
 *
 * Notes:
 *
 *   -1 is returned if the given option would conflict with any currently
 *   selected option.
 */

int					/* O - 0 on success, -1 on failure */
ppdMarkOption(ppd_file_t *ppd,		/* I - PPD file record */
              char       *keyword,	/* I - Keyword */
              char       *option)	/* I - Option name */
{
  return (0);
}


/*
 * End of "$Id: mark.c,v 1.2 1998/06/12 20:33:20 mike Exp $".
 */
