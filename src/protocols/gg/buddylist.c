/**
 * @file buddylist.c
 *
 * gaim
 *
 * Copyright (C) 2005  Bartosz Oler <bartosz@bzimage.us>
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
 */


#include "lib/libgadu.h"

#include "gg.h"
#include "utils.h"
#include "buddylist.h"


/* void ggp_buddylist_send(GaimConnection *gc) {{{ */
void ggp_buddylist_send(GaimConnection *gc)
{
	GGPInfo *info = gc->proto_data;

	GaimBuddyList *blist;
	GaimBlistNode *gnode, *cnode, *bnode;
	GaimBuddy *buddy;
	uin_t *userlist = NULL;
	gchar *types = NULL;
	int userlist_size = 0;

	if ((blist = gaim_get_blist()) != NULL)
	{
		for (gnode = blist->root; gnode != NULL; gnode = gnode->next)
		{
			if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
				continue;
			for (cnode = gnode->child; cnode != NULL; cnode = cnode->next)
			{
				if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
					continue;
				for (bnode = cnode->child; bnode != NULL; bnode = bnode->next)
				{
					if (!GAIM_BLIST_NODE_IS_BUDDY(bnode))
						continue;
					buddy = (GaimBuddy *)bnode;

					if (buddy->account != gc->account)
						continue;

					userlist_size++;
					userlist = (uin_t *) g_renew(uin_t, userlist, userlist_size);
					types = (gchar *) g_renew(gchar, types, userlist_size);
					userlist[userlist_size - 1] = ggp_str_to_uin(buddy->name);
					types[userlist_size - 1] = GG_USER_NORMAL;
					gaim_debug_info("gg", "ggp_buddylist_send: adding %d\n", userlist[userlist_size - 1]);
				}
			}
		}
	}

	if (userlist) {
		int ret = gg_notify_ex(info->session, userlist, types, userlist_size);
		g_free(userlist);
		g_free(types);

		gaim_debug_info("gg", "send: ret=%d; size=%d\n", ret, userlist_size);
	}
}
/* }}} */

/* void ggp_buddylist_load(GaimConnection *gc, char *buddylist) {{{ */
void ggp_buddylist_load(GaimConnection *gc, char *buddylist)
{
	GaimBuddy *buddy;
	GaimGroup *group;
	gchar **users_tbl;
	int i;

	users_tbl = g_strsplit(buddylist, "\r\n", 200);

	for (i = 0; users_tbl[i] != NULL; i++) {
		gchar **data_tbl;
		gchar *name, *show, *g;

		if (strlen(users_tbl[i]) == 0)
			continue;

		data_tbl = g_strsplit(users_tbl[i], ";", 8);

		show = charset_convert(data_tbl[3], "CP1250", "UTF-8");
		name = data_tbl[6];

		gaim_debug_info("gg", "got buddy: name=%s show=%s\n", name, show);

		if (gaim_find_buddy(gaim_connection_get_account(gc), name)) {
			g_free(show);
			g_strfreev(data_tbl);
			continue;
		}

		g = g_strdup("Gadu-Gadu");

		if (strlen(data_tbl[5])) {
			/* Hard limit to at most 50 groups */
			gchar **group_tbl = g_strsplit(data_tbl[5], ",", 50);
			if (strlen(group_tbl[0]) > 0) {
				g_free(g);
				g = g_strdup(group_tbl[0]);
			}
			g_strfreev(group_tbl);
		}

		buddy = gaim_buddy_new(gaim_connection_get_account(gc), name, strlen(show) ? show : NULL);
		if (!(group = gaim_find_group(g))) {
			group = gaim_group_new(g);
			gaim_blist_add_group(group, NULL);
		}

		gaim_blist_add_buddy(buddy, NULL, group, NULL);
		g_free(g);

		g_free(show);
		g_strfreev(data_tbl);
	}
	g_strfreev(users_tbl);

	ggp_buddylist_send(gc);

}
/* }}} */

/* void ggp_buddylist_offline(GaimConnection *gc) {{{ */
void ggp_buddylist_offline(GaimConnection *gc)
{
	GaimBuddyList *blist;
	GaimBlistNode *gnode, *cnode, *bnode;
	GaimBuddy *buddy;

	if ((blist = gaim_get_blist()) != NULL)
	{
		for (gnode = blist->root; gnode != NULL; gnode = gnode->next)
		{
			if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
				continue;
			for (cnode = gnode->child; cnode != NULL; cnode = cnode->next)
			{
				if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
					continue;
				for (bnode = cnode->child; bnode != NULL; bnode = bnode->next)
				{
					if (!GAIM_BLIST_NODE_IS_BUDDY(bnode))
						continue;

					buddy = (GaimBuddy *)bnode;

					if (buddy->account != gc->account)
						continue;

					gaim_prpl_got_user_status(
						gaim_connection_get_account(gc),
						buddy->name, "offline", NULL);
					gaim_debug_info("gg", "ggp_buddylist_offline: gone: %s\n", buddy->name);
				}
			}
		}
	}
}
/* }}} */

/* char *ggp_buddylist_dump(GaimAccount *account) {{{ */
char *ggp_buddylist_dump(GaimAccount *account)
{
	GaimBuddyList *blist;
	GaimBlistNode *gnode, *cnode, *bnode;
	GaimGroup *group;
	GaimBuddy *buddy;

	char *buddylist = g_strdup("");
	char *ptr;

	if ((blist = gaim_get_blist()) == NULL)
		return NULL;

	for (gnode = blist->root; gnode != NULL; gnode = gnode->next) {
		if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;

		group = (GaimGroup *)gnode;

		for (cnode = gnode->child; cnode != NULL; cnode = cnode->next) {
			if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
				continue;

			for (bnode = cnode->child; bnode != NULL; bnode = bnode->next) {
				gchar *newdata, *name, *show, *gname;

				if (!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;

				buddy = (GaimBuddy *)bnode;
				if (buddy->account != account)
					continue;

				name = buddy->name;
				show = buddy->alias ? buddy->alias : buddy->name;
				gname = group->name;

				newdata = g_strdup_printf("%s;%s;%s;%s;%s;%s;%s;%s%s\r\n",
						show, show, show, show, "", gname, name, "", "");

				ptr = buddylist;
				buddylist = g_strconcat(ptr, newdata, NULL);

				g_free(newdata);
				g_free(ptr);
			}
		}
	}

	return buddylist;
}
/* }}} */


/* vim: set ts=4 sts=0 sw=4 noet: */
