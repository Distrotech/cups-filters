/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gspenum.h */
/* Common definitions for client interface to path enumeration */

#ifndef gspenum_INCLUDED
#  define gspenum_INCLUDED

/* Define the path element types. */
#define gs_pe_moveto 1
#define gs_pe_lineto 2
#define gs_pe_curveto 3
#define gs_pe_closepath 4

/* Define an abstract type for the path enumerator. */
typedef struct gs_path_enum_s gs_path_enum;

#endif					/* gspenum_INCLUDED */
