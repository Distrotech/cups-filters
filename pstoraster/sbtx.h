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

/* sbtx.h */
/* Definitions for BTE/BTD streams */
/* Requires scommon.h; strimpl.h if any templates are referenced */

/* ByteTranslateEncode/Decode */
typedef struct stream_BT_state_s {
	stream_state_common;
	byte table[256];
} stream_BT_state;
typedef stream_BT_state stream_BTE_state;
typedef stream_BT_state stream_BTD_state;
#define private_st_BT_state()	/* in sfilter1.c */\
  gs_private_st_simple(st_BT_state, stream_BT_state, "ByteTranslateEncode/Decode state")
extern const stream_template s_BT_template;
#define s_BTD_template s_BT_template
#define s_BTE_template s_BT_template
