/* Copyright (C) 1993 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility to
  anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer to
  the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given to
  you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises is not affiliated with the Free Software Foundation or
  the GNU Project.  GNU Ghostscript, as distributed by Aladdin Enterprises,
  does not depend on any other GNU software.
*/

/* gp_nofb.c */
/* Dummy routines for Ghostscript platforms with no frame buffer management */
#include "gx.h"
#include "gp.h"
#include "gxdevice.h"

/* ------ Screen management ------ */

/* Initialize the console. */
void
gp_init_console(void)
{
}

/* Write a string to the console. */
void
gp_console_puts(const char *str, uint size)
{	fwrite(str, 1, size, stdout);
}

/* Make the console current on the screen. */
int
gp_make_console_current(gx_device *dev)
{	return 0;
}

/* Make the graphics current on the screen. */
int
gp_make_graphics_current(gx_device *dev)
{	return 0;
}
