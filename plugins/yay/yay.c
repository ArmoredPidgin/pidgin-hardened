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
#include "libyahoo.h"

struct yahoo_data {
	struct yahoo_context *ctxt;
};

static char *yahoo_name() {
	return "Yahoo";
}

char *name() {
	return "Yahoo";
}

char *description() {
	return "Allows gaim to use the Yahoo protocol";
}

static void process_packet_status(struct gaim_connection *gc, struct yahoo_packet *pkt) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	int i;

	if (pkt->service == YAHOO_SERVICE_LOGOFF && !strcasecmp(pkt->active_id, gc->username)) {
		signoff(gc);
		return;
	}

	for (i = 0; i < pkt->idstatus_count; i++) {
		struct group *g;
		struct buddy *b;
		struct yahoo_idstatus *rec = pkt->idstatus[i];

		b = find_buddy(gc, rec->id);
		if (!b) {
			struct yahoo_buddy **buddy;
			for (buddy = yd->ctxt->buddies; *buddy; buddy++) {
				struct yahoo_buddy *bud = *buddy;

				if (!strcasecmp(rec->id, bud->id))
					b = add_buddy(gc, bud->group, bud->id, bud->id);
			}
		}
		if (pkt->service == YAHOO_SERVICE_LOGOFF)
			serv_got_update(gc, b->name, 0, 0, 0, 0, 0, 0);
		else
			serv_got_update(gc, b->name, 1, 0, 0, 0, rec->status, 0);
	}
}

static void process_packet_message(struct gaim_connection *gc, struct yahoo_packet *pkt) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;

	if (pkt->msg) {
		if (pkt->msgtype == YAHOO_MSGTYPE_BOUNCE)
			do_error_dialog("Your message did not get received.", "Error");
		else
			serv_got_im(gc, pkt->msg_id, pkt->msg, pkt->msg_timestamp ? 1 : 0);
	}
}

static void process_packet_conf_invite(struct gaim_connection *gc, struct yahoo_packet *pkt) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
}

static void process_packet_conf_add_invite(struct gaim_connection *gc, struct yahoo_packet *pkt) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
}

static void process_packet_conf_msg(struct gaim_connection *gc, struct yahoo_packet *pkt) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
}

static void process_packet_conf_logon(struct gaim_connection *gc, struct yahoo_packet *pkt) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
}

static void process_packet_conf_logoff(struct gaim_connection *gc, struct yahoo_packet *pkt) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
}

static void process_packet_newmail(struct gaim_connection *gc, struct yahoo_packet *pkt) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	char buf[2048];

	if (pkt->mail_status) {
		if (pkt->service == YAHOO_SERVICE_NEWMAIL)
			g_snprintf(buf, sizeof buf, "%s has %d new message%s on Yahoo Mail.",
					gc->username, pkt->mail_status,
					pkt->mail_status == 1 ? "" : "s");
		else
			g_snprintf(buf, sizeof buf, "%s has %d new personal message%s on Yahoo Mail.",
					gc->username, pkt->mail_status,
					pkt->mail_status == 1 ? "" : "s");
		do_error_dialog(buf, "New Mail!");
	}
}

static void yahoo_callback(gpointer data, gint source, GdkInputCondition condition) {
	struct gaim_connection *gc = (struct gaim_connection *)data;
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;

	struct yahoo_rawpacket *rawpkt;
	struct yahoo_packet *pkt;

	if (!yahoo_getdata(yd->ctxt)) {
		signoff(gc);
	}

	while ((rawpkt = yahoo_getpacket(yd->ctxt)) != NULL) {
		pkt = yahoo_parsepacket(yd->ctxt, rawpkt);

		switch (pkt->service) {
			case YAHOO_SERVICE_USERSTAT:
			case YAHOO_SERVICE_CHATLOGON:
			case YAHOO_SERVICE_CHATLOGOFF:
			case YAHOO_SERVICE_LOGON:
			case YAHOO_SERVICE_LOGOFF:
			case YAHOO_SERVICE_ISAWAY:
			case YAHOO_SERVICE_ISBACK:
				process_packet_status(gc, pkt);
				break;
			case YAHOO_SERVICE_MESSAGE:
			case YAHOO_SERVICE_CHATMSG:
			case YAHOO_SERVICE_SYSMESSAGE:
				process_packet_message(gc, pkt);
				break;
			case YAHOO_SERVICE_CONFINVITE:
				process_packet_conf_invite(gc, pkt);
				break;
			case YAHOO_SERVICE_CONFADDINVITE:
				process_packet_conf_add_invite(gc, pkt);
				break;
			case YAHOO_SERVICE_CONFMSG:
				process_packet_conf_msg(gc, pkt);
				break;
			case YAHOO_SERVICE_CONFLOGON:
				process_packet_conf_logon(gc, pkt);
				break;
			case YAHOO_SERVICE_CONFLOGOFF:
				process_packet_conf_logoff(gc, pkt);
				break;
			case YAHOO_SERVICE_NEWMAIL:
			case YAHOO_SERVICE_NEWPERSONALMAIL:
				process_packet_newmail(gc, pkt);
				break;
			default:
				debug_printf("Unhandled packet type %s\n",
						yahoo_get_service_string(pkt->service));
				break;
		}
		yahoo_free_packet(pkt);
		yahoo_free_rawpacket(rawpkt);
	}
}

static void yahoo_login(struct aim_user *user) {
	struct gaim_connection *gc = new_gaim_conn(user);
	struct yahoo_data *yd = gc->proto_data = g_new0(struct yahoo_data, 1);
	int i;

	struct yahoo_options opt;
	struct yahoo_context *ctxt;
	opt.connect_mode = YAHOO_CONNECT_NORMAL;
	opt.proxy_host = NULL;
	ctxt = yahoo_init(user->username, user->password, &opt);
	yd->ctxt = ctxt;

	set_login_progress(gc, 1, "Connecting");
	while (gtk_events_pending())
		gtk_main_iteration();

	if (!ctxt || !yahoo_connect(ctxt)) {
		debug_printf("Yahoo: Unable to connect\n");
		hide_login_progress(gc, "Unable to connect");
		destroy_gaim_conn(gc);
		return;
	}

	debug_printf("Yahoo: connected\n");

	set_login_progress(gc, 3, "Getting Config");
	while (gtk_events_pending())
		gtk_main_iteration();

	yahoo_get_config(ctxt);

	if (yahoo_cmd_logon(ctxt, YAHOO_STATUS_AVAILABLE)) {
		debug_printf("Yahoo: Unable to login\n");
		hide_login_progress(gc, "Unable to login");
		destroy_gaim_conn(gc);
		return;
	}

	if (ctxt->buddies) {
		struct yahoo_buddy **buddies;
		
		for (buddies = ctxt->buddies; *buddies; buddies++) {
			struct yahoo_buddy *bud = *buddies;
			struct buddy *b;
			struct group *g;

			b = find_buddy(gc, bud->id);
			if (!b) add_buddy(gc, bud->group, bud->id, bud->id);
		}
	}

	debug_printf("Yahoo: logged in %s\n", gc->username);
	account_online(gc);
	serv_finish_login(gc);

	gc->inpa = gdk_input_add(ctxt->sockfd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
				yahoo_callback, gc);
}

static void yahoo_close(struct gaim_connection *gc) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	if (gc->inpa)
		gdk_input_remove(gc->inpa);
	gc->inpa = -1;
	yahoo_cmd_logoff(yd->ctxt);
	g_free(yd);
}

static void yahoo_send_im(struct gaim_connection *gc, char *who, char *message, int away) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;

	yahoo_cmd_msg(yd->ctxt, gc->username, who, message);
}

static void yahoo_set_idle(struct gaim_connection *gc, int idle) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;

	if (idle)
		yahoo_cmd_set_away_mode(yd->ctxt, YAHOO_STATUS_IDLE, NULL);
	else
		yahoo_cmd_set_back_mode(yd->ctxt, YAHOO_STATUS_AVAILABLE, NULL);
}

static void yahoo_keepalive(struct gaim_connection *gc) {
	yahoo_cmd_ping(((struct yahoo_data *)gc->proto_data)->ctxt);
}

static void gyahoo_add_buddy(struct gaim_connection *gc, char *name) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct yahoo_buddy *tmpbuddy;
	struct group *g = find_group_by_buddy(gc, name);
	char *group = NULL;

	if (g) {
		group = g->name;
	} else if (yd->ctxt && yd->ctxt->buddies[0]) {
		tmpbuddy = yd->ctxt->buddies[0];
		group = tmpbuddy->group;
	}

	if (group)
		yahoo_add_buddy(yd->ctxt, name, gc->username, group, "");
}

static struct prpl *my_protocol = NULL;

void Yahoo_init(struct prpl *ret) {
	/* the NULL's aren't required but they're nice to have */
	ret->protocol = PROTO_YAHOO;
	ret->name = yahoo_name;
	ret->list_icon = NULL;
	ret->action_menu = NULL;
	ret->login = yahoo_login;
	ret->close = yahoo_close;
	ret->send_im = yahoo_send_im;
	ret->set_info = NULL;
	ret->get_info = NULL;
	ret->set_away = NULL;
	ret->get_away_msg = NULL;
	ret->set_dir = NULL;
	ret->get_dir = NULL;
	ret->dir_search = NULL;
	ret->set_idle = yahoo_set_idle;
	ret->change_passwd = NULL;
	ret->add_buddy = gyahoo_add_buddy;
	ret->add_buddies = NULL;
	ret->remove_buddy = NULL;
	ret->add_permit = NULL;
	ret->add_deny = NULL;
	ret->rem_permit = NULL;
	ret->rem_deny = NULL;
	ret->set_permit_deny = NULL;
	ret->warn = NULL;
	ret->accept_chat = NULL;
	ret->join_chat = NULL;
	ret->chat_invite = NULL;
	ret->chat_leave = NULL;
	ret->chat_whisper = NULL;
	ret->chat_send = NULL;
	ret->keepalive = yahoo_keepalive;

	my_protocol = ret;
}

char *gaim_plugin_init(GModule *handle) {
	load_protocol(Yahoo_init);
	return NULL;
}

void gaim_plugin_remove() {
	struct prpl *p = find_prpl(PROTO_YAHOO);
	if (p == my_protocol)
		unload_protocol(p);
}
