/*
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _MULTI_H_
#define _MULTI_H_

#include "account.h"

struct proto_buddy_menu {
	char *label;
	void (*callback)(GaimConnection *, const char *);
	GaimConnection *gc;
};

struct proto_chat_menu {
	char *label;
	void (*callback)(GaimConnection *, GHashTable *);
	GaimConnection *gc;
};

struct proto_group_menu {
	char *label;
	void (*callback)(GaimGroup *);
};

struct proto_chat_entry {
	char *label;
	char *identifier;
	char *def;
	gboolean is_int;
	int min;
	int max;
	gboolean secret;
};

#endif /* _MULTI_H_ */

/* A big line. */
/* -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
