/*
 * gaim - Napster Protocol Plugin
 *
 * Copyright (C) 2000, Rob Flynn <rob@tgflinux.com>
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

#include "../config.h"

#include <netdb.h>
#include <gtk/gtk.h>
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
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>
#include "multi.h"
#include "prpl.h"
#include "gaim.h"
#include "pixmaps/napster.xpm"

#define NAP_BUF_LEN 4096

GSList *nap_connections = NULL;

static unsigned int chat_id = 0;

struct browse_window {
	GtkWidget *window;
	GtkWidget *list;
	struct gaim_connection *gc;
	char *name;
};

struct nap_download_box {
	GtkWidget *window;
	GtkWidget *ok;
	GtkWidget *entry;
	gchar *who;
};

struct nap_channel {
	unsigned int id;
	gchar *name;
};

struct nap_file_request {
	gchar *name;
	gchar *file;
	int fd;
	long size;
	long total;
	int status;
	int inpa;
	FILE *mp3;
};

struct nap_data {
	int fd;
	int inpa;

	gchar *email;
	GSList *channels;
	GSList *requests;
	GSList *browses;
};

static char *nap_name()
{
	return "Napster";
}

char *name()
{
	return "Napster";
}

char *description()
{
	return "Allows gaim to use the Napster protocol.  Yes, kids, drugs are bad.";
}


/* FIXME: Make this use va_arg stuff */
void nap_write_packet(struct gaim_connection *gc, unsigned short command, char *message)
{
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	unsigned short size;

	size = strlen(message);

	write(ndata->fd, &size, 2);
	write(ndata->fd, &command, 2);
	write(ndata->fd, message, size);
}

void nap_send_download_req(struct gaim_connection *gc, char *who, char *file)
{
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	gchar buf[NAP_BUF_LEN];

	g_snprintf(buf, NAP_BUF_LEN, "%s \"%s\"", who, file);

	nap_write_packet(gc, 0xCB, buf);
}

void nap_handle_download(GtkCList *clist, gint row, gint col, GdkEventButton *event, gpointer user_data)
{
	gchar **results;
	struct browse_window *bw = (struct browse_window *)user_data;

	gtk_clist_get_text(GTK_CLIST(bw->list), row, 0, results);	

	nap_send_download_req(bw->gc, bw->name, results[0]);

}

struct browse_window *browse_window_new(struct gaim_connection *gc, char *name)
{
	struct browse_window *browse = g_new0(struct browse_window, 1);
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;

	browse->window = gtk_window_new(GTK_WINDOW_DIALOG);
	browse->name = g_strdup(name);
	browse->list = gtk_clist_new(1);
	browse->gc = gc;

	gtk_widget_show(browse->list);
	gtk_container_add(GTK_CONTAINER(browse->window), browse->list);

	gtk_widget_set_usize(GTK_WIDGET(browse->window), 300, 250);
	gtk_widget_show(browse->window);

	/*FIXME: I dont like using select-row.  Im lazy. Ill fix it later */
	gtk_signal_connect(GTK_OBJECT(browse->list), "select-row", GTK_SIGNAL_FUNC(nap_handle_download), browse);

	ndata->browses = g_slist_append(ndata->browses, browse);
}

void browse_window_add_file(struct browse_window *bw, char *name)
{
	char *fn[1];
	fn[0] = strdup(name);
	printf("User '%s' has file '%s'\n", bw->name, name);
	gtk_clist_append(GTK_CLIST(bw->list), fn);

	free(fn[0]);
}

static struct browse_window *find_browse_window_by_name(struct gaim_connection *gc, char *name)
{
	struct browse_window *browse;
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	GSList *browses;

	browses = ndata->browses;

	while (browses) {
		browse = (struct browse_window *)browses->data;

		if (browse) {
			if (!g_strcasecmp(name, browse->name)) {
				return browse;
			}
		}
		browses = g_slist_next(browses);
	}

	return NULL;
}

static void nap_send_im(struct gaim_connection *gc, char *who, char *message, int away)
{
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	gchar buf[NAP_BUF_LEN];

	g_snprintf(buf, NAP_BUF_LEN, "%s %s", who, message);
	nap_write_packet(gc, 0xCD, buf);
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
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
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

	if (!b) {
		return NULL;
	}

	return b;
}

/* This is a strange function.  I smoke too many bad bad things :-) */
struct nap_file_request * find_request_by_fd(struct gaim_connection *gc, int fd)
{
	struct nap_file_request *req;
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	GSList *requests;

	requests = ndata->requests;

	while (requests) {
		req = (struct nap_file_request *)requests->data;

		if (req) {
			if (req->fd == fd)
				return req;
		}
		requests = g_slist_next(requests);
	}

	return NULL;
}

static void nap_ctc_callback(gpointer data, gint source, GdkInputCondition condition)
{
	struct gaim_connection *gc = (struct gaim_connection *)data;
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	struct nap_file_request *req;
	unsigned char *buf;
	int i = 0;
	gchar *c;
	int len;

	req = find_request_by_fd(gc, source);
	if (!req) /* Something bad happened */
		return;
	
	buf = (char *)malloc(sizeof(char) * (NAP_BUF_LEN + 1));

	if (req->status == 0)
	{
		int j;
		gchar tmp[32];
		long filesize;
		gchar **parse_name;
		gchar path[2048];


		recv(source, buf, 1, 0);

		/* We should receive a '1' upon connection */
		if (buf[0] != '1')
		{
			do_error_dialog("Uh Oh", "Uh Oh");
			gdk_input_remove(req->inpa);
			ndata->requests = g_slist_remove(ndata->requests, req);
			g_free(req->name);
			g_free(req->file);
			close(source);
			g_free(req);
			free(buf);
			return;
		}
		
		/* Lets take a peek at the awaiting data */
		i = recv(source, buf, NAP_BUF_LEN, MSG_PEEK);
		buf[i] = 0; /* Make sure that we terminate our string */

		/* Looks like the uploader sent the proper data. Let's see how big the 
		 * file is */

		for (j = 0, i = 0; isdigit(buf[i]); i++, j++)
		{
			tmp[j] = buf[i];
		}	
		tmp[j] = 0;
		filesize = atol(tmp);

		/* Save the size of the file */
		req->total = filesize;

		/* If we have a zero file size then something bad happened */
		if (filesize == 0) {
			gdk_input_remove(req->inpa);
			ndata->requests = g_slist_remove(ndata->requests, req);
			g_free(req->name);
			g_free(req->file);
			g_free(req);
			free(buf);
			close(source);
			return;
		}

		/* Now that we've done that, let's go ahead and read that
		 * data to get it out of the way */
		recv(source, buf, strlen(tmp), 0); 

		/* Now, we should tell the server that we're download something */
		nap_write_packet(gc, 0xda, "\n");

		req->status = 1;

		/* FIXME: We dont want to force the file name.  I'll parse this
		 * later */

		parse_name = g_strsplit(req->file, "\\", 0);
		g_snprintf(path, sizeof(path), "%s/%s", getenv("HOME"), parse_name[sizeof(parse_name)]);
		printf("Gonna try to save to: %s\n", path);
		g_strfreev(parse_name);
		
		req->mp3 = fopen(path, "w");
		free(buf);
		return;
	}

	/* Looks like our status isn't 1.  It's safe to assume we're downloadin' */
	i = recv(source, buf, NAP_BUF_LEN, 0);

	req->size += i; /* Lets add up the total */

	printf("Downloaded %ld of %ld\n", req->size, req->total);

	fwrite(buf, i, sizeof(char), req->mp3);

	free(buf);

	if (req->size >= req->total) {
		printf("Download complete.\n");
		nap_write_packet(gc, 0xdb, "\n"); /* Tell the server we're finished */
		gdk_input_remove(req->inpa);
		ndata->requests = g_slist_remove(ndata->requests, req);
		g_free(req->name);
		g_free(req->file);
		g_free(req);
		fclose(req->mp3);
		close(source);
	}
}

void nap_get_file(struct gaim_connection *gc, gchar *user, gchar *file, unsigned long host, unsigned int port)
{
	int fd;
	struct sockaddr_in site;
	char buf[NAP_BUF_LEN];
	int inpa;
	struct nap_file_request *req = g_new0(struct nap_file_request, 1);
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;


	site.sin_family = AF_INET;
	site.sin_addr.s_addr = host;
	site.sin_port = htons(port);

	fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0) {
		do_error_dialog("Error connecting to user", "Gaim: Napster error");
		return;
	}

	/* Make a connection with the server */
	if (connect(fd, (struct sockaddr *)&site, sizeof(site)) < 0) {
		do_error_dialog("Error connecting to user", "Gaim: Napster error");
		return;
	 }
	
	req->fd = fd;
	req->name = g_strdup(user);
	req->file = g_strdup(file);
	req->size = 0;
	req->status = 0;
	req->total = 0;

	/* Send our request to the user */
	g_snprintf(buf, sizeof(buf), "GET%s \"%s\" 0\n", user, file);
	write(fd, buf, strlen(buf));

	/* Add our request */
	ndata->requests = g_slist_append(ndata->requests, req);

	/* And start monitoring */
	req->inpa = gdk_input_add(fd, GDK_INPUT_READ, nap_ctc_callback, gc);
}

static void nap_callback(gpointer data, gint source, GdkInputCondition condition)
{
	struct gaim_connection *gc = data;
	struct nap_data *ndata = gc->proto_data;
	gchar *buf;
	unsigned short header[2];
	int i = 0;
	int len;
	int command;
	gchar **res;

	read(source, header, 4);
	len = header[0];
	command = header[1];	

	buf = (gchar *)g_malloc(sizeof(gchar) * (len + 1));
	
	read(source, buf, len);

	buf[len] = 0;

	if (command == 0xd6) {
		res = g_strsplit(buf, " ", 0);
		/* Do we want to report what the users are doing? */ 
		printf("users: %s, files: %s, size: %sGB\n", res[0], res[1], res[2]);
		g_strfreev(res);
		free(buf);
		return;
	}

	if (command == 0x26d) {
		/* Do we want to use the MOTD? */
		free(buf);
		return;
	}

	if (command == 0xCD) {
		res = g_strsplit(buf, " ", 1);
		serv_got_im(gc, res[0], res[1], 0);
		g_strfreev(res);
		free(buf);
		return;
	}

	if (command == 0x195) {
		struct nap_channel *channel;
		int id;		
	
		channel = find_channel_by_name(gc, buf);

		if (!channel) {
			chat_id++;

			channel = g_new0(struct nap_channel, 1);

			channel->id = chat_id;
			channel->name = g_strdup(buf);

			ndata->channels = g_slist_append(ndata->channels, channel);

			serv_got_joined_chat(gc, chat_id, buf);
		}

		free(buf);
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

		free(buf);
		return;
	}

	if (command == 0x197) {
		struct nap_channel *channel;
		struct conversation *convo;
		gchar **res;

		res = g_strsplit(buf, " ", 0);
		
		channel = find_channel_by_name(gc, res[0]);
		convo = find_conversation_by_id(gc, channel->id);

		remove_chat_buddy(convo, res[1]);
		
		g_strfreev(res);
		free(buf);
		return;
	}

	if (command == 0x193) {
		gchar **res;
		struct nap_channel *channel;

		res = g_strsplit(buf, " ", 2);

		channel = find_channel_by_name(gc, res[0]);

		if (channel)
			serv_got_chat_in(gc, channel->id, res[1], 0, res[2]);

		g_strfreev(res);
		free(buf);
		return;
	}

	if (command == 0x194) {
		do_error_dialog(buf, "Gaim: Napster Error");
		free(buf);
		return;
	}

	if (command == 0x12e) {
		gchar buf2[NAP_BUF_LEN];

		g_snprintf(buf2, NAP_BUF_LEN, "Unable to add '%s' to your hotlist", buf);
		do_error_dialog(buf2, "Gaim: Napster Error");

		free(buf);
		return;

	}

	if (command == 0x191) {
		struct nap_channel *channel;

		channel = find_channel_by_name(gc, buf);

		if (!channel) /* I'm not sure how this would happen =) */
			return;

		serv_got_chat_left(gc, channel->id);
		ndata->channels = g_slist_remove(ndata->channels, channel);

		free(buf);
		return;
		
	}

	if (command == 0xd1) {
		gchar **res;

		res = g_strsplit(buf, " ", 0);

		serv_got_update(gc, res[0], 1, 0, time((time_t *)NULL), 0, 0, 0);
		
		g_strfreev(res);
		free(buf);
		return;
	}

	if (command == 0xd2) {
		serv_got_update(gc, buf, 0, 0, 0, 0, 0, 0);
		free(buf);
		return;
	}

	if (command == 0xd4) {
		/* Looks like we're getting a browse response */
		gchar user[64];
		gchar file[2048];
		struct browse_window *bw = NULL;
		
		int i,j;

		for (i = 0, j = 0; buf[i] != ' '; i++, j++)
		{
			user[j] = buf[i];
		}
		user[j] = 0; i++; i++;

		for (j = 0; buf[i] != '\"'; i++, j++)
		{
			file[j] = buf[i];
		}
		file[j] = 0;

		bw = find_browse_window_by_name(gc, user);
		if (!bw)
		{
			/* If a browse window isn't found, let's create one */
			bw = browse_window_new(gc, user);
		}

		browse_window_add_file(bw, file);
		
		free(buf);
		return;
		
	}

	if (command == 0x12d) {
		/* Our buddy was added successfully */
		free(buf);
		return;
	}

	if (command == 0x2ec) {
		/* Looks like someone logged in as us! =-O */
		free(buf);

		signoff(gc);
		return;
	}

	if (command == 0xcc) {
		/* We received a Download ACK from a user.  The way this is printed is kind of
		 * strange so we'll need to parse this one ourselves.  */

		gchar user[64];
		gchar file[2048];
		gchar hoststr[16];
		gchar portstr[16];
		int i,j;

		for (i = 0, j = 0; buf[i] != ' '; i++, j++)
		{
			user[j] = buf[i];
		}
		user[j] = 0; i++;

		for (j = 0; buf[i] != ' '; i++, j++)
		{
			hoststr[j] = buf[i];
		}
		hoststr[j] = 0; i++;

		for (j = 0; buf[i] != ' '; i++, j++)
		{
			portstr[j] = buf[i];
		}
		portstr[j] = 0; i++;

		i++; /* We do this to ignore the first quotation mark */

		for (j = 0; buf[i] != '\"'; i++, j++)
		{
			file[j] = buf[i];
		}
		file[j] = 0;

		/* Aaight.  We dont need nuttin' else. Let's download the file */
		nap_get_file(gc, user, file, atol(hoststr), atoi(portstr));

		free(buf);

		return;
	}

	printf("NAP: [COMMAND: 0x%04x] %s\n", command, buf);

	free(buf);
}


static void nap_login_callback(gpointer data, gint source, GdkInputCondition condition)
{
	struct gaim_connection *gc = data;
	struct nap_data *ndata = gc->proto_data;
	gchar buf[NAP_BUF_LEN];
	unsigned short header[2];
	int i = 0;
	int len;
	int command;

	read(source, header, 4);
	len = header[0];
	command = header[1];	
	
	read(source, buf, len);
	buf[len] = 0;

	if (command == 0x03) {
		printf("Registered with E-Mail address of: %s\n", buf);
		ndata->email = g_strdup(buf);

		/* Remove old inpa, add new one */
		gdk_input_remove(ndata->inpa);
		ndata->inpa = 0;
		gc->inpa = gdk_input_add(ndata->fd, GDK_INPUT_READ, nap_callback, gc);

		/* Our signon is complete */
		account_online(gc);
		serv_finish_login(gc);

		if (bud_list_cache_exists(gc))
			do_import(NULL, gc);

		return;
	}
}



static void nap_login(struct aim_user *user)
{
	int fd;
	struct hostent *host;
	struct sockaddr_in site;
	struct gaim_connection *gc = new_gaim_conn(user);
	struct nap_data *ndata = gc->proto_data = g_new0(struct nap_data, 1);
	char buf[NAP_BUF_LEN];
	char c;
	char z[4];
	int i;
	int status;

	host = gethostbyname("n184.napster.com");

	if (!host) {
		hide_login_progress(gc, "Unable to resolve hostname");
		signoff(gc);
		return;
	}

	site.sin_family = AF_INET;
	site.sin_addr.s_addr = *(long *)(host->h_addr);
	site.sin_port = htons(8888);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		hide_login_progress(gc, "Unable to create socket");
		signoff(gc);
		return;
	}

	/* Make a connection with the server */
	if (connect(fd, (struct sockaddr *)&site, sizeof(site)) < 0) {
		hide_login_progress(gc, "Unable to connect.");
		signoff(gc);
		 return;
	 }

	ndata->fd = fd;
	
	/* And write our signon data */
	g_snprintf(buf, NAP_BUF_LEN, "%s %s 0 \"Gaim - Napster Plugin\" 0", gc->username, gc->password);
	nap_write_packet(gc, 0x02, buf);
	
	/* And set up the input watcher */
	ndata->inpa = gdk_input_add(ndata->fd, GDK_INPUT_READ, nap_login_callback, gc);


}

static void nap_join_chat(struct gaim_connection *gc, int id, char *name)
{
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	gchar buf[NAP_BUF_LEN];

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
	GSList *channels = ndata->channels;
	
	channel = find_channel_by_id(gc, id);

	if (!channel) /* Again, I'm not sure how this would happen */
		return;

	nap_write_packet(gc, 0x191, channel->name);
	
	ndata->channels = g_slist_remove(ndata->channels, channel);
	g_free(channel->name);
	g_free(channel);
	
}

static void nap_chat_send(struct gaim_connection *gc, int id, char *message)
{
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	struct nap_channel *channel = NULL;
	gchar buf[NAP_BUF_LEN];
	
	channel = find_channel_by_id(gc, id);

	if (!channel) {
		/* This shouldn't happen */
		return;
	}

	g_snprintf(buf, NAP_BUF_LEN, "%s %s", channel->name, message);
	nap_write_packet(gc, 0x192, buf);

}

static void nap_add_buddy(struct gaim_connection *gc, char *name)
{
	nap_write_packet(gc, 0xCF, name);
}

static void nap_remove_buddy(struct gaim_connection *gc, char *name)
{
	nap_write_packet(gc, 0x12F, name);
}

static void nap_close(struct gaim_connection *gc)
{
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	gchar buf[NAP_BUF_LEN];
	struct nap_channel *channel;
	struct browse_window *browse;
	struct nap_file_request *req;
	GSList *channels = ndata->channels;
	GSList *requests = ndata->requests;
	
	if (gc->inpa)
		gdk_input_remove(gc->inpa);

	while (ndata->channels) {
		channel = (struct nap_channel *)ndata->channels->data;
		g_free(channel->name);
		ndata->channels = g_slist_remove(ndata->channels, channel);
		g_free(channel);
	}

	while (ndata->browses) {
		browse = (struct browse_window *)ndata->browses->data;
		g_free(browse->name);
		gtk_widget_destroy(browse->window);
		ndata->browses = g_slist_remove(ndata->browses, browse);
		g_free(browse);
	}

	while (ndata->requests) {
		req = (struct nap_file_request *)ndata->requests->data;
		g_free(req->name);
		g_free(req->file);
		if (req->inpa) {
			gdk_input_remove(req->inpa);
		}
		ndata->requests = g_slist_remove(ndata->requests, req);
		g_free(req);
		
	}

	free(gc->proto_data);
}

static void nap_add_buddies(struct gaim_connection *gc, GList *buddies)
{
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	gchar buf[NAP_BUF_LEN];
	int n = 0;

	while (buddies) {
		nap_write_packet(gc, 0xd0, (char *)buddies->data);
		buddies = buddies -> next;
	}
}

static void nap_draw_new_user(GtkWidget *box)
{
	GtkWidget *label;

	label = gtk_label_new(_("Napster registration is currently under development"));

	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);
}


void nap_send_browse(GtkObject *w, char *who)
{
	struct gaim_connection *gc = (struct gaim_connection *)gtk_object_get_user_data(w);
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	gchar buf[NAP_BUF_LEN];

	g_snprintf(buf, NAP_BUF_LEN, "%s", who);
	nap_write_packet(gc, 0xd3, buf);
}

static void nap_action_menu(GtkWidget *menu, struct gaim_connection *gc, char *who)
{
	GtkWidget *button;

	button = gtk_menu_item_new_with_label("List Files");
	gtk_signal_connect(GTK_OBJECT(button), "activate", GTK_SIGNAL_FUNC(nap_send_browse), who);
	gtk_object_set_user_data(GTK_OBJECT(button), gc);
	gtk_menu_append(GTK_MENU(menu), button);
	gtk_widget_show(button);
}

static char** nap_list_icon(int uc)
{
	return napster_xpm;
}

static struct prpl *my_protocol = NULL;

void nap_init(struct prpl *ret)
{
	ret->protocol = PROTO_NAPSTER;
	ret->name = nap_name;
	ret->list_icon = nap_list_icon;
	ret->action_menu = nap_action_menu;
	ret->user_opts = NULL;
	ret->login = nap_login;
	ret->close = nap_close;
	ret->send_im = nap_send_im;
	ret->set_info = NULL;
	ret->get_info = NULL;
	ret->set_away = NULL;
	ret->get_away_msg = NULL;
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
	ret->accept_chat = NULL;
	ret->join_chat = nap_join_chat;
	ret->chat_invite = NULL;
	ret->chat_leave = nap_chat_leave;
	ret->chat_whisper = NULL;
	ret->chat_send = nap_chat_send;
	ret->keepalive = NULL;
	ret->draw_new_user = nap_draw_new_user;

	my_protocol = ret;
}

char *gaim_plugin_init(GModule * handle)
{
	load_protocol(nap_init);
	return NULL;
}

void gaim_plugin_remove()
{
	struct prpl *p = find_prpl(PROTO_NAPSTER);
	if (p == my_protocol)
		unload_protocol(p);
}
