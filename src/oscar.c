/*
 * gaim
 *
 * Some code copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * libfaim code copyright 1998, 1999 Adam Fritzler <afritz@auk.cx>
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

#ifndef DYNAMIC_OSCAR

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif


#include <netdb.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "multi.h"
#include "prpl.h"
#include "gaim.h"
#include "aim.h"
#include "gnome_applet_mgr.h"

#include "pixmaps/admin_icon.xpm"
#include "pixmaps/aol_icon.xpm"
#include "pixmaps/away_icon.xpm"
#include "pixmaps/dt_icon.xpm"
#include "pixmaps/free_icon.xpm"

int gaim_caps = AIM_CAPS_CHAT | AIM_CAPS_SENDFILE | AIM_CAPS_GETFILE |
		AIM_CAPS_VOICE | AIM_CAPS_IMIMAGE | AIM_CAPS_BUDDYICON;

struct oscar_data {
	struct aim_session_t *sess;
	struct aim_conn_t *conn;

	int cnpa;
	int paspa;

	int create_exchange;
	char *create_name;

	GSList *oscar_chats;
};

struct chat_connection *find_oscar_chat(struct gaim_connection *gc, char *name) {
	GSList *g = ((struct oscar_data *)gc->proto_data)->oscar_chats;
	struct chat_connection *c = NULL;
	if (gc->protocol != PROTO_OSCAR) return NULL;

	while (g) {
		c = (struct chat_connection *)g->data;
		if (!strcmp(name, c->name))
			break;
		g = g->next;
		c = NULL;
	}

	return c;
}

static struct chat_connection *find_oscar_chat_by_conn(struct gaim_connection *gc,
							struct aim_conn_t *conn) {
	GSList *g = ((struct oscar_data *)gc->proto_data)->oscar_chats;
	struct chat_connection *c = NULL;

	while (g) {
		c = (struct chat_connection *)g->data;
		if (c->conn == conn)
			break;
		g = g->next;
		c = NULL;
	}

	return c;
}

static struct gaim_connection *find_gaim_conn_by_aim_sess(struct aim_session_t *sess) {
	GSList *g = connections;
	struct gaim_connection *gc = NULL;

	while (g) {
		gc = (struct gaim_connection *)g->data;
		if (sess == ((struct oscar_data *)gc->proto_data)->sess)
			break;
		g = g->next;
		gc = NULL;
	}

	return gc;
}

static struct gaim_connection *find_gaim_conn_by_oscar_conn(struct aim_conn_t *conn) {
	GSList *g = connections;
	struct gaim_connection *c = NULL;
	struct aim_conn_t *s;
	while (g) {
		c = (struct gaim_connection *)g->data;
		if (c->protocol != PROTO_OSCAR) {
			c = NULL;
			g = g->next;
			continue;
		}
		s = ((struct oscar_data *)c->proto_data)->sess->connlist;
		while (s) {
			if (conn == s)
				break;
			s = s->next;
		}
		if (s) break;
		g = g->next;
		c = NULL;
	}

	return c;
}

static int gaim_parse_auth_resp  (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_login      (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_server_ready     (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_handle_redirect  (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_oncoming   (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_offgoing   (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_incoming_im(struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_misses     (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_user_info  (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_motd       (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_chatnav_info     (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_chat_join        (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_chat_leave       (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_chat_info_update (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_chat_incoming_msg(struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_msgack     (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_ratechange (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_evilnotify (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_bosrights        (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_rateresp         (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_reportinterval   (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_msgerr     (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_buddyrights(struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_locerr     (struct aim_session_t *, struct command_rx_struct *, ...);

static char *msgerrreason[] = {
	"Invalid error",
	"Invalid SNAC",
	"Rate to host",
	"Rate to client",
	"Not logged in",
	"Service unavailable",
	"Service not defined",
	"Obsolete SNAC",
	"Not supported by host",
	"Not supported by client",
	"Refused by client",
	"Reply too big",
	"Responses lost",
	"Request denied",
	"Busted SNAC payload",
	"Insufficient rights",
	"In local permit/deny",
	"Too evil (sender)",
	"Too evil (receiver)",
	"User temporarily unavailable",
	"No match",
	"List overflow",
	"Request ambiguous",
	"Queue full",
	"Not while on AOL"
};
static int msgerrreasonlen = 25;

static void oscar_callback(gpointer data, gint source,
				GdkInputCondition condition) {
	struct aim_conn_t *conn = (struct aim_conn_t *)data;
	struct gaim_connection *gc = find_gaim_conn_by_oscar_conn(conn);
	struct oscar_data *odata = (struct oscar_data *)gc->proto_data;
	if (!gc) {
		/* oh boy. this is probably bad. i guess the only thing we can really do
		 * is return? */
		debug_print("oscar callback for closed connection.\n");
		return;
	}

	if (condition & GDK_INPUT_EXCEPTION) {
		hide_login_progress(gc, _("Disconnected."));
		signoff(gc);
		return;
	}
	if (condition & GDK_INPUT_READ) {
		if (conn->type == AIM_CONN_TYPE_RENDEZVOUS_OUT) {
			debug_print("got information on rendezvous\n");
			if (aim_handlerendconnect(odata->sess, conn) < 0) {
				debug_print(_("connection error (rend)\n"));
			}
		} else {
			if (aim_get_command(odata->sess, conn) >= 0) {
				aim_rxdispatch(odata->sess);
			} else {
				if ((conn->type == AIM_CONN_TYPE_BOS) ||
					   !(aim_getconn_type(odata->sess, AIM_CONN_TYPE_BOS))) {
					debug_print(_("major connection error\n"));
					hide_login_progress(gc, _("Disconnected."));
					signoff(gc);
				} else if (conn->type == AIM_CONN_TYPE_CHAT) {
					struct chat_connection *c = find_oscar_chat_by_conn(gc, conn);
					char buf[BUF_LONG];
					sprintf(debug_buff, "disconnected from chat room %s\n", c->name);
					debug_print(debug_buff);
					c->conn = NULL;
					if (c->inpa > -1)
						gdk_input_remove(c->inpa);
					c->inpa = -1;
					c->fd = -1;
					aim_conn_kill(odata->sess, &conn);
					sprintf(buf, _("You have been disconnected from chat room %s."), c->name);
					do_error_dialog(buf, _("Chat Error!"));
				} else if (conn->type == AIM_CONN_TYPE_CHATNAV) {
					if (odata->cnpa > -1)
						gdk_input_remove(odata->cnpa);
					odata->cnpa = -1;
					debug_print("removing chatnav input watcher\n");
					aim_conn_kill(odata->sess, &conn);
				} else {
					sprintf(debug_buff, "holy crap! generic connection error! %d\n",
							conn->type);
					debug_print(debug_buff);
					aim_conn_kill(odata->sess, &conn);
				}
			}
		}
	}
}

void oscar_login(struct aim_user *user) {
	struct aim_session_t *sess;
	struct aim_conn_t *conn;
	char buf[256];
	struct gaim_connection *gc = new_gaim_conn(PROTO_OSCAR, user->username, user->password);
	struct oscar_data *odata = gc->proto_data = g_new0(struct oscar_data, 1);
	gc->user = user;

	sprintf(debug_buff, _("Logging in %s\n"), user->username);
	debug_print(debug_buff);

	sess = g_new0(struct aim_session_t, 1);
	aim_session_init(sess);
	/* we need an immediate queue because we don't use a while-loop to
	 * see if things need to be sent. */
	sess->tx_enqueue = &aim_tx_enqueue__immediate;
	odata->sess = sess;

	sprintf(buf, _("Looking up %s"), FAIM_LOGIN_SERVER);
	set_login_progress(gc, 1, buf);
	/* this creates a possible race condition, but hey, what can you do */
	while (gtk_events_pending())
		gtk_main_iteration();
	conn = aim_newconn(sess, AIM_CONN_TYPE_AUTH, FAIM_LOGIN_SERVER);

	if (conn == NULL) {
		debug_print(_("internal connection error\n"));
		hide_login_progress(gc, _("Unable to login to AIM"));
		destroy_gaim_conn(gc);
		return;
	} else if (conn->fd == -1) {
		if (conn->status & AIM_CONN_STATUS_RESOLVERR) {
			sprintf(debug_buff, _("couldn't resolve host"));
			debug_print(debug_buff); debug_print("\n");
			hide_login_progress(gc, debug_buff);
		} else if (conn->status & AIM_CONN_STATUS_CONNERR) {
			sprintf(debug_buff, _("couldn't connect to host"));
			debug_print(debug_buff); debug_print("\n");
			hide_login_progress(gc, debug_buff);
		}
		destroy_gaim_conn(gc);
		return;
	}
	g_snprintf(buf, sizeof(buf), _("Signon: %s"), gc->username);
	set_login_progress(gc, 2, buf);

	aim_conn_addhandler(sess, conn, 0x0017, 0x0007, gaim_parse_login, 0);
	aim_conn_addhandler(sess, conn, 0x0017, 0x0003, gaim_parse_auth_resp, 0);
	aim_sendconnack(sess, conn);
	aim_request_login(sess, conn, gc->username);

	gc->inpa = gdk_input_add(conn->fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
			oscar_callback, conn);

	sprintf(gc->user_info, "%s", user->user_info);
	gc->options = user->options;
	save_prefs(); /* is this necessary anymore? */

	debug_print(_("Password sent, waiting for response\n"));
}

void oscar_close(struct gaim_connection *gc) {
	struct oscar_data *odata = (struct oscar_data *)gc->proto_data;
	GSList *c = odata->oscar_chats;
	struct chat_connection *n;
	if (gc->protocol != PROTO_OSCAR) return;
	
	while (c) {
		n = (struct chat_connection *)c->data;
		gdk_input_remove(n->inpa);
		g_free(n->name);
		c = g_slist_remove(c, n);
		g_free(n);
	}
	if (gc->inpa > 0)
		gdk_input_remove(gc->inpa);
	if (odata->cnpa > 0)
		gdk_input_remove(odata->cnpa);
	if (odata->paspa > 0)
		gdk_input_remove(odata->paspa);
	aim_logoff(odata->sess);
	g_free(odata->sess);
	g_free(gc->proto_data);
	debug_print(_("Signed off.\n"));
}

int gaim_parse_auth_resp(struct aim_session_t *sess,
			 struct command_rx_struct *command, ...) {
	struct aim_conn_t *bosconn = NULL;
	struct gaim_connection *gc = find_gaim_conn_by_aim_sess(sess);
	sprintf(debug_buff, "inside auth_resp (Screen name: %s)\n",
			sess->logininfo.screen_name);
	debug_print(debug_buff);

	if (sess->logininfo.errorcode) {
		switch (sess->logininfo.errorcode) {
		case 0x18:
			/* connecting too frequently */
			hide_login_progress(gc, _("You have been connecting and disconnecting too frequently. Wait ten minutes and try again. If you continue to try, you will need to wait even longer."));
			plugin_event(event_error, (void *)983, 0, 0, 0);
			break;
		case 0x05:
			/* Incorrect nick/password */
			hide_login_progress(gc, _("Incorrect nickname or password."));
			plugin_event(event_error, (void *)980, 0, 0, 0);
			break;
		case 0x1c:
			/* client too old */
			hide_login_progress(gc, _("The client version you are using is too old. Please upgrade at http://www.marko.net/gaim/"));
			plugin_event(event_error, (void *)989, 0, 0, 0);
			break;
		default:
			hide_login_progress(gc, _("Authentication Failed"));
			break;
		}
		sprintf(debug_buff, "Login Error Code 0x%04x\n",
				sess->logininfo.errorcode);
		debug_print(debug_buff);
		sprintf(debug_buff, "Error URL: %s\n",
				sess->logininfo.errorurl);
		debug_print(debug_buff);
#ifdef USE_APPLET
		set_user_state(offline);
#endif
		signoff(gc);
		return 0;
	}


	if (sess->logininfo.email) {
		sprintf(debug_buff, "Email: %s\n", sess->logininfo.email);
		debug_print(debug_buff);
	} else {
		debug_print("Email is NULL\n");
	}
	sprintf(debug_buff, "Closing auth connection...\n");
	debug_print(debug_buff);
	gdk_input_remove(gc->inpa);
	aim_conn_kill(sess, &command->conn);

	bosconn = aim_newconn(sess, AIM_CONN_TYPE_BOS, sess->logininfo.BOSIP);
	if (bosconn == NULL) {
#ifdef USE_APPLET
		set_user_state(offline);
#endif
		hide_login_progress(gc, _("Internal Error"));
		destroy_gaim_conn(gc);
		return -1;
	} else if (bosconn->status != 0) {
#ifdef USE_APPLET
		set_user_state(offline);
#endif
		hide_login_progress(gc, _("Could Not Connect"));
		destroy_gaim_conn(gc);
		return -1;
	}

	aim_conn_addhandler(sess, bosconn, 0x0009, 0x0003, gaim_bosrights, 0);
	aim_conn_addhandler(sess, bosconn, 0x0001, 0x0007, gaim_rateresp, 0); /* rate info */
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ACK, AIM_CB_ACK_ACK, NULL, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_SERVERREADY, gaim_server_ready, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_RATEINFO, NULL, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_REDIRECT, gaim_handle_redirect, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_STS, AIM_CB_STS_SETREPORTINTERVAL, gaim_reportinterval, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_RIGHTSINFO, gaim_parse_buddyrights, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_ONCOMING, gaim_parse_oncoming, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_OFFGOING, gaim_parse_offgoing, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_INCOMING, gaim_parse_incoming_im, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, AIM_CB_LOC_ERROR, gaim_parse_locerr, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_MISSEDCALL, gaim_parse_misses, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_RATECHANGE, gaim_parse_ratechange, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_EVIL, gaim_parse_evilnotify, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_ERROR, gaim_parse_msgerr, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, AIM_CB_LOC_USERINFO, gaim_parse_user_info, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_ACK, gaim_parse_msgack, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_CTN, AIM_CB_CTN_DEFAULT, aim_parse_unknown, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_DEFAULT, aim_parse_unknown, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_MOTD, gaim_parse_motd, 0);

	aim_auth_sendcookie(sess, bosconn, sess->logininfo.cookie);
	((struct oscar_data *)gc->proto_data)->conn = bosconn;
	gc->inpa = gdk_input_add(bosconn->fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
			oscar_callback, bosconn);
	set_login_progress(gc, 4, _("Connection established, cookie sent"));
	return 1;
}

int gaim_parse_login(struct aim_session_t *sess,
		     struct command_rx_struct *command, ...) {
	struct client_info_s info = {"AOL Instant Messenger (SM), version 4.1.2010/WIN32", 4, 30, 3141, "us", "en", 0x0004, 0x0001, 0x055};
	char *key;
	va_list ap;
	struct gaim_connection *gc = find_gaim_conn_by_aim_sess(sess);

	va_start(ap, command);
	key = va_arg(ap, char *);
	va_end(ap);

	aim_send_login(sess, command->conn, gc->username, gc->password, &info, key);
	return 1;
}

int gaim_server_ready(struct aim_session_t *sess,
		      struct command_rx_struct *command, ...) {
	static int id = 1;
	struct gaim_connection *gc = find_gaim_conn_by_aim_sess(sess);
	switch (command->conn->type) {
	case AIM_CONN_TYPE_BOS:
		aim_setversions(sess, command->conn);
		aim_bos_reqrate(sess, command->conn); /* request rate info */
		debug_print("done with BOS ServerReady\n");
		break;
	case AIM_CONN_TYPE_CHATNAV:
		debug_print("chatnav: got server ready\n");
		aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CTN, AIM_CB_CTN_INFO, gaim_chatnav_info, 0);
		aim_bos_reqrate(sess, command->conn);
		aim_bos_ackrateresp(sess, command->conn);
		aim_chatnav_clientready(sess, command->conn);
		aim_chatnav_reqrights(sess, command->conn);
		break;
	case AIM_CONN_TYPE_CHAT:
		debug_print("chat: got server ready\n");
		aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_USERJOIN, gaim_chat_join, 0);
		aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_USERLEAVE, gaim_chat_leave, 0);
		aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_ROOMINFOUPDATE, gaim_chat_info_update, 0);
		aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_INCOMINGMSG, gaim_chat_incoming_msg, 0);
		aim_bos_reqrate(sess, command->conn);
		aim_bos_ackrateresp(sess, command->conn);
		aim_chat_clientready(sess, command->conn);
		serv_got_joined_chat(gc, id++, aim_chat_getname(command->conn));
		break;
	case AIM_CONN_TYPE_RENDEZVOUS:
		break;
	default: /* huh? */
		sprintf(debug_buff, "server ready: got unexpected connection type %04x\n", command->conn->type);
		debug_print(debug_buff);
		break;
	}
	return 1;
}

int gaim_handle_redirect(struct aim_session_t *sess,
			 struct command_rx_struct *command, ...) {
	va_list ap;
	int serviceid;
	char *ip;
	unsigned char *cookie;
	struct gaim_connection *gc = find_gaim_conn_by_aim_sess(sess);
	struct oscar_data *odata = (struct oscar_data *)gc->proto_data;

	va_start(ap, command);
	serviceid = va_arg(ap, int);
	ip        = va_arg(ap, char *);
	cookie    = va_arg(ap, unsigned char *);

	switch(serviceid) {
	case 0x7: /* Authorizer */
		debug_print("Reconnecting with authorizor...\n");
		{
		struct aim_conn_t *tstconn = aim_newconn(sess, AIM_CONN_TYPE_AUTH, ip);
		if (tstconn == NULL || tstconn->status >= AIM_CONN_STATUS_RESOLVERR)
			debug_print("unable to reconnect with authorizer\n");
		else {
			odata->paspa = gdk_input_add(tstconn->fd,
					GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
					oscar_callback, tstconn);
			aim_auth_sendcookie(sess, tstconn, cookie);
		}
		}
		break;
	case 0xd: /* ChatNav */
		{
		struct aim_conn_t *tstconn = aim_newconn(sess, AIM_CONN_TYPE_CHATNAV, ip);
		if (tstconn == NULL || tstconn->status >= AIM_CONN_STATUS_RESOLVERR) {
			debug_print("unable to connect to chatnav server\n");
			return 1;
		}
		aim_conn_addhandler(sess, tstconn, 0x0001, 0x0003, gaim_server_ready, 0);
		aim_auth_sendcookie(sess, tstconn, cookie);
		odata->cnpa = gdk_input_add(tstconn->fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
					oscar_callback, tstconn);
		}
		debug_print("chatnav: connected\n");
		break;
	case 0xe: /* Chat */
		{
		struct aim_conn_t *tstconn = aim_newconn(sess, AIM_CONN_TYPE_CHAT, ip);
		char *roomname = va_arg(ap, char *);
		struct chat_connection *ccon;
		if (tstconn == NULL || tstconn->status >= AIM_CONN_STATUS_RESOLVERR) {
			debug_print("unable to connect to chat server\n");
			return 1;
		}
		sprintf(debug_buff, "Connected to chat room %s\n", roomname);
		debug_print(debug_buff);

		ccon = g_new0(struct chat_connection, 1);
		ccon->conn = tstconn;
		ccon->fd = tstconn->fd;
		ccon->name = g_strdup(roomname);
		
		ccon->inpa = gdk_input_add(tstconn->fd,
				GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
				oscar_callback, tstconn);

		odata->oscar_chats = g_slist_append(odata->oscar_chats, ccon);
		
		aim_chat_attachname(tstconn, roomname);
		aim_conn_addhandler(sess, tstconn, 0x0001, 0x0003, gaim_server_ready, 0);
		aim_auth_sendcookie(sess, tstconn, cookie);
		}
		break;
	default: /* huh? */
		sprintf(debug_buff, "got redirect for unknown service 0x%04x\n",
				serviceid);
		debug_print(debug_buff);
		break;
	}

	va_end(ap);

	return 1;
}

int gaim_parse_oncoming(struct aim_session_t *sess,
			struct command_rx_struct *command, ...) {
	struct aim_userinfo_s *info;
	time_t time_idle;
	int type = 0;
	struct gaim_connection *gc = find_gaim_conn_by_aim_sess(sess);

	va_list ap;
	va_start(ap, command);
	info = va_arg(ap, struct aim_userinfo_s *);
	va_end(ap);

	if (info->flags & AIM_FLAG_UNCONFIRMED)
		type |= UC_UNCONFIRMED;
	else if (info->flags & AIM_FLAG_ADMINISTRATOR)
		type |= UC_ADMIN;
	else if (info->flags & AIM_FLAG_AOL)
		type |= UC_AOL;
	else if (info->flags & AIM_FLAG_FREE)
		type |= UC_NORMAL;
	if (info->flags & AIM_FLAG_AWAY)
		type |= UC_UNAVAILABLE;

	if (info->idletime) {
		time(&time_idle);
		time_idle -= info->idletime*60;
	} else
		time_idle = 0;

	serv_got_update(gc, info->sn, 1, info->warnlevel/10, info->onlinesince,
			time_idle, type, info->capabilities);

	return 1;
}

int gaim_parse_offgoing(struct aim_session_t *sess,
			struct command_rx_struct *command, ...) {
	char *sn;
	va_list ap;
	struct gaim_connection *gc = find_gaim_conn_by_aim_sess(sess);

	va_start(ap, command);
	sn = va_arg(ap, char *);
	va_end(ap);

	serv_got_update(gc, sn, 0, 0, 0, 0, 0, 0);

	return 1;
}

int gaim_parse_incoming_im(struct aim_session_t *sess,
			   struct command_rx_struct *command, ...) {
	int channel;
	va_list ap;
	struct gaim_connection *gc = find_gaim_conn_by_aim_sess(sess);

	va_start(ap, command);
	channel = va_arg(ap, int);

	/* channel 1: standard message */
	if (channel == 1) {
		struct aim_userinfo_s *userinfo;
		char *msg = NULL;
		char *tmp = g_malloc(BUF_LONG);
		u_int icbmflags = 0;
		u_short flag1, flag2;

		userinfo  = va_arg(ap, struct aim_userinfo_s *);
		msg       = va_arg(ap, char *);
		icbmflags = va_arg(ap, u_int);
		flag1     = (u_short)va_arg(ap, u_int);
		flag2     = (u_short)va_arg(ap, u_int);
		va_end(ap);

		g_snprintf(tmp, BUF_LONG, "%s", msg);
		serv_got_im(gc, userinfo->sn, tmp, icbmflags & AIM_IMFLAGS_AWAY);
		g_free(tmp);
	} else if (channel == 2) {
		struct aim_userinfo_s *userinfo;
		int rendtype = va_arg(ap, int);
		if (rendtype & AIM_CAPS_CHAT) {
			char *msg, *encoding, *lang;
			struct aim_chat_roominfo *roominfo;

			userinfo = va_arg(ap, struct aim_userinfo_s *);
			roominfo = va_arg(ap, struct aim_chat_roominfo *);
			msg      = va_arg(ap, char *);
			encoding = va_arg(ap, char *);
			lang     = va_arg(ap, char *);
			va_end(ap);

			serv_got_chat_invite(gc,
					     roominfo->name,
					     roominfo->exchange,
					     userinfo->sn,
					     msg);
		} else if (rendtype & AIM_CAPS_SENDFILE) {
			/* libfaim won't tell us that we got this just yet */
		} else if (rendtype & AIM_CAPS_GETFILE) {
			/* nor will it tell us this. but it's still there */
		} else if (rendtype & AIM_CAPS_VOICE) {
			/* this one libfaim tells us unuseful info about  */
		} else if (rendtype & AIM_CAPS_BUDDYICON) {
			/* bah */
		} else if (rendtype & AIM_CAPS_IMIMAGE) {
			/* DirectIM stuff */
		} else {
			sprintf(debug_buff, "Unknown rendtype %d\n", rendtype);
			debug_print(debug_buff);
		}
	}

	return 1;
}

int gaim_parse_misses(struct aim_session_t *sess,
		      struct command_rx_struct *command, ...) {
	va_list ap;
	u_short chan, nummissed, reason;
	struct aim_userinfo_s *userinfo;
	char buf[1024];

	va_start(ap, command);
	chan = (u_short)va_arg(ap, u_int);
	userinfo = va_arg(ap, struct aim_userinfo_s *);
	nummissed = (u_short)va_arg(ap, u_int);
	reason = (u_short)va_arg(ap, u_int);
	va_end(ap);

	switch(reason) {
		case 1:
			/* message too large */
			sprintf(buf, _("You missed a message from %s because it was too large."), userinfo->sn);
			do_error_dialog(buf, _("Gaim - Error"));
			plugin_event(event_error, (void *)961, 0, 0, 0);
			break;
		default:
			sprintf(buf, _("You missed a message from %s for unknown reasons."), userinfo->sn);
			do_error_dialog(buf, _("Gaim - Error"));
			plugin_event(event_error, (void *)970, 0, 0, 0);
			break;
	}

	return 1;
}

int gaim_parse_msgerr(struct aim_session_t *sess,
		      struct command_rx_struct *command, ...) {
	va_list ap;
	char *destn;
	u_short reason;
	char buf[1024];

	va_start(ap, command);
	destn = va_arg(ap, char *);
	reason = (u_short)va_arg(ap, u_int);
	va_end(ap);

	sprintf(buf, _("Your message to %s did not get sent: %s"), destn,
			(reason < msgerrreasonlen) ? msgerrreason[reason] : _("Reason unknown"));
	do_error_dialog(buf, _("Gaim - Error"));

	return 1;
}

int gaim_parse_locerr(struct aim_session_t *sess,
		      struct command_rx_struct *command, ...) {
	va_list ap;
	char *destn;
	u_short reason;
	char buf[1024];

	va_start(ap, command);
	destn = va_arg(ap, char *);
	reason = (u_short)va_arg(ap, u_int);
	va_end(ap);

	sprintf(buf, _("User information for %s unavailable: %s"), destn,
			(reason < msgerrreasonlen) ? msgerrreason[reason] : _("Reason unknown"));
	do_error_dialog(buf, _("Gaim - Error"));

	return 1;
}

int gaim_parse_user_info(struct aim_session_t *sess,
			 struct command_rx_struct *command, ...) {
	struct aim_userinfo_s *info;
	char *prof_enc = NULL, *prof = NULL;
	u_short infotype;
	char buf[BUF_LONG];
	struct gaim_connection *gc = find_gaim_conn_by_aim_sess(sess);
	va_list ap;

	va_start(ap, command);
	info = va_arg(ap, struct aim_userinfo_s *);
	prof_enc = va_arg(ap, char *);
	prof = va_arg(ap, char *);
	infotype = (u_short)va_arg(ap, u_int);
	va_end(ap);

	if (prof == NULL || !strlen(prof)) {
		/* no info/away message */
		char buf[1024];
		sprintf(buf, _("%s has no info/away message."), info->sn);
		do_error_dialog(buf, _("Gaim - Error"));
		plugin_event(event_error, (void *)977, 0, 0, 0);
		return 1;
	}

	snprintf(buf, sizeof buf, _("Username : <B>%s</B>\n<BR>"
				  "Warning Level : <B>%d %%</B>\n<BR>"
				  "Online Since : <B>%s</B><BR>"
				  "Idle Minutes : <B>%d</B>\n<BR><HR><BR>"
				  "%s\n"),
				  info->sn,
				  info->warnlevel/10,
				  asctime(localtime(&info->onlinesince)),
				  info->idletime,
				  infotype == AIM_GETINFO_GENERALINFO ? prof :
 				  away_subs(prof, gc->username));
	g_show_info_text(away_subs(buf, gc->username));

	return 1;
}

int gaim_parse_motd(struct aim_session_t *sess,
		    struct command_rx_struct *command, ...) {
	char *msg;
	u_short id;
	va_list ap;
	struct gaim_connection *gc = find_gaim_conn_by_aim_sess(sess);

	va_start(ap, command);
	id  = (u_short)va_arg(ap, u_int);
	msg = va_arg(ap, char *);
	va_end(ap);

	sprintf(debug_buff, "MOTD: %s (%d)\n", msg, id);
	debug_print(debug_buff);
	sprintf(debug_buff, "Gaim %s / Libfaim %s\n",
			VERSION, aim_getbuildstring());
	debug_print(debug_buff);
	if (id != 4)
		do_error_dialog(_("Your connection may be lost."),
				_("AOL error"));

	if (gc->keepalive < 0)
		update_keepalive(gc, gc->keepalive);

	return 1;
}

int gaim_chatnav_info(struct aim_session_t *sess,
		      struct command_rx_struct *command, ...) {
	va_list ap;
	u_short type;
	struct gaim_connection *gc = find_gaim_conn_by_aim_sess(sess);
	struct oscar_data *odata = (struct oscar_data *)gc->proto_data;

	va_start(ap, command);
	type = (u_short)va_arg(ap, u_int);

	switch(type) {
		case 0x0002: {
			int maxrooms;
			struct aim_chat_exchangeinfo *exchanges;
			int exchangecount, i = 0;

			maxrooms = (u_char)va_arg(ap, u_int);
			exchangecount = va_arg(ap, int);
			exchanges = va_arg(ap, struct aim_chat_exchangeinfo *);
			va_end(ap);

			debug_print("chat info: Chat Rights:\n");
			sprintf(debug_buff, "chat info: \tMax Concurrent Rooms: %d\n", maxrooms);
			debug_print(debug_buff);
			sprintf(debug_buff, "chat info: \tExchange List: (%d total)\n", exchangecount);
			debug_print(debug_buff);
			while (i < exchangecount) {
				sprintf(debug_buff, "chat info: \t\t%x: %s (%s/%s)\n",
						exchanges[i].number,
						exchanges[i].name,
						exchanges[i].charset1,
						exchanges[i].lang1);
				debug_print(debug_buff);
				i++;
			}
			if (odata->create_exchange) {
				sprintf(debug_buff, "creating room %s\n", odata->create_name);
				debug_print(debug_buff);
				aim_chatnav_createroom(sess, command->conn, odata->create_name,
						odata->create_exchange);
				odata->create_exchange = 0;
				g_free(odata->create_name);
				odata->create_name = NULL;
			}
			}
			break;
		case 0x0008: {
			char *fqcn, *name, *ck;
			u_short instance, flags, maxmsglen, maxoccupancy, unknown;
			unsigned char createperms;
			unsigned long createtime;

			fqcn = va_arg(ap, char *);
			instance = (u_short)va_arg(ap, u_int);
			flags = (u_short)va_arg(ap, u_int);
			createtime = va_arg(ap, unsigned long);
			maxmsglen = (u_short)va_arg(ap, u_int);
			maxoccupancy = (u_short)va_arg(ap, u_int);
			createperms = (unsigned char)va_arg(ap, int);
			unknown = (u_short)va_arg(ap, u_int);
			name = va_arg(ap, char *);
			ck = va_arg(ap, char *);
			va_end(ap);

			sprintf(debug_buff, "created room: %s %d %d %lu %d %d %d %d %s %s\n", fqcn, instance, flags, createtime, maxmsglen, maxoccupancy, createperms, unknown, name, ck);
			debug_print(debug_buff);
			if (flags & 0x4) {
				sprintf(debug_buff, "joining %s on exchange 5\n", name);
				debug_print(debug_buff);
				aim_chat_join(odata->sess, odata->conn, 5, ck);
			} else 
				sprintf(debug_buff, "joining %s on exchange 4\n", name);{
				debug_print(debug_buff);
				aim_chat_join(odata->sess, odata->conn, 4, ck);
			}
			}
			break;
		default:
			va_end(ap);
			sprintf(debug_buff, "chatnav info: unknown type (%04x)\n", type);
			debug_print(debug_buff);
			break;
	}
	return 1;
}

int gaim_chat_join(struct aim_session_t *sess,
		   struct command_rx_struct *command, ...) {
	va_list ap;
	int count, i = 0;
	struct aim_userinfo_s *info;
	struct gaim_connection *g = find_gaim_conn_by_aim_sess(sess);

	GSList *bcs = g->buddy_chats;
	struct conversation *b = NULL;

	va_start(ap, command);
	count = va_arg(ap, int);
	info  = va_arg(ap, struct aim_userinfo_s *);
	va_end(ap);

	while(bcs) {
		b = (struct conversation *)bcs->data;
		if (!strcasecmp(b->name, (char *)command->conn->priv))
			break;	
		bcs = bcs->next;
		b = NULL;
	}
	if (!b)
		return 1;
		
	while (i < count)
		add_chat_buddy(b, info[i++].sn);

	return 1;
}

int gaim_chat_leave(struct aim_session_t *sess,
		    struct command_rx_struct *command, ...) {
	va_list ap;
	int count, i = 0;
	struct aim_userinfo_s *info;
	struct gaim_connection *g = find_gaim_conn_by_aim_sess(sess);

	GSList *bcs = g->buddy_chats;
	struct conversation *b = NULL;

	va_start(ap, command);
	count = va_arg(ap, int);
	info  = va_arg(ap, struct aim_userinfo_s *);
	va_end(ap);

	while(bcs) {
		b = (struct conversation *)bcs->data;
		if (!strcasecmp(b->name, (char *)command->conn->priv))
			break;	
		bcs = bcs->next;
		b = NULL;
	}
	if (!b)
		return 1;
		
	while (i < count)
		remove_chat_buddy(b, info[i++].sn);

	return 1;
}

int gaim_chat_info_update(struct aim_session_t *sess,
			  struct command_rx_struct *command, ...) {
	debug_print("inside chat_info_update\n");
	return 1;
}

int gaim_chat_incoming_msg(struct aim_session_t *sess,
			   struct command_rx_struct *command, ...) {
	va_list ap;
	struct aim_userinfo_s *info;
	char *msg;
	struct gaim_connection *gc = find_gaim_conn_by_aim_sess(sess);

	GSList *bcs = gc->buddy_chats;
	struct conversation *b = NULL;

	va_start(ap, command);
	info = va_arg(ap, struct aim_userinfo_s *);
	msg  = va_arg(ap, char *);

	while(bcs) {
		b = (struct conversation *)bcs->data;
		if (!strcasecmp(b->name, (char *)command->conn->priv))
			break;
		bcs = bcs->next;
		b = NULL;
	}
	if (!b)
		return 0;

	serv_got_chat_in(gc, b->id, info->sn, 0, msg);

	return 1;
}

 /*
  * Recieved in response to an IM sent with the AIM_IMFLAGS_ACK option.
  */
int gaim_parse_msgack(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	u_short type;
	char *sn = NULL;

	va_start(ap, command);
	type = (u_short)va_arg(ap, u_int);
	sn = va_arg(ap, char *);
	va_end(ap);

	sprintf(debug_buff, "Sent message to %s.\n", sn);
	debug_print(debug_buff);

	return 1;
}

int gaim_parse_ratechange(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	unsigned long newrate;

	va_start(ap, command); 
	newrate = va_arg(ap, unsigned long);
	va_end(ap);

	sprintf(debug_buff, "ratechange: %lu\n", newrate);
	debug_print(debug_buff);

	return 1;
}

int gaim_parse_evilnotify(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	char *sn;

	va_start(ap, command);
	sn = va_arg(ap, char *);
	va_end(ap);

	serv_got_eviled(sn, 0);

	return 1;
}

int gaim_rateresp(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	struct gaim_connection *gc = find_gaim_conn_by_aim_sess(sess);
	switch (command->conn->type) {
	case AIM_CONN_TYPE_BOS:
		aim_bos_ackrateresp(sess, command->conn);
		aim_bos_reqpersonalinfo(sess, command->conn);
		aim_bos_reqlocaterights(sess, command->conn);
		aim_bos_setprofile(sess, command->conn, gc->user_info, NULL, gaim_caps);
		aim_bos_reqbuddyrights(sess, command->conn);

		account_online(gc->user, gc); /* this is an awkward hack */
		serv_finish_login(gc);

		if (bud_list_cache_exists(gc))
			do_import(NULL, gc);

		debug_print("buddy list loaded\n");

		aim_addicbmparam(sess, command->conn);
		aim_bos_reqicbmparaminfo(sess, command->conn);

		aim_bos_reqrights(sess, command->conn);
		aim_bos_setgroupperm(sess, command->conn, AIM_FLAG_ALLUSERS);
		aim_bos_setprivacyflags(sess, command->conn, AIM_PRIVFLAGS_ALLOWIDLE |
							     AIM_PRIVFLAGS_ALLOWMEMBERSINCE);

		break;
	default:
		sprintf(debug_buff, "got rate response for unhandled connection type %04x\n",
				command->conn->type);
		debug_print(debug_buff);
		break;
	}

	return 1;
}

int gaim_reportinterval(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	if (command->data) {
		sprintf(debug_buff, "minimum report interval: %d (seconds?)\n", aimutil_get16(command->data+10));
		debug_print(debug_buff);
	} else
		debug_print("NULL minimum report interval!\n");
	return 1;
}

int gaim_parse_buddyrights(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	u_short maxbuddies, maxwatchers;

	va_start(ap, command);
	maxbuddies = (u_short)va_arg(ap, u_int);
	maxwatchers = (u_short)va_arg(ap, u_int);
	va_end(ap);

	sprintf(debug_buff, "buddy list rights: Max buddies = %d / Max watchers = %d\n", maxbuddies, maxwatchers);
	debug_print(debug_buff);

	return 1;
}

int gaim_bosrights(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	u_short maxpermits, maxdenies;
	va_list ap;

	va_start(ap, command);
	maxpermits = (u_short)va_arg(ap, u_int);
	maxdenies = (u_short)va_arg(ap, u_int);
	va_end(ap);

	sprintf(debug_buff, "BOS rights: Max permit = %d / Max deny = %d\n", maxpermits, maxdenies);
	debug_print(debug_buff);

	aim_bos_clientready(sess, command->conn);

	aim_bos_reqservice(sess, command->conn, AIM_CONN_TYPE_CHATNAV);

	return 1;
}

static void oscar_keepalive(struct gaim_connection *gc) {
	struct oscar_data *odata = (struct oscar_data *)gc->proto_data;
	aim_flap_nop(odata->sess, odata->conn);
}

static char *oscar_name() {
	return "Oscar";
}

static void oscar_send_im(struct gaim_connection *gc, char *name, char *message, int away) {
	struct oscar_data *odata = (struct oscar_data *)gc->proto_data;
	if (away)
		aim_send_im(odata->sess, odata->conn, name, AIM_IMFLAGS_AWAY, message);
	else
		aim_send_im(odata->sess, odata->conn, name, AIM_IMFLAGS_ACK, message);
}

static void oscar_get_info(struct gaim_connection *g, char *name) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_getinfo(odata->sess, odata->conn, name, AIM_GETINFO_GENERALINFO);
}

static void oscar_get_away_msg(struct gaim_connection *g, char *name) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_getinfo(odata->sess, odata->conn, name, AIM_GETINFO_AWAYMESSAGE);
}

static void oscar_set_dir(struct gaim_connection *g, char *first, char *middle, char *last,
			  char *maiden, char *city, char *state, char *country, int web) {
	/* FIXME : some of these things are wrong, but i'm lazy */
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_setdirectoryinfo(odata->sess, odata->conn, first, middle, last,
				maiden, NULL, NULL, city, state, NULL, 0, web);
}


static void oscar_set_idle(struct gaim_connection *g, int time) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_bos_setidle(odata->sess, odata->conn, time);
}

static void oscar_set_info(struct gaim_connection *g, char *info) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	if (awaymessage)
		aim_bos_setprofile(odata->sess, odata->conn, info,
					awaymessage->message, gaim_caps);
	else
		aim_bos_setprofile(odata->sess, odata->conn, info,
					NULL, gaim_caps);
}

static void oscar_set_away(struct gaim_connection *g, char *message) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_bos_setprofile(odata->sess, odata->conn, g->user_info, message, gaim_caps);
}

static void oscar_warn(struct gaim_connection *g, char *name, int anon) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_send_warning(odata->sess, odata->conn, name, anon);
}

static void oscar_dir_search(struct gaim_connection *g, char *first, char *middle, char *last,
			     char *maiden, char *city, char *state, char *country, char *email) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	if (strlen(email))
		aim_usersearch_address(odata->sess, odata->conn, email);
}

static void oscar_add_buddy(struct gaim_connection *g, char *name) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_add_buddy(odata->sess, odata->conn, name);
}

static void oscar_add_buddies(struct gaim_connection *g, GList *buddies) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	char buf[MSG_LEN];
	int n = 0;
	while (buddies) {
		if (n > MSG_LEN - 18) {
			aim_bos_setbuddylist(odata->sess, odata->conn, buf);
			n = 0;
		}
		n += g_snprintf(buf + n, sizeof(buf) - n, "%s&", (char *)buddies->data);
		buddies = buddies->next;
	}
	aim_bos_setbuddylist(odata->sess, odata->conn, buf);
}

static void oscar_remove_buddy(struct gaim_connection *g, char *name) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_remove_buddy(odata->sess, odata->conn, name);
}

static void oscar_join_chat(struct gaim_connection *g, int exchange, char *name) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	struct aim_conn_t *cur = NULL;
	sprintf(debug_buff, "Attempting to join chat room %s.\n", name);
	debug_print(debug_buff);
	if ((cur = aim_getconn_type(odata->sess, AIM_CONN_TYPE_CHATNAV))) {
		debug_print("chatnav exists, creating room\n");
		aim_chatnav_createroom(odata->sess, cur, name, exchange);
	} else {
		/* this gets tricky */
		debug_print("chatnav does not exist, opening chatnav\n");
		odata->create_exchange = exchange;
		odata->create_name = g_strdup(name);
		aim_bos_reqservice(odata->sess, odata->conn, AIM_CONN_TYPE_CHATNAV);
	}
}

static void oscar_chat_invite(struct gaim_connection *g, int id, char *message, char *name) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	GSList *bcs = g->buddy_chats;
	struct conversation *b = NULL;

	while (bcs) {
		b = (struct conversation *)bcs->data;
		if (id == b->id)
			break;
		bcs = bcs->next;
		b = NULL;
	}

	if (!b)
		return;

	aim_chat_invite(odata->sess, odata->conn, name,
			message ? message : "", 0x4, b->name, 0x0);
}

static void oscar_chat_leave(struct gaim_connection *g, int id) {
	struct oscar_data *odata = g ? (struct oscar_data *)g->proto_data : NULL;
	GSList *bcs = g->buddy_chats;
	struct conversation *b = NULL;
	struct chat_connection *c = NULL;
	int count = 0;

	while (bcs) {
		count++;
		b = (struct conversation *)bcs->data;
		if (id == b->id)
			break;
		bcs = bcs->next;
		b = NULL;
	}

	if (!b)
		return;

	sprintf(debug_buff, "Attempting to leave room %s (currently in %d rooms)\n",
				b->name, count);
	debug_print(debug_buff);
	
	c = find_oscar_chat(g, b->name);
	if (c != NULL) {
		if (odata)
			odata->oscar_chats = g_slist_remove(odata->oscar_chats, c);
		gdk_input_remove(c->inpa);
		if (g && odata->sess)
			aim_conn_kill(odata->sess, &c->conn);
		g_free(c->name);
		g_free(c);
	}
	/* we do this because with Oscar it doesn't tell us we left */
	serv_got_chat_left(g, b->id);
}

static void oscar_chat_whisper(struct gaim_connection *g, int id, char *who, char *message) {
	do_error_dialog("Sorry, Oscar doesn't whisper. Send an IM. (The last message was not received.)",
			"Gaim - Chat");
}

static void oscar_chat_send(struct gaim_connection *g, int id, char *message) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	struct aim_conn_t *cn; 
	GSList *bcs = g->buddy_chats;
	struct conversation *b = NULL;

	while (bcs) {
		b = (struct conversation *)bcs->data;
		if (id == b->id)
			break;
		bcs = bcs->next;
		b = NULL;
	}
	if (!b)
		return;

	cn = aim_chat_getconn(odata->sess, b->name);
	aim_chat_send_im(odata->sess, cn, message);
}

static char **oscar_list_icon(int uc) {
	if (uc & UC_UNAVAILABLE)
		return (char **)away_icon_xpm;
	if (uc & UC_AOL)
		return (char **)aol_icon_xpm;
	if (uc & UC_NORMAL)
		return (char **)free_icon_xpm;
	if (uc & UC_ADMIN)
		return (char **)admin_icon_xpm;
	if (uc & UC_UNCONFIRMED)
		return (char **)dt_icon_xpm;
	return NULL;
}

static void oscar_info(GtkObject *obj, char *who) {
	struct gaim_connection *gc = (struct gaim_connection *)gtk_object_get_user_data(obj);
	serv_get_info(gc, who);
}

static void oscar_away_msg(GtkObject *obj, char *who) {
	struct gaim_connection *gc = (struct gaim_connection *)gtk_object_get_user_data(obj);
	serv_get_away_msg(gc, who);
}

static void oscar_action_menu(GtkWidget *menu, struct gaim_connection *gc, char *who) {
	GtkWidget *button;

	button = gtk_menu_item_new_with_label(_("Get Info"));
	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(oscar_info), who);
	gtk_object_set_user_data(GTK_OBJECT(button), gc);
	gtk_menu_append(GTK_MENU(menu), button);
	gtk_widget_show(button);

	button = gtk_menu_item_new_with_label(_("Get Away Msg"));
	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(oscar_away_msg), who);
	gtk_object_set_user_data(GTK_OBJECT(button), gc);
	gtk_menu_append(GTK_MENU(menu), button);
	gtk_widget_show(button);
}

void oscar_init(struct prpl *ret) {
	ret->protocol = PROTO_OSCAR;
	ret->name = oscar_name;
	ret->list_icon = oscar_list_icon;
	ret->action_menu = oscar_action_menu;
	ret->login = oscar_login;
	ret->close = oscar_close;
	ret->send_im = oscar_send_im;
	ret->set_info = oscar_set_info;
	ret->get_info = oscar_get_info;
	ret->set_away = oscar_set_away;
	ret->get_away_msg = oscar_get_away_msg;
	ret->set_dir = oscar_set_dir;
	ret->get_dir = NULL; /* Oscar really doesn't have this */
	ret->dir_search = oscar_dir_search;
	ret->set_idle = oscar_set_idle;
	ret->change_passwd = NULL; /* Oscar doesn't have this either */
	ret->add_buddy = oscar_add_buddy;
	ret->add_buddies = oscar_add_buddies;
	ret->remove_buddy = oscar_remove_buddy;
	ret->add_permit = NULL; /* Oscar's permit/deny stuff is messed up */
	ret->add_deny = NULL; /* at least, i can't figure it out :-P */
	ret->rem_permit = NULL;
	ret->rem_deny = NULL;
	ret->set_permit_deny = NULL;
	ret->warn = oscar_warn;
	ret->accept_chat = NULL; /* oscar doesn't have accept, it just joins */
	ret->join_chat = oscar_join_chat;
	ret->chat_invite = oscar_chat_invite;
	ret->chat_leave = oscar_chat_leave;
	ret->chat_whisper = oscar_chat_whisper;
	ret->chat_send = oscar_chat_send;
	ret->keepalive = oscar_keepalive;
}

#endif
