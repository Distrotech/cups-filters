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

/* dirent_.h */
/* Generic substitute for Unix dirent.h */

/* We must include std.h before any file that includes sys/types.h. */
#include "std.h"

/* The location (or existence) of certain system headers is */
/* environment-dependent. We detect this in the makefile */
/* and conditionally define switches in gconfig_.h. */
#include "gconfig_.h"

/* Directory entries may be defined in quite a number of different */
/* header files.  The following switches are defined in gconfig_.h. */
#if !defined (SYSNDIR_H) && !defined (NDIR_H) && !defined (SYSDIR_H)
#  include <dirent.h>
typedef struct dirent dir_entry;
#else		/* SYSNDIR or NDIR or SYSDIR, i.e., no dirent */
#  ifdef SYSDIR_H
#    include <sys/dir.h>
#  endif
#  ifdef SYSNDIR_H
#    include <sys/ndir.h>
#  endif
#  ifdef NDIR_H
#    include <ndir.h>
#  endif
typedef struct direct dir_entry;
#endif		/* SYSNDIR or NDIR or SYSDIR */
