/*
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Some code copyright 2003 Tim Ringenbach <omarvo@hotmail.com>
 * (marv on irc.freenode.net)
 * Some code borrowed from libyahoo2, copyright (C) 2002, Philip
 * S Tellis <philip . tellis AT gmx . net>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "debug.h"
#include "prpl.h"

#include "conversation.h"
#include "notify.h"
#include "util.h"
#include "multi.h"
#include "internal.h"

#include "yahoo.h"
#include "yahoochat.h"

#define YAHOO_CHAT_ID (1)

/* prototype(s) */
static void yahoo_chat_leave(GaimConnection *gc, const char *room, const char *dn, gboolean logout);

/* special function to log us on to the yahoo chat service */
static void yahoo_chat_online(GaimConnection *gc)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;


	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATONLINE, YAHOO_STATUS_AVAILABLE,0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 109, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 6, "abcde");

	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);
}

static gint _mystrcmpwrapper(gconstpointer a, gconstpointer b)
{
	return strcmp(a, b);
}

/* this is slow, and different from the gaim_* version in that it (hopefully) won't add a user twice */
static void yahoo_chat_add_users(GaimConvChat *chat, GList *newusers)
{
	GList *users, *i, *j;

	users = gaim_conv_chat_get_users(chat);

	for (i = newusers; i; i = i->next) {
		j = g_list_find_custom(users, i->data, _mystrcmpwrapper);
		if (j)
			continue;
		gaim_conv_chat_add_user(chat, i->data, NULL);
	}
}

static void yahoo_chat_add_user(GaimConvChat *chat, const char *user, const char *reason)
{
	GList *users;

	users = gaim_conv_chat_get_users(chat);

	if ((g_list_find_custom(users, user, _mystrcmpwrapper)))
		return;

	gaim_conv_chat_add_user(chat, user, reason);
}

static GaimConversation *yahoo_find_conference(GaimConnection *gc, const char *name)
{
	struct yahoo_data *yd;
	GSList *l;

	yd = gc->proto_data;

	for (l = yd->confs; l; l = l->next) {
		GaimConversation *c = l->data;
		if (!gaim_utf8_strcasecmp(gaim_conversation_get_name(c), name))
			return c;
	}
	return NULL;
}


void yahoo_process_conference_invite(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *who = NULL;
	char *msg = NULL;
	GString *members = NULL;
	GHashTable *components;


	if (pkt->status == 2)
		return; /* XXX */

	members = g_string_sized_new(512);

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 1: /* us, but we already know who we are */
			break;
		case 57:
			room = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 50: /* inviter */
			who = pair->value;
			g_string_append_printf(members, "%s\n", who);
			break;
		case 52: /* members */
			g_string_append_printf(members, "%s\n", pair->value);
			break;
		case 58:
			msg = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 13: /* ? */
			break;
		}
	}

	if (!room) {
		g_string_free(members, TRUE);
		return;
	}

	components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_replace(components, g_strdup("room"), room);
	if (msg)
		g_hash_table_replace(components, g_strdup("topic"), msg);
	g_hash_table_replace(components, g_strdup("type"), g_strdup("Conference"));
	if (members) {
		g_hash_table_replace(components, g_strdup("members"), g_strdup(members->str));
	}
	serv_got_chat_invite(gc, room, who, msg, components);

	g_string_free(members, TRUE);
}

void yahoo_process_conference_decline(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *who = NULL;
	char *msg = NULL;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 57:
			room = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 54:
			who = pair->value;
			break;
		case 14:
			msg = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		}
	}

	if (who && room) {
		char *tmp;

		tmp = g_strdup_printf(_("%s declined your conference invitation to room \"%s\" because \"%s\"."),
						who, room, msg?msg:"");
		gaim_notify_info(gc, NULL, _("Invitation Rejected"), tmp);
		g_free(tmp);
		g_free(room);
		if (msg)
			g_free(msg);
	}
}

void yahoo_process_conference_logon(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *who = NULL;
	GaimConversation *c;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 57:
			room = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 53:
			who = pair->value;
			break;
		}
	}

	if (who && room) {
		c = yahoo_find_conference(gc, room);
		if (c)
			yahoo_chat_add_user(GAIM_CONV_CHAT(c), who, NULL);
		g_free(room);
	}
}

void yahoo_process_conference_logoff(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *who = NULL;
	GaimConversation *c;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 57:
			room = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 56:
			who = pair->value;
			break;
		}
	}

	if (who && room) {
		c = yahoo_find_conference(gc, room);
		if (c)
			gaim_conv_chat_remove_user(GAIM_CONV_CHAT(c), who, NULL);
		g_free(room);
	}
}

void yahoo_process_conference_message(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *who = NULL;
	char *msg = NULL;
	char *msg2;
	int utf8 = 0;
	GaimConversation *c;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 57:
			room = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 3:
			who = pair->value;
			break;
		case 14:
			msg = pair->value;
			break;
		case 97:
			utf8 = strtol(pair->value, NULL, 10);
			break;
		}
	}

		if (room && who && msg) {
			msg2 = yahoo_string_decode(gc, msg, utf8);
			c = yahoo_find_conference(gc, room);
			if (!c)
				return;
			msg = yahoo_codes_to_html(msg2);
			serv_got_chat_in(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(c)), who, 0, msg, time(NULL));
			g_free(msg);
			g_free(msg2);
		}
		if (room)
			g_free(room);
}


/* this is a comfirmation of yahoo_chat_online(); */
void yahoo_process_chat_online(GaimConnection *gc, struct yahoo_packet *pkt)
{
	struct yahoo_data *yd = (struct yahoo_data *) gc->proto_data;

	if (pkt->status == 1)
		yd->chat_online = 1;
}

/* this is basicly the opposite of chat_online */
void yahoo_process_chat_logout(GaimConnection *gc, struct yahoo_packet *pkt)
{
	struct yahoo_data *yd = (struct yahoo_data *) gc->proto_data;
	GSList *l;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		if (pair->key == 1)
			if (g_ascii_strcasecmp(pair->value,
					gaim_connection_get_display_name(gc)))
				return;
	}

	if (pkt->status == 1) {
		yd->chat_online = 0;
		if (yd->in_chat)
			yahoo_c_leave(gc, YAHOO_CHAT_ID);
	}
}

void yahoo_process_chat_join(GaimConnection *gc, struct yahoo_packet *pkt)
{
	struct yahoo_data *yd = (struct yahoo_data *) gc->proto_data;
	GaimConversation *c = NULL;
	GSList *l;
	GList *members = NULL;
	char *room = NULL;
	char *topic = NULL;
	char *someid, *someotherid, *somebase64orhashosomething, *somenegativenumber;

	if (pkt->status == -1) {
		gaim_notify_error(gc, NULL, _("Failed to join chat"), _("Maybe the room is full?"));
		return;
	}

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {

		case 104:
			room = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 105:
			topic = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 128:
			someid = pair->value;
			break;
		case 108: /* number of joiners */
			break;
		case 129:
			someotherid = pair->value;
			break;
		case 130:
			somebase64orhashosomething = pair->value;
			break;
		case 126:
			somenegativenumber = pair->value;
			break;
		case 13: /* this is 1. maybe its the type of room? (normal, user created, private, etc?) */
			break;
		case 61: /*this looks similiar to 130 */
			break;

		/* the previous section was just room info. this next section is
		   info about individual room members, (including us) */

		case 109: /* the yahoo id */
			members = g_list_append(members, pair->value);
			break;
		case 110: /* age */
			break;
		case 141: /* nickname */
			break;
		case 142: /* location */
			break;
		case 113: /* bitmask */
			break;
		}
	}

	if (!room)
		return;

	if (yd->chat_name && gaim_utf8_strcasecmp(room, yd->chat_name))
		yahoo_chat_leave(gc, room,
				gaim_connection_get_display_name(gc), FALSE);

	c = gaim_find_chat(gc, YAHOO_CHAT_ID);

	if ((!c || gaim_conv_chat_has_left(GAIM_CONV_CHAT(c))) && members &&
	   ((g_list_length(members) > 1) ||
	     !g_ascii_strcasecmp(members->data, gaim_connection_get_display_name(gc)))) {
		if (c && gaim_conv_chat_has_left(GAIM_CONV_CHAT(c))) {
			/* this might be a hack, but oh well, it should nicely */
			char *tmpmsg;

			gaim_conversation_set_name(c, room);

			c = serv_got_joined_chat(gc, YAHOO_CHAT_ID, room);
			if (topic)
				gaim_conv_chat_set_topic(GAIM_CONV_CHAT(c), NULL, topic);
			yd->in_chat = 1;
			yd->chat_name = g_strdup(room);
			gaim_conv_chat_add_users(GAIM_CONV_CHAT(c), members);

			tmpmsg = g_strdup_printf(_("You are now chatting in %s."), room);
			gaim_conv_chat_write(GAIM_CONV_CHAT(c), "", tmpmsg, GAIM_MESSAGE_SYSTEM, time(NULL));
			g_free(tmpmsg);
		} else {
			c = serv_got_joined_chat(gc, YAHOO_CHAT_ID, room);
			if (topic)
				gaim_conv_chat_set_topic(GAIM_CONV_CHAT(c), NULL, topic);
			yd->in_chat = 1;
			yd->chat_name = g_strdup(room);
			gaim_conv_chat_add_users(GAIM_CONV_CHAT(c), members);
		}
	} else if (c) {
		yahoo_chat_add_users(GAIM_CONV_CHAT(c), members);
	}

	g_list_free(members);
	g_free(room);
	if (topic)
		g_free(topic);
}

void yahoo_process_chat_exit(GaimConnection *gc, struct yahoo_packet *pkt)
{
	char *who = NULL;
	char *room = NULL;
	GSList *l;
	struct yahoo_data *yd;

	yd = gc->proto_data;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		if (pair->key == 104)
			room = yahoo_string_decode(gc, pair->value, FALSE);
		if (pair->key == 109)
			who = pair->value;
	}


	if (who && room) {
		GaimConversation *c = gaim_find_chat(gc, YAHOO_CHAT_ID);
		if (c && !gaim_utf8_strcasecmp(gaim_conversation_get_name(c), room))
			gaim_conv_chat_remove_user(GAIM_CONV_CHAT(c), who, NULL);

	}
	if (room)
		g_free(room);
}

void yahoo_process_chat_message(GaimConnection *gc, struct yahoo_packet *pkt)
{
	char *room = NULL, *who = NULL, *msg = NULL, *msg2;
	int msgtype = 1, utf8 = 0;
	GaimConversation *c = NULL;
	GSList *l;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {

		case 97:
			utf8 = strtol(pair->value, NULL, 10);
			break;
		case 104:
			room = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 109:
			who = pair->value;
			break;
		case 117:
			msg = pair->value;
			break;
		case 124:
			msgtype = strtol(pair->value, NULL, 10);
			break;
		}
	}


	c = gaim_find_chat(gc, YAHOO_CHAT_ID);
	if (!who || !c) {
		if (room)
			g_free(room);
		/* we still get messages after we part, funny that */
		return;
	}

	if (!msg) {
		gaim_debug(GAIM_DEBUG_MISC, "yahoo", "Got a message packet with no message.\nThis probably means something important, but we're ignoring it.\n");
		return;
	}
	msg2 = yahoo_string_decode(gc, msg, utf8);
	msg = yahoo_codes_to_html(msg2);
	g_free(msg2);

	if (msgtype == 2 || msgtype == 3) {
		char *tmp;
		tmp = g_strdup_printf("/me %s", msg);
		g_free(msg);
		msg = tmp;
	}

	serv_got_chat_in(gc, YAHOO_CHAT_ID, who, 0, msg, time(NULL));
	g_free(msg);
}

void yahoo_process_chat_addinvite(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *msg = NULL;
	char *who = NULL;


	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 104:
			room = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 129: /* room id? */
			break;
		case 126: /* ??? */
			break;
		case 117:
			msg = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 119:
			who = pair->value;
			break;
		case 118: /* us */
			break;
		}
	}

	if (room && who) {
		GHashTable *components;

		components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_replace(components, g_strdup("room"), g_strdup(room));
		serv_got_chat_invite(gc, room, who, msg, components);
	}
	if (room)
		g_free(room);
	if (msg)
		g_free(msg);
}

void yahoo_process_chat_goto(GaimConnection *gc, struct yahoo_packet *pkt)
{
	if (pkt->status == -1)
		gaim_notify_error(gc, NULL, _("Failed to join buddy in chat"),
						_("Maybe they're not in a chat?"));
}


/*
 * Functions dealing with conferences
 * I think conference names are always ascii.
 */

static void yahoo_conf_leave(struct yahoo_data *yd, const char *room, const char *dn, GList *who)
{
	struct yahoo_packet *pkt;
	GList *w;


	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFLOGOFF, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, dn);
	for (w = who; w; w = w->next) {
		yahoo_packet_hash(pkt, 3, (char *)w->data);
	}

	yahoo_packet_hash(pkt, 57, room);

	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);
}

static int yahoo_conf_send(GaimConnection *gc, const char *dn, const char *room,
							GList *members, const char *what)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;
	GList *who;
	char *msg, *msg2;
	int utf8 = 1;

	msg = yahoo_html_to_codes(what);
	msg2 = yahoo_string_encode(gc, msg, &utf8);


	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFMSG, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, dn);
	for (who = members; who; who = who->next)
		yahoo_packet_hash(pkt, 53, (char *)who->data);
	yahoo_packet_hash(pkt, 57, room);
	yahoo_packet_hash(pkt, 14, msg2);
	if (utf8)
		yahoo_packet_hash(pkt, 97, "1"); /* utf-8 */

	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);
	g_free(msg);
	g_free(msg2);

	return 0;
}

static void yahoo_conf_join(struct yahoo_data *yd, GaimConversation *c, const char *dn, const char *room,
						const char *topic, const char *members)
{
	struct yahoo_packet *pkt;
	char **memarr = NULL;
	int i;

	if (members)
		memarr = g_strsplit(members, "\n", 0);


	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFLOGON, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, dn);
	yahoo_packet_hash(pkt, 3, dn);
	yahoo_packet_hash(pkt, 57, room);
	if (memarr) {
		for(i = 0 ; memarr[i]; i++) {
			if (!strcmp(memarr[i], "") || !strcmp(memarr[i], dn))
					continue;
			yahoo_packet_hash(pkt, 3, memarr[i]);
			gaim_conv_chat_add_user(GAIM_CONV_CHAT(c), memarr[i], NULL);
		}
	}
	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);

	if (memarr)
		g_strfreev(memarr);
}

static void yahoo_conf_invite(GaimConnection *gc, GaimConversation *c,
		const char *dn, const char *buddy, const char *room, const char *msg)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;
	GList *members;
	char *msg2 = NULL;

	if (msg)
		msg2 = yahoo_string_encode(gc, msg, NULL);

	members = gaim_conv_chat_get_users(GAIM_CONV_CHAT(c));

	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFADDINVITE, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, dn);
	yahoo_packet_hash(pkt, 51, buddy);
	yahoo_packet_hash(pkt, 57, room);
	yahoo_packet_hash(pkt, 58, msg?msg2:"");
	yahoo_packet_hash(pkt, 13, "0");
	for(; members; members = members->next) {
		if (!strcmp(members->data, dn))
			continue;
		yahoo_packet_hash(pkt, 52, (char *)members->data);
		yahoo_packet_hash(pkt, 53, (char *)members->data);
	}
	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);
	if (msg)
		g_free(msg2);
}

/*
 * Functions dealing with chats
 */

static void yahoo_chat_leave(GaimConnection *gc, const char *room, const char *dn, gboolean logout)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;
	GaimConversation *c;
	char *eroom;

	eroom = yahoo_string_encode(gc, room, NULL);

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATEXIT, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 104, eroom);
	yahoo_packet_hash(pkt, 109, dn);
	yahoo_packet_hash(pkt, 108, "1");
	yahoo_packet_hash(pkt, 112, "0"); /* what does this one mean? */

	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);

	yd->in_chat = 0;
	if (yd->chat_name) {
		g_free(yd->chat_name);
		yd->chat_name = NULL;
	}

	if ((c = gaim_find_chat(gc, YAHOO_CHAT_ID)))
		serv_got_chat_left(gc, YAHOO_CHAT_ID);

	if (!logout)
		return;
	
	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATLOGOUT,
			YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, dn);
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);

	yd->chat_online = 0;
	g_free(eroom);
}

/* borrowed from gtkconv.c */
static gboolean
meify(char *message, size_t len)
{
	/*
	 * Read /me-ify: If the message (post-HTML) starts with /me,
	 * remove the "/me " part of it (including that space) and return TRUE.
	 */
	char *c;
	gboolean inside_html = 0;

	/* Umm.. this would be very bad if this happens. */
	g_return_val_if_fail(message != NULL, FALSE);

	if (len == -1)
		len = strlen(message);

	for (c = message; *c != '\0'; c++, len--) {
		if (inside_html) {
			if (*c == '>')
				inside_html = FALSE;
		}
		else {
			if (*c == '<')
				inside_html = TRUE;
			else
				break;
		}
	}

	if (*c != '\0' && !g_ascii_strncasecmp(c, "/me ", 4)) {
		memmove(c, c + 4, len - 3);

		return TRUE;
	}

	return FALSE;
}

static int yahoo_chat_send(GaimConnection *gc, const char *dn, const char *room, const char *what)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;
	int me = 0;
	char *msg1, *msg2, *room2;
	gboolean utf8 = TRUE;

	msg1 = g_strdup(what);

	if (meify(msg1, -1))
		me = 1;

	msg2 = yahoo_html_to_codes(msg1);
	g_free(msg1);
	msg1 = yahoo_string_encode(gc, msg2, &utf8);
	g_free(msg2);
	room2 = yahoo_string_encode(gc, room, NULL);

	pkt = yahoo_packet_new(YAHOO_SERVICE_COMMENT, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, dn);
	yahoo_packet_hash(pkt, 104, room2);
	yahoo_packet_hash(pkt, 117, msg1);
	if (me)
		yahoo_packet_hash(pkt, 124, "2");
	else
		yahoo_packet_hash(pkt, 124, "1");
	/* fixme: what about /think? (124=3) */
	if (utf8)
		yahoo_packet_hash(pkt, 97, "1");

	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
	g_free(msg1);
	g_free(room2);

	return 0;
}

static void yahoo_chat_join(GaimConnection *gc, const char *dn, const char *room, const char *topic)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;
	char *room2;

	room2 = yahoo_string_encode(gc, room, NULL);

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATJOIN, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 62, "2");
	yahoo_packet_hash(pkt, 104, room2);
	yahoo_packet_hash(pkt, 129, "0");

	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);
	g_free(room2);
}

static void yahoo_chat_invite(GaimConnection *gc, const char *dn, const char *buddy,
							const char *room, const char *msg)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;
	char *room2, *msg2 = NULL;

	room2 = yahoo_string_encode(gc, room, NULL);
	if (msg)
		msg2 = yahoo_string_encode(gc, msg, NULL);
	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATADDINVITE, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, dn);
	yahoo_packet_hash(pkt, 118, buddy);
	yahoo_packet_hash(pkt, 104, room2);
	yahoo_packet_hash(pkt, 117, (msg2?msg2:""));
	yahoo_packet_hash(pkt, 129, "0");

	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);

	g_free(room2);
	if (msg2)
		g_free(msg2);
}

void yahoo_chat_goto(GaimConnection *gc, const char *name)
{
	struct yahoo_data *yd;
	struct yahoo_packet *pkt;

	yd = gc->proto_data;

	if (!yd->chat_online)
		yahoo_chat_online(gc);

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATGOTO, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 109, name);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 62, "2");

	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
}
/*
 * These are the functions registered with the core
 * which get called for both chats and conferences.
 */

void yahoo_c_leave(GaimConnection *gc, int id)
{
	struct yahoo_data *yd = (struct yahoo_data *) gc->proto_data;
	GaimConversation *c;

	if (!yd)
		return;


	c = gaim_find_chat(gc, id);
	if (!c)
		return;

	if (id != YAHOO_CHAT_ID) {
		yahoo_conf_leave(yd, gaim_conversation_get_name(c),
			gaim_connection_get_display_name(gc), gaim_conv_chat_get_users(GAIM_CONV_CHAT(c)));
			yd->confs = g_slist_remove(yd->confs, c);
	} else {
		yahoo_chat_leave(gc, gaim_conversation_get_name(c), gaim_connection_get_display_name(gc), TRUE);
	}

	serv_got_chat_left(gc, id);
}

int yahoo_c_send(GaimConnection *gc, int id, const char *what)
{
	GaimConversation *c;
	int ret;
	struct yahoo_data *yd;

	yd = (struct yahoo_data *) gc->proto_data;
	if (!yd)
		return -1;

	c = gaim_find_chat(gc, id);
	if (!c)
		return -1;

	if (id != YAHOO_CHAT_ID) {
		ret = yahoo_conf_send(gc, gaim_connection_get_display_name(gc),
				gaim_conversation_get_name(c), gaim_conv_chat_get_users(GAIM_CONV_CHAT(c)), what);
	} else {
		ret = yahoo_chat_send(gc, gaim_connection_get_display_name(gc),
						gaim_conversation_get_name(c), what);
		if (!ret)
			serv_got_chat_in(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(c)),
					gaim_connection_get_display_name(gc), 0, what, time(NULL));
	}
	return ret;
}

GList *yahoo_c_info(GaimConnection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Room:");
	pce->identifier = "room";
	m = g_list_append(m, pce);

	return m;
}

void yahoo_c_join(GaimConnection *gc, GHashTable *data)
{
	struct yahoo_data *yd;
	char *room, *topic, *members, *type;
	int id;
	GaimConversation *c;

	yd = (struct yahoo_data *) gc->proto_data;
	if (!yd)
		return;

	room = g_hash_table_lookup(data, "room");
	if (!room)
		return;

	topic = g_hash_table_lookup(data, "topic");
	if (!topic)
		topic = "";

	members = g_hash_table_lookup(data, "members");


	if ((type = g_hash_table_lookup(data, "type")) && !strcmp(type, "Conference")) {
		id = yd->conf_id++;
		c = serv_got_joined_chat(gc, id, room);
		yd->confs = g_slist_prepend(yd->confs, c);
		gaim_conv_chat_set_topic(GAIM_CONV_CHAT(c), gaim_connection_get_display_name(gc), topic);
		yahoo_conf_join(yd, c, gaim_connection_get_display_name(gc), room, topic, members);
		return;
	} else {
		if (yd->in_chat)
			yahoo_chat_leave(gc, room,
					gaim_connection_get_display_name(gc),
					FALSE);
		if (!yd->chat_online)
			yahoo_chat_online(gc);
		yahoo_chat_join(gc, gaim_connection_get_display_name(gc), room, topic);
		return;
	}
}

void yahoo_c_invite(GaimConnection *gc, int id, const char *msg, const char *name)
{
	GaimConversation *c;

	c = gaim_find_chat(gc, id);
	if (!c || !c->name)
		return;

	if (id != YAHOO_CHAT_ID) {
		yahoo_conf_invite(gc, c, gaim_connection_get_display_name(gc), name,
							gaim_conversation_get_name(c), msg);
	} else {
		yahoo_chat_invite(gc, gaim_connection_get_display_name(gc), name,
							gaim_conversation_get_name(c), msg);
	}
}


struct yahoo_roomlist {
	int fd;
	int inpa;
	guchar *rxqueue;
	int rxlen;
	gboolean started;
	char *path;
	char *host;
	GaimRoomlist *list;
	GaimRoomlistRoom *cat;
	GaimRoomlistRoom *ucat;
	GMarkupParseContext *parse;

};

static void yahoo_roomlist_destroy(struct yahoo_roomlist *yrl)
{
	if (yrl->inpa)
		gaim_input_remove(yrl->inpa);
	if (yrl->rxqueue)
		g_free(yrl->rxqueue);
	if (yrl->path)
		g_free(yrl->path);
	if (yrl->host)
		g_free(yrl->host);
	if (yrl->parse)
		g_markup_parse_context_free(yrl->parse);
}

enum yahoo_room_type {
	yrt_yahoo,
	yrt_user,
};

struct yahoo_chatxml_state {
	GaimRoomlist *list;
	struct yahoo_roomlist *yrl;
	GQueue *q;
	struct {
		enum yahoo_room_type type;
		char *name;
		char *topic;
		char *id;
		int users, voices, webcams;
	} room;
};

struct yahoo_lobby {
	int count, users, voices, webcams;
};

static struct yahoo_chatxml_state *yahoo_chatxml_state_new(GaimRoomlist *list, struct yahoo_roomlist *yrl)
{
	struct yahoo_chatxml_state *s;

	s = g_new0(struct yahoo_chatxml_state, 1);

	s->list = list;
	s->yrl = yrl;
	s->q = g_queue_new();

	return s;
}

static void yahoo_chatxml_state_destroy(struct yahoo_chatxml_state *s)
{
	g_queue_free(s->q);
	if (s->room.name)
		g_free(s->room.name);
	if (s->room.topic)
		g_free(s->room.topic);
	if (s->room.id)
		g_free(s->room.id);
	g_free(s);
}

static void yahoo_chatlist_start_element(GMarkupParseContext *context, const gchar *ename,
                                  const gchar **anames, const gchar **avalues,
                                  gpointer user_data, GError **error)
{
	struct yahoo_chatxml_state *s = user_data;
	GaimRoomlist *list = s->list;
	GaimRoomlistRoom *r;
	GaimRoomlistRoom *parent;
	int i;

	if (!strcmp(ename, "category")) {
		const gchar *name = NULL, *id = NULL;

		for (i = 0; anames[i]; i++) {
			if (!strcmp(anames[i], "id"))
				id = avalues[i];
			if (!strcmp(anames[i], "name"))
				name = avalues[i];
		}
		if (!name || !id)
			return;

		parent = g_queue_peek_head(s->q);
		r = gaim_roomlist_room_new(GAIM_ROOMLIST_ROOMTYPE_CATAGORY, name, parent);
		gaim_roomlist_room_add_field(list, r, (gpointer)name);
		gaim_roomlist_room_add_field(list, r, (gpointer)id);
		gaim_roomlist_room_add(list, r);
		g_queue_push_head(s->q, r);
	} else if (!strcmp(ename, "room")) {
		s->room.users = s->room.voices = s->room.webcams = 0;

		for (i = 0; anames[i]; i++) {
			if (!strcmp(anames[i], "id")) {
				if (s->room.id)
					g_free(s->room.id);
				s->room.id = g_strdup(avalues[i]);
			} else if (!strcmp(anames[i], "name")) {
				if (s->room.name)
					g_free(s->room.name);
				s->room.name = g_strdup(avalues[i]);
			} else if (!strcmp(anames[i], "topic")) {
				if (s->room.topic)
					g_free(s->room.topic);
				s->room.topic = g_strdup(avalues[i]);
			} else if (!strcmp(anames[i], "type")) {
				if (!strcmp("yahoo", avalues[i]))
					s->room.type = yrt_yahoo;
				else
					s->room.type = yrt_user;
			}
		}

	} else if (!strcmp(ename, "lobby")) {
		struct yahoo_lobby *lob = g_new0(struct yahoo_lobby, 1);

		for (i = 0; anames[i]; i++) {
			if (!strcmp(anames[i], "count")) {
				lob->count = strtol(avalues[i], NULL, 10);
			} else if (!strcmp(anames[i], "users")) {
				s->room.users += lob->users = strtol(avalues[i], NULL, 10);
			} else if (!strcmp(anames[i], "voices")) {
				s->room.voices += lob->voices = strtol(avalues[i], NULL, 10);
			} else if (!strcmp(anames[i], "webcams")) {
				s->room.webcams += lob->webcams = strtol(avalues[i], NULL, 10);
			}
		}

		g_queue_push_head(s->q, lob);
	}

}

static void yahoo_chatlist_end_element(GMarkupParseContext *context, const gchar *ename,
                                       gpointer user_data, GError **error)
{
	struct yahoo_chatxml_state *s = user_data;

	if (!strcmp(ename, "category")) {
		g_queue_pop_head(s->q);
	} else if (!strcmp(ename, "room")) {
		struct yahoo_lobby *lob;
		GaimRoomlistRoom *r, *l;

		if (s->room.type == yrt_yahoo)
			r = gaim_roomlist_room_new(GAIM_ROOMLIST_ROOMTYPE_CATAGORY|GAIM_ROOMLIST_ROOMTYPE_ROOM,
		                                   s->room.name, s->yrl->cat);
		else
			r = gaim_roomlist_room_new(GAIM_ROOMLIST_ROOMTYPE_CATAGORY|GAIM_ROOMLIST_ROOMTYPE_ROOM,
		                                   s->room.name, s->yrl->ucat);

		gaim_roomlist_room_add_field(s->list, r, s->room.name);
		gaim_roomlist_room_add_field(s->list, r, s->room.id);
		gaim_roomlist_room_add_field(s->list, r, GINT_TO_POINTER(s->room.users));
		gaim_roomlist_room_add_field(s->list, r, GINT_TO_POINTER(s->room.voices));
		gaim_roomlist_room_add_field(s->list, r, GINT_TO_POINTER(s->room.webcams));
		gaim_roomlist_room_add_field(s->list, r, s->room.topic);
		gaim_roomlist_room_add(s->list, r);

		while ((lob = g_queue_pop_head(s->q))) {
			char *name = g_strdup_printf("%s:%d", s->room.name, lob->count);
			l = gaim_roomlist_room_new(GAIM_ROOMLIST_ROOMTYPE_ROOM, name, r);

			gaim_roomlist_room_add_field(s->list, l, name);
			gaim_roomlist_room_add_field(s->list, l, s->room.id);
			gaim_roomlist_room_add_field(s->list, l, GINT_TO_POINTER(lob->users));
			gaim_roomlist_room_add_field(s->list, l, GINT_TO_POINTER(lob->voices));
			gaim_roomlist_room_add_field(s->list, l, GINT_TO_POINTER(lob->webcams));
			gaim_roomlist_room_add_field(s->list, l, s->room.topic);
			gaim_roomlist_room_add(s->list, l);

			g_free(name);
			g_free(lob);
		}

	}

}

static GMarkupParser parser = {
	yahoo_chatlist_start_element,
	yahoo_chatlist_end_element,
	NULL,
	NULL,
	NULL
};

static void yahoo_roomlist_cleanup(GaimRoomlist *list, struct yahoo_roomlist *yrl)
{
	gaim_roomlist_set_in_progress(list, FALSE);

	if (yrl) {
		list->proto_data = g_list_remove(list->proto_data, yrl);
		yahoo_roomlist_destroy(yrl);
	}

	gaim_roomlist_unref(list);
}

static void yahoo_roomlist_pending(gpointer data, gint source, GaimInputCondition cond)
{
	struct yahoo_roomlist *yrl = data;
	GaimRoomlist *list = yrl->list;
	char buf[1024];
	int len;
	guchar *start;
	struct yahoo_chatxml_state *s;

	len = read(yrl->fd, buf, sizeof(buf));

	if (len <= 0) {
		if (yrl->parse)
			g_markup_parse_context_end_parse(yrl->parse, NULL);
		yahoo_roomlist_cleanup(list, yrl);
		return;
	}


	yrl->rxqueue = g_realloc(yrl->rxqueue, len + yrl->rxlen);
	memcpy(yrl->rxqueue + yrl->rxlen, buf, len);
	yrl->rxlen += len;

	if (!yrl->started) {
		yrl->started = TRUE;
		start = g_strstr_len(yrl->rxqueue, yrl->rxlen, "\r\n\r\n");
		if (!start || (start - yrl->rxqueue + 4) >= yrl->rxlen)
			return;
		start += 4;
	} else {
		start = yrl->rxqueue;
	}

	if (yrl->parse == NULL) {
		s = yahoo_chatxml_state_new(list, yrl);
		yrl->parse = g_markup_parse_context_new(&parser, 0, s,
		             (GDestroyNotify)yahoo_chatxml_state_destroy);
	}

	if (!g_markup_parse_context_parse(yrl->parse, start, (yrl->rxlen - (start - yrl->rxqueue)), NULL)) {

		yahoo_roomlist_cleanup(list, yrl);
		return;
	}

	yrl->rxlen = 0;
}

static void yahoo_roomlist_got_connected(gpointer data, gint source, GaimInputCondition cond)
{
	struct yahoo_roomlist *yrl = data;
	GaimRoomlist *list = yrl->list;
	char *buf, *cookie;
	struct yahoo_data *yd = gaim_account_get_connection(list->account)->proto_data;

	if (source < 0) {
		gaim_notify_error(gaim_account_get_connection(list->account), NULL, _("Unable to connect"), _("Fetching the room list failed."));
		yahoo_roomlist_cleanup(list, yrl);
		return;
	}

	yrl->fd = source;

	cookie = g_strdup_printf("Y=%s; T=%s", yd->cookie_y, yd->cookie_t);
	buf = g_strdup_printf("GET /%s HTTP/1.0\r\nHost: %s\r\nCookie: %s\r\n\r\n", yrl->path, yrl->host, cookie);
	write(yrl->fd, buf, strlen(buf));
	g_free(cookie);
	g_free(buf);
	yrl->inpa = gaim_input_add(yrl->fd, GAIM_INPUT_READ, yahoo_roomlist_pending, yrl);

}

GaimRoomlist *yahoo_roomlist_get_list(GaimConnection *gc)
{
	struct yahoo_roomlist *yrl;
	GaimRoomlist *rl;
	char *url;
	GList *fields = NULL;
	GaimRoomlistField *f;

	url = g_strdup_printf("%s?chatcat=0",
	                      gaim_account_get_string(
	                      gaim_connection_get_account(gc),
	                      "room_list", YAHOO_ROOMLIST_URL));

	yrl = g_new0(struct yahoo_roomlist, 1);
	rl = gaim_roomlist_new(gaim_connection_get_account(gc));
	yrl->list = rl;

	gaim_url_parse(url, &(yrl->host), NULL, &(yrl->path));
	g_free(url);

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, "", "room", TRUE);
	fields = g_list_append(fields, f);

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, "", "id", TRUE);
	fields = g_list_append(fields, f);

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_INT, _("Users"), "users", FALSE);
	fields = g_list_append(fields, f);

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_INT, _("Voices"), "voices", FALSE);
	fields = g_list_append(fields, f);

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_INT, _("Webcams"), "webcams", FALSE);
	fields = g_list_append(fields, f);

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, _("Topic"), "topic", FALSE);
	fields = g_list_append(fields, f);

	gaim_roomlist_set_fields(rl, fields);

	if (gaim_proxy_connect(gaim_connection_get_account(gc),
	                       yrl->host, 80, yahoo_roomlist_got_connected, yrl) != 0)
	{
		gaim_notify_error(gc, NULL, _("Connection problem"), _("Unable to fetch room list."));
		yahoo_roomlist_cleanup(rl, yrl);
		return NULL;
	}

	rl->proto_data = g_list_append(rl->proto_data, yrl);

	gaim_roomlist_set_in_progress(rl, TRUE);
	return rl;
}

void yahoo_roomlist_cancel(GaimRoomlist *list)
{
	GList *l, *k;

	k = l = list->proto_data;
	list->proto_data = NULL;

	gaim_roomlist_set_in_progress(list, FALSE);

	for (; l; l = l->next) {
		yahoo_roomlist_destroy(l->data);
		gaim_roomlist_unref(l->data);
	}
	g_list_free(k);
}

void yahoo_roomlist_expand_catagory(GaimRoomlist *list, GaimRoomlistRoom *catagory)
{
	struct yahoo_roomlist *yrl;
	char *url;
	char *id;

	if (catagory->type != GAIM_ROOMLIST_ROOMTYPE_CATAGORY)
		return;

	if (!(id = g_list_nth_data(catagory->fields, 1))) {
		gaim_roomlist_set_in_progress(list, FALSE);
		return;
	}

	url = g_strdup_printf("%s?chatroom_%s=0",
	                      gaim_account_get_string(
	                      list->account,
	                      "room_list", YAHOO_ROOMLIST_URL), id);

	yrl = g_new0(struct yahoo_roomlist, 1);
	yrl->list = list;
	yrl->cat = catagory;
	list->proto_data = g_list_append(list->proto_data, yrl);

	gaim_url_parse(url, &(yrl->host), NULL, &(yrl->path));
	g_free(url);

	yrl->ucat = gaim_roomlist_room_new(GAIM_ROOMLIST_ROOMTYPE_CATAGORY, _("User Rooms"), yrl->cat);
	gaim_roomlist_room_add(list, yrl->ucat);

	if (gaim_proxy_connect(list->account,
	                       yrl->host, 80, yahoo_roomlist_got_connected, yrl) != 0)
	{
		gaim_notify_error(gaim_account_get_connection(list->account),
		                  NULL, _("Connection problem"), _("Unable to fetch room list."));
		yahoo_roomlist_cleanup(list, yrl);
		return;
	}

	gaim_roomlist_set_in_progress(list, TRUE);
	gaim_roomlist_ref(list);
}

