/*
 * gaim - Napster Protocol Plugin
 *
 * Copyright (C) 2000-2001, Rob Flynn <rob@tgflinux.com>
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

#include <config.h>

#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "multi.h"
#include "prpl.h"
#include "gaim.h"
#include "proxy.h"
#include "pixmaps/napster.xpm"

#define NAP_BUF_LEN 4096

GSList *nap_connections = NULL;

static unsigned int chat_id = 0;

struct nap_channel {
	unsigned int id;
	gchar *name;
};

struct nap_data {
	int fd;
	int inpa;

	gchar *email;
	GSList *channels;
};

static char *nap_name()
{
	return "Napster";
}


/* FIXME: Make this use va_arg stuff */
static void nap_write_packet(struct gaim_connection *gc, unsigned short command, char *message)
{
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	unsigned short size;

	size = strlen(message);

	write(ndata->fd, &size, 2);
	write(ndata->fd, &command, 2);
	write(ndata->fd, message, size);
}

static int nap_send_im(struct gaim_connection *gc, char *who, char *message, int flags)
{
	gchar buf[NAP_BUF_LEN];

	g_snprintf(buf, NAP_BUF_LEN, "%s %s", who, message);
	nap_write_packet(gc, 0xCD, buf);

	return 1;
}

static struct nap_channel *find_channel_by_name(struct gaim_connection *gc, char *name)
{
	struct nap_channel *channel;
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	GSList *channels;

	channels = ndata->channels;

	while (channels) {
		channel = (struct nap_channel *)channels->data;

		if (channel) {
			if (!g_strcasecmp(name, channel->name)) {
				return channel;
			}
		}
		channels = g_slist_next(channels);
	}

	return NULL;
}

static struct nap_channel *find_channel_by_id(struct gaim_connection *gc, int id)
{
	struct nap_channel *channel;
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	GSList *channels;

	channels = ndata->channels;

	while (channels) {
		channel = (struct nap_channel *)channels->data;
		if (id == channel->id) {
			return channel;
		}

		channels = g_slist_next(channels);
	}

	return NULL;
}

static struct conversation *find_conversation_by_id(struct gaim_connection *gc, int id)
{
	GSList *bc = gc->buddy_chats;
	struct conversation *b = NULL;

	while (bc) {
		b = (struct conversation *)bc->data;
		if (id == b->id) {
			break;
		}
		bc = bc->next;
		b = NULL;
	}

	return b;
}

static void nap_callback(gpointer data, gint source, GaimInputCondition condition)
{
	struct gaim_connection *gc = data;
	struct nap_data *ndata = gc->proto_data;
	gchar *buf;
	unsigned short header[2];
	int len;
	int command;
	gchar **res;
	int i;

	if (recv(source, header, 4, 0) != 4) {
		hide_login_progress(gc, "Unable to read");
		signoff(gc);
		return;
	}

	len = header[0];
	command = header[1];	

	buf = (gchar *)g_malloc(sizeof(gchar) * (len + 1));

	i = 0;
	do {
		int tmp = recv(source, buf + i, len - i, 0);
		if (tmp <= 0) {
			g_free(buf);
			hide_login_progress(gc, "Unable to read");
			signoff(gc);
			return;
		}
		i += tmp;
	} while (i != len);

	buf[len] = 0;
	
	debug_printf("DEBUG: %s\n", buf);

	if (command == 0xd6) {
		res = g_strsplit(buf, " ", 0);
		/* Do we want to report what the users are doing? */ 
		printf("users: %s, files: %s, size: %sGB\n", res[0], res[1], res[2]);
		g_strfreev(res);
		g_free(buf);
		return;
	}

	if (command == 0x26d) {
		/* Do we want to use the MOTD? */
		g_free(buf);
		return;
	}

	if (command == 0xCD) {
		res = g_strsplit(buf, " ", 1);
		serv_got_im(gc, res[0], res[1], 0, time((time_t)NULL));
		g_strfreev(res);
		g_free(buf);
		return;
	}

	if (command == 0x195) {
		struct nap_channel *channel;
	
		channel = find_channel_by_name(gc, buf);

		if (!channel) {
			chat_id++;

			channel = g_new0(struct nap_channel, 1);

			channel->id = chat_id;
			channel->name = g_strdup(buf);

			ndata->channels = g_slist_append(ndata->channels, channel);

			serv_got_joined_chat(gc, chat_id, buf);
		}

		g_free(buf);
		return;
	}

	if (command == 0x198 || command == 0x196) {
		struct nap_channel *channel;
		struct conversation *convo;
		gchar **res;

		res = g_strsplit(buf, " ", 0);

		channel = find_channel_by_name(gc, res[0]);
		convo = find_conversation_by_id(gc, channel->id);

		add_chat_buddy(convo, res[1]);

		g_strfreev(res);

		g_free(buf);
		return;
	}

	if (command == 0x197) {
		struct nap_channel *channel;
		struct conversation *convo;
		gchar **res;

		res = g_strsplit(buf, " ", 0);
		
		channel = find_channel_by_name(gc, res[0]);
		convo = find_conversation_by_id(gc, channel->id);

		remove_chat_buddy(convo, res[1], NULL);
		
		g_strfreev(res);
		g_free(buf);
		return;
	}

	if (command == 0x193) {
		gchar **res;
		struct nap_channel *channel;

		res = g_strsplit(buf, " ", 2);

		channel = find_channel_by_name(gc, res[0]);

		if (channel)
			serv_got_chat_in(gc, channel->id, res[1], 0, res[2], time((time_t)NULL));

		g_strfreev(res);
		g_free(buf);
		return;
	}

	if (command == 0x194) {
		do_error_dialog(buf, "Gaim: Napster Error");
		g_free(buf);
		return;
	}

	if (command == 0x12e) {
		gchar buf2[NAP_BUF_LEN];

		g_snprintf(buf2, NAP_BUF_LEN, "Unable to add '%s' to your hotlist", buf);
		do_error_dialog(buf2, "Gaim: Napster Error");

		g_free(buf);
		return;

	}

	if (command == 0x191) {
		struct nap_channel *channel;

		channel = find_channel_by_name(gc, buf);

		if (!channel) /* I'm not sure how this would happen =) */
			return;

		serv_got_chat_left(gc, channel->id);
		ndata->channels = g_slist_remove(ndata->channels, channel);

		g_free(buf);
		return;
		
	}

	if (command == 0xd1) {
		gchar **res;

		res = g_strsplit(buf, " ", 0);

		serv_got_update(gc, res[0], 1, 0, 0, 0, 0, 0);
		
		g_strfreev(res);
		g_free(buf);
		return;
	}

	if (command == 0xd2) {
		serv_got_update(gc, buf, 0, 0, 0, 0, 0, 0);
		g_free(buf);
		return;
	}

	if (command == 0x12d) {
		/* Our buddy was added successfully */
		g_free(buf);
		return;
	}

	if (command == 0x2ec) {
		/* Looks like someone logged in as us! =-O */
		g_free(buf);

		signoff(gc);
		return;
	}

	debug_printf("NAP: [COMMAND: 0x%04x] %s\n", command, buf);

	g_free(buf);
}


static void nap_login_callback(gpointer data, gint source, GaimInputCondition condition)
{
	struct gaim_connection *gc = data;
	struct nap_data *ndata = gc->proto_data;
	gchar buf[NAP_BUF_LEN];
	unsigned short header[2];
	int len;
	int command;

	read(source, header, 4);
	len = header[0];
	command = header[1];	

	read(source, buf, len);
	buf[len] = 0;

	/* If we have some kind of error, get outta here */
	if (command == 0x00)
	{
		do_error_dialog(buf, "Gaim: Napster Error");
		gaim_input_remove(ndata->inpa);
		ndata->inpa = 0;
		close(source);
		signoff(gc);
		return;
	}

	if (command == 0x03) {
		printf("Registered with E-Mail address of: %s\n", buf);
		ndata->email = g_strdup(buf);

		/* Remove old inpa, add new one */
		gaim_input_remove(ndata->inpa);
		ndata->inpa = 0;
		gc->inpa = gaim_input_add(ndata->fd, GAIM_INPUT_READ, nap_callback, gc);

		/* Our signon is complete */
		account_online(gc);
		serv_finish_login(gc);

		if (bud_list_cache_exists(gc))
			do_import(gc, NULL);

		return;
	}
}


static void nap_login_connect(gpointer data, gint source, GaimInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct nap_data *ndata;
	char buf[NAP_BUF_LEN];

	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	if (source < 0) {
		hide_login_progress(gc, "Unable to connect");
		signoff(gc);
		return;
	}

	ndata = gc->proto_data;
	if (ndata->fd != source)
		ndata->fd = source;

	/* And write our signon data */
	g_snprintf(buf, NAP_BUF_LEN, "%s %s 0 \"gaimster\" 0", gc->username, gc->password);
	nap_write_packet(gc, 0x02, buf);
	
	/* And set up the input watcher */
	ndata->inpa = gaim_input_add(ndata->fd, GAIM_INPUT_READ, nap_login_callback, gc);
}


static void nap_login(struct aim_user *user)
{
	struct gaim_connection *gc = new_gaim_conn(user);
	struct nap_data *ndata = gc->proto_data = g_new0(struct nap_data, 1);

	ndata->fd = proxy_connect("64.124.41.187", 8888, nap_login_connect, gc);
	if (ndata->fd < 0) {
		hide_login_progress(gc, "Unable to connect");
		signoff(gc);
	}
}

static GList *nap_chat_info(struct gaim_connection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Join what group:");
	m = g_list_append(m, pce);

	return m;
}

static void nap_join_chat(struct gaim_connection *gc, GList *data)
{
	gchar buf[NAP_BUF_LEN];
	char *name;

	if (!data)
		return;

	name = data->data;

	/* Make sure the name has a # preceeding it */
	if (name[0] != '#') 
		g_snprintf(buf, NAP_BUF_LEN, "#%s", name);
	else
		g_snprintf(buf, NAP_BUF_LEN, "%s", name);

	nap_write_packet(gc, 0x190, buf);
}

static void nap_chat_leave(struct gaim_connection *gc, int id)
{
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	struct nap_channel *channel = NULL;
	
	channel = find_channel_by_id(gc, id);

	if (!channel) /* Again, I'm not sure how this would happen */
		return;

	nap_write_packet(gc, 0x191, channel->name);
	
	ndata->channels = g_slist_remove(ndata->channels, channel);
	g_free(channel->name);
	g_free(channel);
	
}

static int nap_chat_send(struct gaim_connection *gc, int id, char *message)
{
	struct nap_channel *channel = NULL;
	gchar buf[NAP_BUF_LEN];
	
	channel = find_channel_by_id(gc, id);

	if (!channel) {
		/* This shouldn't happen */
		return -EINVAL;
	}

	g_snprintf(buf, NAP_BUF_LEN, "%s %s", channel->name, message);
	nap_write_packet(gc, 0x192, buf);
	return 0;
}

static void nap_add_buddy(struct gaim_connection *gc, char *name)
{
	nap_write_packet(gc, 0xCF, name);
}

static void nap_remove_buddy(struct gaim_connection *gc, char *name, char *group)
{
	nap_write_packet(gc, 0x12F, name);
}

static void nap_close(struct gaim_connection *gc)
{
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	struct nap_channel *channel;
	
	if (gc->inpa)
		gaim_input_remove(gc->inpa);

	while (ndata->channels) {
		channel = (struct nap_channel *)ndata->channels->data;
		g_free(channel->name);
		ndata->channels = g_slist_remove(ndata->channels, channel);
		g_free(channel);
	}

	g_free(gc->proto_data);
}

static void nap_add_buddies(struct gaim_connection *gc, GList *buddies)
{
	while (buddies) {
		nap_write_packet(gc, 0xd0, (char *)buddies->data);
		buddies = buddies -> next;
	}
}

static char** nap_list_icon(int uc)
{
	return napster_xpm;
}

static struct prpl *my_protocol = NULL;

void napster_init(struct prpl *ret)
{
	ret->protocol = PROTO_NAPSTER;
	ret->name = nap_name;
	ret->list_icon = nap_list_icon;
	ret->user_opts = NULL;
	ret->login = nap_login;
	ret->close = nap_close;
	ret->send_im = nap_send_im;
	ret->set_info = NULL;
	ret->get_info = NULL;
	ret->set_away = NULL;
	ret->set_dir = NULL;
	ret->get_dir = NULL;
	ret->dir_search = NULL;
	ret->set_idle = NULL;
	ret->change_passwd = NULL;
	ret->add_buddy = nap_add_buddy;
	ret->add_buddies = nap_add_buddies;
	ret->remove_buddy = nap_remove_buddy;
	ret->add_permit = NULL;
	ret->rem_permit = NULL;
	ret->add_deny = NULL;
	ret->rem_deny = NULL;
	ret->warn = NULL;
	ret->chat_info = nap_chat_info;
	ret->join_chat = nap_join_chat;
	ret->chat_invite = NULL;
	ret->chat_leave = nap_chat_leave;
	ret->chat_whisper = NULL;
	ret->chat_send = nap_chat_send;
	ret->keepalive = NULL;

	my_protocol = ret;
}

#ifndef STATIC

char *gaim_plugin_init(GModule * handle)
{
	load_protocol(napster_init, sizeof(struct prpl));
	return NULL;
}

void gaim_plugin_remove()
{
	struct prpl *p = find_prpl(PROTO_NAPSTER);
	if (p == my_protocol)
		unload_protocol(p);
}

char *name()
{
	return "Napster";
}

char *description()
{
	return PRPL_DESC("Napster");
}

#endif
