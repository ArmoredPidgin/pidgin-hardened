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
#include <config.h>
#endif
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include "prpl.h"
#include "multi.h"
#include "gaim.h"
#ifdef USE_APPLET
#include "applet.h"
#endif

#include "pixmaps/ok.xpm"
#include "pixmaps/cancel.xpm"

void serv_login(struct aim_user *user)
{
	struct prpl *p = find_prpl(user->protocol);
	
	if (user->gc != NULL)
		return;

	if (p && p->login) {
		if (!strlen(user->password) && !(p->options & OPT_PROTO_NO_PASSWORD)) {
			do_error_dialog(_("Please enter your password"), _("Signon Error"));
			return;
		}

		debug_printf("Logging in using %s\n", p->name());
		p->login(user);
	} else {
		do_error_dialog(_("You cannot log this account in; you do not have "
				  "the protocol it uses loaded, or the protocol does "
				  "not have a login function."), _("Login Error"));
	}
}

static gboolean send_keepalive(gpointer d)
{
	struct gaim_connection *gc = d;
	if (gc->prpl && gc->prpl->keepalive)
		gc->prpl->keepalive(gc);
	return TRUE;
}

static void update_keepalive(struct gaim_connection *gc, gboolean on)
{
	if (on && !gc->keepalive) {
		debug_printf("allowing NOP\n");
		gc->keepalive = g_timeout_add(60000, send_keepalive, gc);
	} else if (!on && gc->keepalive > 0) {
		debug_printf("removing NOP\n");
		g_source_remove(gc->keepalive);
		gc->keepalive = 0;
	}
}

void serv_close(struct gaim_connection *gc)
{
	while (gc->buddy_chats) {
		struct conversation *b = gc->buddy_chats->data;
		gc->buddy_chats = g_slist_remove(gc->buddy_chats, b);
		b->gc = NULL;
		update_buttons_by_protocol(b);
	}

	if (gc->idle_timer > 0)
		g_source_remove(gc->idle_timer);
	gc->idle_timer = 0;

	update_keepalive(gc, FALSE);

	if (gc->prpl && gc->prpl->close)
		gc->prpl->close(gc);

	account_offline(gc);
	destroy_gaim_conn(gc);
}

void serv_touch_idle(struct gaim_connection *gc)
{
	/* Are we idle?  If so, not anymore */
	if (gc->is_idle > 0) {
		gc->is_idle = 0;
		serv_set_idle(gc, 0);
	}
	time(&gc->lastsent);
	if (gc->is_auto_away)
		check_idle(gc);
}

void serv_finish_login(struct gaim_connection *gc)
{
	if (strlen(gc->user->user_info)) {
		/* g_malloc(strlen(gc->user->user_info) * 4);
		   strncpy_withhtml(buf, gc->user->user_info, strlen(gc->user->user_info) * 4); */
		serv_set_info(gc, gc->user->user_info);
		/* g_free(buf); */
	}

	if (gc->idle_timer > 0)
		g_source_remove(gc->idle_timer);

	gc->idle_timer = g_timeout_add(20000, check_idle, gc);
	serv_touch_idle(gc);

	time(&gc->login_time);

	if (gc->prpl->options & OPT_PROTO_CORRECT_TIME)
		serv_add_buddy(gc, gc->username);

	update_keepalive(gc, TRUE);
}



int serv_send_im(struct gaim_connection *gc, char *name, char *message, int flags)
{
	int val = -EINVAL;
	if (gc->prpl && gc->prpl->send_im)
		val = gc->prpl->send_im(gc, name, message, flags);

	if (!(flags & IM_FLAG_AWAY))
		serv_touch_idle(gc);

	return val;
}

void serv_get_info(struct gaim_connection *g, char *name)
{
	if (g && g->prpl && g->prpl->get_info)
		g->prpl->get_info(g, name);
}

void serv_get_dir(struct gaim_connection *g, char *name)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->get_dir)
		g->prpl->get_dir(g, name);
}

void serv_set_dir(struct gaim_connection *g, char *first, char *middle, char *last, char *maiden,
		  char *city, char *state, char *country, int web)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->set_dir)
		g->prpl->set_dir(g, first, middle, last, maiden, city, state, country, web);
}

void serv_dir_search(struct gaim_connection *g, char *first, char *middle, char *last, char *maiden,
		     char *city, char *state, char *country, char *email)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->dir_search)
		g->prpl->dir_search(g, first, middle, last, maiden, city, state, country, email);
}


void serv_set_away(struct gaim_connection *gc, char *state, char *message)
{
	if (gc && gc->prpl && gc->prpl->set_away) {
		char *buf = NULL;
		if (message) {
			buf = g_malloc(strlen(message) + 1);
			if (gc->prpl->options & OPT_PROTO_HTML)
				strncpy(buf, message, strlen(message) + 1);
			else
				strncpy_nohtml(buf, message, strlen(message) + 1);
		}

		gc->prpl->set_away(gc, state, buf);

		plugin_event(event_away, gc, state, buf, 0);

		if (buf)
			g_free(buf);
	}

	system_log(log_away, gc, NULL, OPT_LOG_BUDDY_AWAY | OPT_LOG_MY_SIGNON);
}

void serv_set_away_all(char *message)
{
	GSList *c = connections;
	struct gaim_connection *g;

	while (c) {
		g = (struct gaim_connection *)c->data;
		serv_set_away(g, GAIM_AWAY_CUSTOM, message);
		c = c->next;
	}
}

void serv_set_info(struct gaim_connection *g, char *info)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->set_info) {
		plugin_event(event_set_info, g, info, 0, 0);
		g->prpl->set_info(g, info);
	}
}

void serv_change_passwd(struct gaim_connection *g, char *orig, char *new)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->change_passwd)
		g->prpl->change_passwd(g, orig, new);
}

void serv_add_buddy(struct gaim_connection *g, char *name)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->add_buddy)
		g->prpl->add_buddy(g, name);
}

void serv_add_buddies(struct gaim_connection *g, GList *buddies)
{
	if (g && g_slist_find(connections, g) && g->prpl) {
		if (g->prpl->add_buddies)
			g->prpl->add_buddies(g, buddies);
		else if (g->prpl->add_buddy)
			while (buddies) {
				g->prpl->add_buddy(g, buddies->data);
				buddies = buddies->next;
			}
	}
}


void serv_remove_buddy(struct gaim_connection *g, char *name)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->remove_buddy)
		g->prpl->remove_buddy(g, name);
}

void serv_remove_buddies(struct gaim_connection *gc, GList *g)
{
	if (!g_slist_find(connections, gc))
		return;
	if (!gc->prpl)
		return;		/* how the hell did that happen? */
	if (gc->prpl->remove_buddies)
		gc->prpl->remove_buddies(gc, g);
	else {
		while (g) {
			serv_remove_buddy(gc, g->data);
			g = g->next;
		}
	}
}

void serv_add_permit(struct gaim_connection *g, char *name)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->add_permit)
		g->prpl->add_permit(g, name);
}

void serv_add_deny(struct gaim_connection *g, char *name)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->add_deny)
		g->prpl->add_deny(g, name);
}

void serv_rem_permit(struct gaim_connection *g, char *name)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->rem_permit)
		g->prpl->rem_permit(g, name);
}

void serv_rem_deny(struct gaim_connection *g, char *name)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->rem_deny)
		g->prpl->rem_deny(g, name);
}

void serv_set_permit_deny(struct gaim_connection *g)
{
	/* this is called when either you import a buddy list, and make lots of changes that way,
	 * or when the user toggles the permit/deny mode in the prefs. In either case you should
	 * probably be resetting and resending the permit/deny info when you get this. */
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->set_permit_deny)
		g->prpl->set_permit_deny(g);
}


void serv_set_idle(struct gaim_connection *g, int time)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->set_idle)
		g->prpl->set_idle(g, time);
}

void serv_warn(struct gaim_connection *g, char *name, int anon)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->warn)
		g->prpl->warn(g, name, anon);
}

void serv_join_chat(struct gaim_connection *g, GList *data)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->join_chat)
		g->prpl->join_chat(g, data);
}

void serv_chat_invite(struct gaim_connection *g, int id, char *message, char *name)
{
	char *buffy = message && *message ? g_strdup(message) : NULL;
	plugin_event(event_chat_send_invite, g, (void *)id, name, &buffy);
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->chat_invite)
		g->prpl->chat_invite(g, id, buffy, name);
	if (buffy)
		g_free(buffy);
}

void serv_chat_leave(struct gaim_connection *g, int id)
{
	if (!g_slist_find(connections, g))
		return;

	if (g->prpl && g->prpl->chat_leave)
		g->prpl->chat_leave(g, id);
}

void serv_chat_whisper(struct gaim_connection *g, int id, char *who, char *message)
{
	if (g->prpl && g->prpl->chat_whisper)
		g->prpl->chat_whisper(g, id, who, message);
}

int serv_chat_send(struct gaim_connection *g, int id, char *message)
{
	int val = -EINVAL;
	if (g->prpl && g->prpl->chat_send)
		val = g->prpl->chat_send(g, id, message);
	serv_touch_idle(g);
	return val;
}

int find_queue_row_by_name(char *name)
{
	GSList *templist;
	char *temp;
	int i;

	templist = message_queue;

	for (i = 0; i < GTK_CLIST(clistqueue)->rows; i++) {
		gtk_clist_get_text(GTK_CLIST(clistqueue), i, 0, &temp);

		if (!strcmp(name, temp))
			return i;
	}

	return -1;
}

int find_queue_total_by_name(char *name)
{
	GSList *templist;
	int i = 0;

	templist = message_queue;

	while (templist) {
		struct queued_message *qm = (struct queued_message *)templist->data;
		if ((qm->flags & WFLAG_RECV) && !strcmp(name, qm->name))
			i++;

		templist = templist->next;
	}

	return i;
}

struct queued_away_response {
	char name[80];
	time_t sent_away;
};

struct queued_away_response *find_queued_away_response_by_name(char *name)
{
	GSList *templist;
	struct queued_away_response *qar;

	templist = away_time_queue;

	while (templist) {
		qar = (struct queued_away_response *)templist->data;

		if (!strcmp(name, qar->name))
			return qar;

		templist = templist->next;
	}

	return NULL;
}

/* woo. i'm actually going to comment this function. isn't that fun. make sure to follow along, kids */
void serv_got_im(struct gaim_connection *gc, char *name, char *message, guint32 flags, time_t mtime)
{
	char *buffy;
	char *angel;
	int plugin_return;
	int away = 0;

	struct conversation *cnv;
	int new_conv = 0;

	/* pay no attention to the man behind the curtain.
	 *
	 * the reason i feel okay with this is because it's useful to some plugins.
	 * Gaim doesn't ever use it itself. Besides, it's not entirely accurate; it's
	 * possible to have false negatives with most protocols. Also with some it's
	 * easy to have false positives as well. So if you're a plugin author, don't
	 * rely on this, still do your own checks. but uh. it's a start. */
	if (flags & IM_FLAG_GAIMUSER)
		debug_printf("%s is a gaim user.\n", name);

	/* we should update the conversation window buttons and menu, if it exists. */
	cnv = find_conversation(name);
	if (cnv)
		set_convo_gc(cnv, gc);
	/* we do the new_conv check here in case any plugins decide to create it */
	else
		new_conv = 1;

	/* plugin stuff. we pass a char ** but we don't want to pass what's been given us
	 * by the prpls. so we create temp holders and pass those instead. it's basically
	 * just to avoid segfaults. */
	buffy = g_malloc(MAX(strlen(message) + 1, BUF_LONG));
	strcpy(buffy, message);
	angel = g_strdup(name);
	plugin_return = plugin_event(event_im_recv, gc, &angel, &buffy, (void *)flags);

	if (!buffy || !angel || plugin_return) {
		if (buffy)
			g_free(buffy);
		if (angel)
			g_free(angel);
		return;
	}
	name = angel;
	message = buffy;

	/* TiK, using TOC, sends an automated message in order to get your away message. Now,
	 * this is one of the biggest hacks I think I've seen. But, in order to be nice to
	 * TiK, we're going to give users the option to ignore it. */
	if ((away_options & OPT_AWAY_TIK_HACK) && gc->away && strlen(gc->away) &&
	    !strcmp(message, ">>>Automated Message: Getting Away Message<<<")) {
		char *tmpmsg = stylize(awaymessage->message, MSG_LEN);
		serv_send_im(gc, name, tmpmsg, IM_FLAG_AWAY);
		g_free(tmpmsg);
		g_free(name);
		g_free(message);
		return;
	}

	/* if you can't figure this out, stop reading right now.
	 * "we're not worthy! we're not worthy!" */
	if (convo_options & OPT_CONVO_SEND_LINKS)
		linkify_text(message);

	/* um. when we call write_to_conv with the message we received, it's nice to pass whether
	 * or not it was an auto-response. so if it was an auto-response, we set the appropriate
	 * flag. this is just so prpls don't have to know about WFLAG_* (though some do anyway) */
	if (flags & IM_FLAG_AWAY)
		away = WFLAG_AUTO;

	/* alright. two cases for how to handle this. either we're away or we're not. if we're not,
	 * then it's easy. if we are, then there are three or four different ways of handling it
	 * and different things we have to do for each. */
	if (gc->away) {
		time_t t;
		char *tmpmsg;
		struct buddy *b = find_buddy(gc, name);
		char *alias = b ? b->show : name;
		int row;
		struct queued_away_response *qar;

		time(&t);

		/* either we're going to queue it or not. Because of the way awayness currently
		 * works, this is fucked up. it's possible for an account to be away without the
		 * imaway dialog being shown. in fact, it's possible for *all* the accounts to be
		 * away without the imaway dialog being shown. so in order for this to be queued
		 * properly, we have to make sure that the imaway dialog actually exists, first. */
		if (!cnv && clistqueue && (away_options & OPT_AWAY_QUEUE)) {
			/* alright, so we're going to queue it. neat, eh? :) so first we create
			 * something to store the message, and add it to our queue. Then we update
			 * the away dialog to indicate that we've queued something. */
			struct queued_message *qm;

			qm = g_new0(struct queued_message, 1);
			g_snprintf(qm->name, sizeof(qm->name), "%s", name);
			qm->message = g_strdup(message);
			qm->gc = gc;
			qm->tm = mtime;
			qm->flags = WFLAG_RECV | away;
			message_queue = g_slist_append(message_queue, qm);

#ifdef USE_APPLET
			set_user_state(away);
#endif

			row = find_queue_row_by_name(qm->name);

			if (row >= 0) {
				char number[32];
				int qtotal;

				qtotal = find_queue_total_by_name(qm->name);
				g_snprintf(number, 32, _("(%d messages)"), qtotal);
				gtk_clist_set_text(GTK_CLIST(clistqueue), row, 1, number);
			} else {
				gchar *heh[2];

				heh[0] = qm->name;
				heh[1] = _("(1 message)");
				gtk_clist_append(GTK_CLIST(clistqueue), heh);
			}
		} else {
			/* ok, so we're not queuing it. well then, we'll try to handle it normally.
			 * Some people think that ignoring it is a perfectly acceptible way to handle
			 * it. i think they're on crack, but hey, that's why it's optional. */
			if (away_options & OPT_AWAY_DISCARD) {
				g_free(name);
				g_free(message);
				return;
			}

			/* ok, so we're not ignoring it. make sure the conversation exists and is
			 * updated (partly handled above already), play the receive sound (sound.c
			 * will take care of not playing while away), and then write it to the
			 * convo window. */
			if (cnv == NULL) {
				cnv = new_conversation(name);
				set_convo_gc(cnv, gc);
			}
			if (new_conv && (sound_options & OPT_SOUND_FIRST_RCV))
				play_sound(FIRST_RECEIVE);
			else if (cnv->makesound && (sound_options & OPT_SOUND_RECV))
				play_sound(RECEIVE);

			write_to_conv(cnv, message, away | WFLAG_RECV, NULL, mtime);
		}

		/* regardless of whether we queue it or not, we should send an auto-response. That is,
		 * of course, unless the horse.... no wait. */
		if ((away_options & OPT_AWAY_NO_AUTO_RESP) || !strlen(gc->away)) {
			g_free(name);
			g_free(message);
			return;
		}

		/* this used to be based on the conversation window. but um, if you went away, and
		 * someone sent you a message and got your auto-response, and then you closed the
		 * window, and then the sent you another one, they'd get the auto-response back
		 * too soon. besides that, we need to keep track of this even if we've got a queue.
		 * so the rest of this block is just the auto-response, if necessary */
		qar = find_queued_away_response_by_name(name);
		if (!qar) {
			qar = (struct queued_away_response *)g_new0(struct queued_away_response, 1);
			g_snprintf(qar->name, sizeof(qar->name), "%s", name);
			qar->sent_away = 0;
			away_time_queue = g_slist_append(away_time_queue, qar);
		}
		if ((t - qar->sent_away) < 120) {
			g_free(name);
			g_free(message);
			return;
		}
		qar->sent_away = t;

		/* apply default fonts and colors */
		tmpmsg = stylize(gc->away, MSG_LEN);
		serv_send_im(gc, name, away_subs(tmpmsg, alias), IM_FLAG_AWAY);
		if (!cnv && clistqueue && (away_options & OPT_AWAY_QUEUE)) {
			struct queued_message *qm;
			qm = g_new0(struct queued_message, 1);
			g_snprintf(qm->name, sizeof(qm->name), "%s", name);
			qm->message = g_strdup(away_subs(tmpmsg, alias));
			qm->gc = gc;
			qm->tm = mtime;
			qm->flags = WFLAG_SEND | WFLAG_AUTO;
			message_queue = g_slist_append(message_queue, qm);
		} else if (cnv != NULL)
			write_to_conv(cnv, away_subs(tmpmsg, alias), WFLAG_SEND | WFLAG_AUTO, NULL,
				      mtime);
		g_free(tmpmsg);
	} else {
		/* we're not away. this is easy. if the convo window doesn't exist, create and update
		 * it (if it does exist it was updated earlier), then play a sound indicating we've
		 * received it and then display it. easy. */
		if (cnv == NULL) {
			cnv = new_conversation(name);
			set_convo_gc(cnv, gc);
		}
		if (new_conv && (sound_options & OPT_SOUND_FIRST_RCV))
			play_sound(FIRST_RECEIVE);
		else if (cnv->makesound && (sound_options & OPT_SOUND_RECV))
			play_sound(RECEIVE);

		write_to_conv(cnv, message, away | WFLAG_RECV, NULL, mtime);
	}

	plugin_event(event_im_displayed_rcvd, gc, name, message, (void *)flags);
	g_free(name);
	g_free(message);
}



void serv_got_update(struct gaim_connection *gc, char *name, int loggedin, int evil, time_t signon,
		     time_t idle, int type, gushort caps)
{
	struct buddy *b = find_buddy(gc, name);

	if (gc->prpl->options & OPT_PROTO_CORRECT_TIME) {
		char *tmp = g_strdup(normalize(name));
		if (!g_strcasecmp(tmp, normalize(gc->username))) {
			gc->evil = evil;
			gc->correction_time = (signon - gc->login_time);
			update_idle_times();
		}
		g_free(tmp);
	}

	if (!b) {
		debug_printf("Error, no such buddy %s\n", name);
		return;
	}

	/* This code will 'align' the name from the TOC */
	/* server with what's in our record.  We want to */
	/* store things how THEY want it... */
	if (strcmp(name, b->name)) {
		GList *cnv = conversations;
		struct conversation *cv;

		char *who = g_malloc(80);

		strcpy(who, normalize(name));

		while (cnv) {
			cv = (struct conversation *)cnv->data;
			if (!g_strcasecmp(who, normalize(cv->name))) {
				if (im_options & OPT_IM_ONE_WINDOW) {
					set_convo_tab_label(cv, b->name);
				} else {
					g_snprintf(cv->name, sizeof(cv->name), "%s", name);
					if (find_log_info(name) || (logging_options & OPT_LOG_ALL))
						 g_snprintf(who, 63, LOG_CONVERSATION_TITLE, name);
					else
						g_snprintf(who, 63, CONVERSATION_TITLE, name);
					gtk_window_set_title(GTK_WINDOW(cv->window), who);
					/* was g_free(buf), but break gives us that
					 * and freeing twice is not good --Sumner */
					break;
				}
			}
			cnv = cnv->next;
		}
		g_free(who);
		who = g_strdup(b->name);
		g_snprintf(b->name, sizeof(b->name), "%s", name);
		handle_buddy_rename(b, who);
		g_free(who);
	}

	if (!b->idle && idle) {
		plugin_event(event_buddy_idle, gc, b->name, 0, 0);
		system_log(log_idle, gc, b, OPT_LOG_BUDDY_IDLE);
	}
	if (b->idle && !idle) {
		do_pounce(gc, b->name, OPT_POUNCE_UNIDLE);
		plugin_event(event_buddy_unidle, gc, b->name, 0, 0);
		system_log(log_unidle, gc, b, OPT_LOG_BUDDY_IDLE);
	}

	b->idle = idle;
	b->evil = evil;

	if ((b->uc & UC_UNAVAILABLE) && !(type & UC_UNAVAILABLE)) {
		do_pounce(gc, b->name, OPT_POUNCE_UNAWAY);
		plugin_event(event_buddy_back, gc, b->name, 0, 0);
		system_log(log_back, gc, b, OPT_LOG_BUDDY_AWAY);
	} else if (!(b->uc & UC_UNAVAILABLE) && (type & UC_UNAVAILABLE)) {
		plugin_event(event_buddy_away, gc, b->name, 0, 0);
		system_log(log_away, gc, b, OPT_LOG_BUDDY_AWAY);
	}

	b->uc = type;
	if (caps)
		b->caps = caps;

	b->signon = signon;

	if (loggedin) {
		if (!b->present) {
			b->present = 1;
			do_pounce(gc, b->name, OPT_POUNCE_SIGNON);
			plugin_event(event_buddy_signon, gc, b->name, 0, 0);
			system_log(log_signon, gc, b, OPT_LOG_BUDDY_SIGNON);
		}
	} else {
		if (b->present) {
			plugin_event(event_buddy_signoff, gc, b->name, 0, 0);
			system_log(log_signoff, gc, b, OPT_LOG_BUDDY_SIGNON);
		}
		b->present = 0;
	}

	set_buddy(gc, b);
}


void serv_got_eviled(struct gaim_connection *gc, char *name, int lev)
{
	char buf2[1024];

	plugin_event(event_warned, gc, name, (void *)lev, 0);

	if (gc->evil >= lev) {
		gc->evil = lev;
		return;
	}

	gc->evil = lev;

	g_snprintf(buf2, sizeof(buf2), "%s has just been warned by %s.\nYour new warning level is %d%%",
		   gc->username, ((name == NULL)? "an anonymous person" : name), lev);

	do_error_dialog(buf2, _("Warned"));
}



static void close_invite(GtkWidget *w, GtkWidget *w2)
{
	GList *str = gtk_object_get_user_data(GTK_OBJECT(w2));
	GList *tmp = str;

	while (tmp) {
		g_free(tmp->data);
		tmp = tmp->next;
	}
	if (str)
		g_list_free(str);

	gtk_widget_destroy(w2);
}

static void chat_invite_callback(GtkWidget *w, GtkWidget *w2)
{
	struct gaim_connection *g = (struct gaim_connection *)
	    gtk_object_get_user_data(GTK_OBJECT(GTK_DIALOG(w2)->vbox));
	GList *str, *tmp;

	str = gtk_object_get_user_data(GTK_OBJECT(w2));

	serv_join_chat(g, str);

	tmp = str;

	while (tmp) {
		/* this is either a g_malloc'd char* or g_malloc'd int* */
		g_free(tmp->data);
		tmp = tmp->next;
	}
	if (str)
		g_list_free(str);

	gtk_widget_destroy(w2);
}



void serv_got_chat_invite(struct gaim_connection *g, char *name, char *who, char *message, GList *data)
{
	GtkWidget *d;
	GtkWidget *label;
	GtkWidget *yesbtn;
	GtkWidget *nobtn;

	char buf2[BUF_LONG];


	plugin_event(event_chat_invited, g, who, name, message);

	if (message)
		g_snprintf(buf2, sizeof(buf2), "User '%s' invites %s to buddy chat room: '%s'\n%s", who,
			   g->username, name, message);
	else
		g_snprintf(buf2, sizeof(buf2), "User '%s' invites %s to buddy chat room: '%s'\n", who,
			   g->username, name);

	d = gtk_dialog_new();
	gtk_widget_realize(d);
	aol_icon(d->window);


	label = gtk_label_new(buf2);
	gtk_widget_show(label);
	yesbtn = picture_button(d, _("Yes"), ok_xpm);
	nobtn = picture_button(d, _("No"), cancel_xpm);
	gtk_widget_show(nobtn);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->vbox), label, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->action_area), yesbtn, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->action_area), nobtn, FALSE, FALSE, 5);

	gtk_object_set_user_data(GTK_OBJECT(GTK_DIALOG(d)->vbox), g);
	gtk_object_set_user_data(GTK_OBJECT(d), data);


	gtk_window_set_title(GTK_WINDOW(d), "Buddy chat invite");
	gtk_signal_connect(GTK_OBJECT(nobtn), "clicked", GTK_SIGNAL_FUNC(close_invite), d);
	gtk_signal_connect(GTK_OBJECT(yesbtn), "clicked", GTK_SIGNAL_FUNC(chat_invite_callback), d);


	gtk_widget_show(d);
}

struct conversation *serv_got_joined_chat(struct gaim_connection *gc, int id, char *name)
{
	struct conversation *b;

	plugin_event(event_chat_join, gc, (void *)id, name, 0);

	b = (struct conversation *)g_new0(struct conversation, 1);
	gc->buddy_chats = g_slist_append(gc->buddy_chats, b);
	chats = g_list_append(chats, b);

	b->is_chat = TRUE;
	b->ignored = NULL;
	b->in_room = NULL;
	b->id = id;
	b->gc = gc;
	b->history = g_string_new("");
	g_snprintf(b->name, 80, "%s", name);

	if ((logging_options & OPT_LOG_ALL) || find_log_info(b->name)) {
		FILE *fd;
		char *filename;

		filename = (char *)malloc(100);
		g_snprintf(filename, 100, "%s.chat", b->name);

		fd = open_log_file(filename);
		if (fd) {
			if (!(logging_options & OPT_LOG_STRIP_HTML))
				fprintf(fd,
					"<HR><BR><H3 Align=Center> ---- New Conversation @ %s ----</H3><BR>\n",
					full_date());
			else
				fprintf(fd, "---- New Conversation @ %s ----\n", full_date());

			fclose(fd);
		}
		free(filename);
	}

	show_new_buddy_chat(b);

	return b;
}

void serv_got_chat_left(struct gaim_connection *g, int id)
{
	GSList *bcs = g->buddy_chats;
	struct conversation *b = NULL;


	while (bcs) {
		b = (struct conversation *)bcs->data;
		if (id == b->id) {
			break;
		}
		b = NULL;
		bcs = bcs->next;
	}

	if (!b)
		return;

	plugin_event(event_chat_leave, g, (void *)b->id, 0, 0);

	debug_printf("Leaving room %s.\n", b->name);

	g->buddy_chats = g_slist_remove(g->buddy_chats, b);

	delete_chat(b);
}

void serv_got_chat_in(struct gaim_connection *g, int id, char *who, int whisper, char *message,
		      time_t mtime)
{
	int w;
	GSList *bcs = g->buddy_chats;
	struct conversation *b = NULL;
	char *buf;

	while (bcs) {
		b = (struct conversation *)bcs->data;
		if (id == b->id)
			break;
		bcs = bcs->next;
		b = NULL;

	}
	if (!b)
		return;

	if (plugin_event(event_chat_recv, g, (void *)b->id, who, message))
		return;

	buf = g_malloc(MAX(strlen(message) * 2, 8192));
	strcpy(buf, message);

	if (convo_options & OPT_CONVO_SEND_LINKS)
		linkify_text(buf);

	if (whisper)
		w = WFLAG_WHISPER;
	else
		w = 0;

	chat_write(b, who, w, buf, mtime);
	g_free(buf);
}
