/* Copyright (C) 1994, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* srlx.h */
/* Definitions for RLE/RLD streams */
/* Requires scommon.h; strimpl.h if any templates are referenced */

/* Common state */
#define stream_RL_state_common\
	stream_state_common;\
	bool EndOfData		/* true if 128 = EOD */

/* RunLengthEncode */
typedef struct stream_RLE_state_s {
	stream_RL_state_common;
		/* The following parameters are set by the client. */
	ulong record_size;
		/* The following change dynamically. */
	ulong record_left;		/* bytes left in current record */
} stream_RLE_state;
#define private_st_RLE_state()	/* in srle.c */\
  gs_private_st_simple(st_RLE_state, stream_RLE_state, "RunLengthEncode state")
/* We define the initialization procedure here, so that clients */
/* can avoid a procedure call. */
#define s_RLE_set_defaults_inline(ss)\
  ((ss)->EndOfData = true, (ss)->record_size = 0)
#define s_RLE_init_inline(ss)\
  ((ss)->record_left = ((ss)->record_size == 0 ?\
			((ss)->record_size = max_uint) :\
			((ss)->record_size)), 0)
extern const stream_template s_RLE_template;

/* RunLengthDecode */
typedef struct stream_RLD_state_s {
	stream_RL_state_common;
} stream_RLD_state;
#define private_st_RLD_state()	/* in srld.c */\
  gs_private_st_simple(st_RLD_state, stream_RLD_state, "RunLengthDecode state")
/* We define the initialization procedure here, so that clients */
/* can avoid a procedure call. */
#define s_RLD_set_defaults_inline(ss)\
  ((ss)->EndOfData = true)
#define s_RLD_init_inline(ss) DO_NOTHING
extern const stream_template s_RLD_template;
