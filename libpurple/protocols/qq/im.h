/**
 * @file im.h
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _QQ_IM_H_
#define _QQ_IM_H_

#include <glib.h>
#include "connection.h"
#include "group.h"

#define QQ_MSG_IM_MAX               500	/* max length of IM */
#define QQ_SEND_IM_BEFORE_MSG_LEN   53
#define QQ_SEND_IM_AFTER_MSG_LEN    13	/* there is one 0x00 at the end */

enum {
	QQ_IM_TEXT = 0x01,
	QQ_IM_AUTO_REPLY = 0x02
};

enum {
	QQ_MSG_TO_BUDDY = 0x0009,
	QQ_MSG_TO_UNKNOWN = 0x000a,
	QQ_MSG_NEWS = 0x0018,
	QQ_MSG_UNKNOWN_QUN_IM = 0x0020,
	QQ_MSG_ADD_TO_QUN = 0x0021,
	QQ_MSG_DEL_FROM_QUN = 0x0022,
	QQ_MSG_APPLY_ADD_TO_QUN = 0x0023,
	QQ_MSG_APPROVE_APPLY_ADD_TO_QUN = 0x0024,
	QQ_MSG_REJCT_APPLY_ADD_TO_QUN = 0x0025,
	QQ_MSG_CREATE_QUN = 0x0026,
	QQ_MSG_TEMP_QUN_IM = 0x002A,
	QQ_MSG_QUN_IM = 0x002B,
	QQ_MSG_SYS_30 = 0x0030,
	QQ_MSG_SYS_4C = 0x004C,
	QQ_MSG_EXTEND = 0x0084,
	QQ_MSG_EXTEND_85 = 0x0085,
};

void qq_got_attention(PurpleConnection *gc, const gchar *msg);

guint8 *qq_get_send_im_tail(const gchar *font_color,
		const gchar *font_size,
		const gchar *font_name,
		gboolean is_bold, gboolean is_italic, gboolean is_underline, gint len);

void qq_request_send_im(PurpleConnection *gc, guint32 uid_to, gchar *msg, gint type);

void qq_process_im(PurpleConnection *gc, guint8 *data, gint len);
void qq_process_extend_im(PurpleConnection *gc, guint8 *data, gint len);
#endif
