/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsptype2.h,v 1.1 2000/03/13 18:57:56 mike Exp $ */
/* Client interface to PatternType 2 Patterns */

#ifndef gsptype2_INCLUDED
#  define gsptype2_INCLUDED

#include "gspcolor.h"

/* ---------------- Types and structures ---------------- */

#ifndef gs_shading_t_DEFINED
#  define gs_shading_t_DEFINED
typedef struct gs_shading_s gs_shading_t;

#endif

/* PatternType 2 template */
typedef struct gs_pattern2_template_s {
    gs_pattern_template_common;
    const gs_shading_t *Shading;
} gs_pattern2_template_t;

/* ---------------- Procedures ---------------- */

/* Initialize a PatternType 2 pattern. */
void gs_pattern2_init(P1(gs_pattern2_template_t *));

#endif /* gsptype2_INCLUDED */
