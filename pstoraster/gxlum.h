/*
  Copyright 1993-1999 by Easy Software Products.
  Copyright (C) 1992 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
  to anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer
  to the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given
  to you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises supports the work of the GNU Project, but is not
  affiliated with the Free Software Foundation or the GNU Project.  GNU
  Ghostscript, as distributed by Aladdin Enterprises, does not require any
  GNU software to build or run it.
*/

/*$Id: gxlum.h,v 1.4 2000/03/08 23:15:02 mike Exp $ */
/* Luminance computation parameters for Ghostscript */

#ifndef gxlum_INCLUDED
#  define gxlum_INCLUDED

/*
 * Color weights used for computing luminance.
 *
 * The original ones used here were for NTSC video displays.  Of course,
 * if you want to print instead of display things are different...
 */
#ifdef NTSC_LUM
#  define lum_red_weight	30
#  define lum_green_weight	59
#  define lum_blue_weight	11
#else
#  define lum_red_weight	31
#  define lum_green_weight	61
#  define lum_blue_weight	8
#endif /* NTSC_LUN */
#define lum_all_weights	(lum_red_weight + lum_green_weight + lum_blue_weight)

#endif /* gxlum_INCLUDED */
