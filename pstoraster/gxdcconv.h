/* Copyright (C) 1992, 1993 Aladdin Enterprises.  All rights reserved.
  
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

/* gxdcconv.h */
/* Internal device color conversion interfaces */
#include "gxfrac.h"

/* Color space conversion routines */
frac color_rgb_to_gray(P4(frac r, frac g, frac b,
  const gs_state *pgs));
void color_rgb_to_cmyk(P5(frac r, frac g, frac b,
  const gs_state *pgs, frac cmyk[4]));
frac color_cmyk_to_gray(P5(frac c, frac m, frac y, frac k,
  const gs_state *pgs));
void color_cmyk_to_rgb(P6(frac c, frac m, frac y, frac k,
  const gs_state *pgs, frac rgb[3]));
