/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>

#include "gaim.h"

static gint UI_fd = -1;
struct UI {
	GIOChannel *channel;
	guint inpa;
};
GSList *uis = NULL;

gint UI_write(struct UI *ui, guchar *data, gint len)
{
	guchar *send = g_new0(guchar, len + 6);
	gint sent;
	send[0] = 'f';
	send[1] = 1;
	memcpy(send + 2, &len, sizeof(len));
	memcpy(send + 6, data, len);
	/* we'll let the write silently fail because the read will pick it up as dead */
	g_io_channel_write(ui->channel, send, len + 6, &sent);
	return sent;
}

void UI_broadcast(guchar *data, gint len)
{
	GSList *u = uis;
	while (u) {
		struct UI *ui = u->data;
		UI_write(ui, data, len);
		u = u->next;
	}
}

static gint gaim_recv(GIOChannel *source, void *buf, gint len)
{
	gint total = 0;
	gint cur;

	while (total < len) {
		if (g_io_channel_read(source, buf + total, len - total, &cur) != G_IO_ERROR_NONE)
			return -1;
		if (cur == 0)
			return total;
		total += cur;
	}

	return total;
}

static gboolean UI_readable(GIOChannel *source, GIOCondition cond, gpointer data)
{
	struct UI *ui = data;

	guchar type;
	guchar subtype;
	guint32 len;

	guchar *in;

	/* no byte order worries! this'll change if we go to TCP */
	if (gaim_recv(source, &type, sizeof(type)) != sizeof(type)) {
		debug_printf("UI has abandoned us!\n");
		uis = g_slist_remove(uis, ui);
		g_io_channel_close(ui->channel);
		g_source_remove(ui->inpa);
		g_free(ui);
		return FALSE;
	}

	if (gaim_recv(source, &subtype, sizeof(subtype)) != sizeof(subtype)) {
		debug_printf("UI has abandoned us!\n");
		uis = g_slist_remove(uis, ui);
		g_io_channel_close(ui->channel);
		g_source_remove(ui->inpa);
		g_free(ui);
		return FALSE;
	}

	if (gaim_recv(source, &len, sizeof(len)) != sizeof(len)) {
		debug_printf("UI has abandoned us!\n");
		uis = g_slist_remove(uis, ui);
		g_io_channel_close(ui->channel);
		g_source_remove(ui->inpa);
		g_free(ui);
		return FALSE;
	}

	in = g_new0(guchar, len);
	if (gaim_recv(source, in, len) != len) {
		debug_printf("UI has abandoned us!\n");
		uis = g_slist_remove(uis, ui);
		g_io_channel_close(ui->channel);
		g_source_remove(ui->inpa);
		g_free(ui);
		return FALSE;
	}

	switch (type) {
			/*
		case CUI_TYPE_META:
			meta_handler(ui, in);
			break;
		case CUI_TYPE_PLUGIN:
			plugin_handler(ui, in);
			break;
		case CUI_TYPE_USER:
			user_handler(ui, in);
			break;
		case CUI_TYPE_CONN:
			conn_handler(ui, in);
			break;
		case CUI_TYPE_BUDDY:
			buddy_handler(ui, in);
			break;
		case CUI_TYPE_MESSAGE:
			message_handler(ui, in);
			break;
		case CUI_TYPE_CHAT:
			chat_handler(ui, in);
			break;
			*/
		default:
			debug_printf("unhandled type %d\n", type);
			break;
	}

	g_free(in);
	return TRUE;
}

static gboolean socket_readable(GIOChannel *source, GIOCondition cond, gpointer data)
{
	struct sockaddr_un saddr;
	gint len;
	gint fd;

	struct UI *ui;

	if ((fd = accept(UI_fd, (struct sockaddr *)&saddr, &len)) == -1)
		return FALSE;

	ui = g_new0(struct UI, 1);
	uis = g_slist_append(uis, ui);

	ui->channel = g_io_channel_unix_new(fd);
	ui->inpa = g_io_add_watch(ui->channel, G_IO_IN | G_IO_HUP | G_IO_ERR, UI_readable, ui);
	g_io_channel_unref(ui->channel);

	debug_printf("got one\n");
	return TRUE;
}

static gint open_socket()
{
	struct sockaddr_un saddr;
	gint fd;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) != -1) {
		umask(0177);
		saddr.sun_family = AF_UNIX;
		g_snprintf(saddr.sun_path, 108, "%s/gaim_%s.%d",
				g_get_tmp_dir(), g_get_user_name(), getpid());
		if (bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)) != -1)
			listen(fd, 100);
		else
			g_log(NULL, G_LOG_LEVEL_CRITICAL,
					"Failed to assign %s to a socket (Error: %s)",
					saddr.sun_path, strerror(errno));
	} else
		g_log(NULL, G_LOG_LEVEL_CRITICAL, "Unable to open socket: %s", strerror(errno));
	return fd;
}

int core_main()
{
	/*
	GMainLoop *loop;
	 */

	GIOChannel *channel;

	UI_fd = open_socket();
	if (UI_fd < 0)
		return 1;

	channel = g_io_channel_unix_new(UI_fd);
	g_io_add_watch(channel, G_IO_IN, socket_readable, NULL);
	g_io_channel_unref(channel);

	/*
	loop = g_main_new(TRUE);
	g_main_run(loop);
	 */

	return 0;
}

void core_quit()
{
	char buf[1024];
	close(UI_fd);
	sprintf(buf, "%s/gaim_%s.%d", g_get_tmp_dir(), g_get_user_name(), getpid());
	unlink(buf);
}
