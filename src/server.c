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
#include "gtkimhtml.h"
#include "prpl.h"
#include "multi.h"
#include "gaim.h"

#include "pixmaps/ok.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/tb_search.xpm"

void serv_login(struct aim_user *user)
{
	struct prpl *p = find_prpl(user->protocol);

	if (user->gc != NULL || p == NULL)
		return;

	if(!ref_protocol(p))
		return;

	if (p->login) {
		if (!strlen(user->password) && !(p->options & OPT_PROTO_NO_PASSWORD) &&
			!(p->options & OPT_PROTO_PASSWORD_OPTIONAL)) {
			do_error_dialog(_("Please enter your password"), NULL, GAIM_ERROR);
			return;
		}

		debug_printf(PACKAGE " " VERSION " logging in %s using %s\n", user->username, p->name);
		user->connecting = TRUE;
		connecting_count++;
		debug_printf("connecting_count: %d\n", connecting_count);
		plugin_event(event_connecting, user);
		p->login(user);
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
	struct prpl *prpl;
	while (gc->buddy_chats) {
		struct gaim_conversation *b = gc->buddy_chats->data;

		gc->buddy_chats = g_slist_remove(gc->buddy_chats, b);

		/* TODO: Nuke the UI-specific code here. */
		if (GAIM_IS_GTK_CONVERSATION(b))
			gaim_gtkconv_update_buttons_by_protocol(b);
	}

	if (gc->idle_timer > 0)
		g_source_remove(gc->idle_timer);
	gc->idle_timer = 0;

	update_keepalive(gc, FALSE);

	if (gc->prpl && gc->prpl->close)
		gc->prpl->close(gc);

	prpl = gc->prpl;
	account_offline(gc);
	destroy_gaim_conn(gc);

	unref_protocol(prpl);
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

	if (gc->prpl->options & OPT_PROTO_CORRECT_TIME)
		serv_add_buddy(gc, gc->username);

	update_keepalive(gc, TRUE);
}

/* This should return the elapsed time in seconds in which Gaim will not send
 * typing notifications.
 * if it returns zero, it will not send any more typing notifications 
 * typing is a flag - TRUE for typing, FALSE for stopped typing */
int serv_send_typing(struct gaim_connection *g, char *name, int typing) {
	if (g && g->prpl && g->prpl->send_typing)
		return g->prpl->send_typing(g, name, typing);
	else return 0;
}

struct queued_away_response {
	char name[80];
	time_t sent_away;
};

struct queued_away_response *find_queued_away_response_by_name(char *name);

int serv_send_im(struct gaim_connection *gc, char *name, char *message,
				 int len, int flags)
{
	struct gaim_conversation *c;
	int val = -EINVAL;

	c = gaim_find_conversation(name);

	if (gc->prpl && gc->prpl->send_im)
		val = gc->prpl->send_im(gc, name, message, len, flags);

	if (!(flags & IM_FLAG_AWAY))
		serv_touch_idle(gc);

	if (gc->away && away_options & OPT_AWAY_DELAY_IN_USE &&
			!(away_options & OPT_AWAY_NO_AUTO_RESP)) {
		time_t t;
		struct queued_away_response *qar;
		time(&t);
		qar = find_queued_away_response_by_name(name);
		if (!qar) {
			qar = (struct queued_away_response *)g_new0(struct queued_away_response, 1);
			g_snprintf(qar->name, sizeof(qar->name), "%s", name);
			qar->sent_away = 0;
			away_time_queue = g_slist_append(away_time_queue, qar);
		}
		qar->sent_away = t;
	}

	if (c && gaim_im_get_type_again_timeout(GAIM_IM(c)))
		gaim_im_stop_type_again_timeout(GAIM_IM(c));

	return val;
}

void serv_get_info(struct gaim_connection *g, char *name)
{
	if (g && g->prpl && g->prpl->get_info)
		g->prpl->get_info(g, name);
}

void serv_get_away(struct gaim_connection *g, char *name)
{
	if (g && g->prpl && g->prpl->get_away)
		g->prpl->get_away(g, name);
}

void serv_get_dir(struct gaim_connection *g, char *name)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->get_dir)
		g->prpl->get_dir(g, name);
}

void serv_set_dir(struct gaim_connection *g, const char *first,
				  const char *middle, const char *last, const char *maiden,
				  const char *city, const char *state, const char *country,
				  int web)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->set_dir)
		g->prpl->set_dir(g, first, middle, last, maiden, city, state,
						 country, web);
}

void serv_dir_search(struct gaim_connection *g, const char *first,
					 const char *middle, const char *last, const char *maiden,
		     const char *city, const char *state, const char *country,
			 const char *email)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->dir_search)
		g->prpl->dir_search(g, first, middle, last, maiden, city, state,
							country, email);
}


void serv_set_away(struct gaim_connection *gc, char *state, char *message)
{
	if (gc && gc->prpl && gc->prpl->set_away) {
		char *buf = NULL;

		if (gc->away_state) {
			g_free(gc->away_state);
			gc->away_state = NULL;
		}

		if (message) {
			buf = g_malloc(strlen(message) + 1);
			if (gc->flags & OPT_CONN_HTML)
				strncpy(buf, message, strlen(message) + 1);
			else
				strncpy_nohtml(buf, message, strlen(message) + 1);
		}

		gc->prpl->set_away(gc, state, buf);

		if (gc->away && state) {
			gc->away_state = g_strdup(state);
		}

		plugin_event(event_away, gc, state, buf);

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
		if (plugin_event(event_set_info, g, info))
			return;
		g->prpl->set_info(g, info);
	}
}

void serv_change_passwd(struct gaim_connection *g, const char *orig, const char *new)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->change_passwd)
		g->prpl->change_passwd(g, orig, new);
}

void serv_add_buddy(struct gaim_connection *g, const char *name)
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


void serv_remove_buddy(struct gaim_connection *g, char *name, char *group)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->remove_buddy)
		g->prpl->remove_buddy(g, name, group);
}

void serv_remove_buddies(struct gaim_connection *gc, GList *g, char *group)
{
	if (!g_slist_find(connections, gc))
		return;
	if (!gc->prpl)
		return;		/* how the hell did that happen? */
	if (gc->prpl->remove_buddies)
		gc->prpl->remove_buddies(gc, g, group);
	else {
		while (g) {
			serv_remove_buddy(gc, g->data, group);
			g = g->next;
		}
	}
}

/*
 * Set buddy's alias on server roster/list
 */
void serv_alias_buddy(struct buddy *b)
{
	if(b && b->user->gc && b->user->gc->prpl && b->user->gc->prpl->alias_buddy) {
		b->user->gc->prpl->alias_buddy(b->user->gc, b->name, b->alias);
	}
}

void serv_got_alias(struct gaim_connection *gc, char *who, char *alias) {
	struct buddy *b = find_buddy(gc->user, who);
	if(!b)
		return;

	if(alias)
		g_snprintf(b->server_alias, sizeof(b->server_alias), "%s", alias);
	else
		b->server_alias[0] = '\0';

	handle_buddy_rename(b, b->name);
}

/*
 * Move a buddy from one group to another on server.
 *
 * Note: For now we'll not deal with changing gc's at the same time, but
 * it should be possible.  Probably needs to be done, someday.
 */
void serv_move_buddy(struct buddy *b, struct group *og, struct group *ng)
{
	if(b && b->user->gc && og && ng) {
		if(b->user->gc->prpl && b->user->gc->prpl->group_buddy) {
			b->user->gc->prpl->group_buddy(b->user->gc, b->name, og->name, ng->name);
		}
	}
}

/*
 * Rename a group on server roster/list.
 */
void serv_rename_group(struct gaim_connection *g, struct group *old_group, const char *new_name)
{
	if (g && g->prpl && old_group && new_name) {
		GList *tobemoved = NULL;
		GSList *original;

		for (original=old_group->members; original; original=g_slist_next(original))
			tobemoved = g_list_append(tobemoved, ((struct buddy *)original->data)->name);

		if (g->prpl->rename_group) {
			/* prpl's might need to check if the group already 
			 * exists or not, and handle that differently */
			g->prpl->rename_group(g, old_group->name, new_name, tobemoved);
		} else {
			serv_remove_buddies(g, tobemoved, old_group->name);
			serv_add_buddies(g, tobemoved);
		}

		g_list_free(tobemoved);
	}
}

void serv_add_permit(struct gaim_connection *g, const char *name)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->add_permit)
		g->prpl->add_permit(g, name);
}

void serv_add_deny(struct gaim_connection *g, const char *name)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->add_deny)
		g->prpl->add_deny(g, name);
}

void serv_rem_permit(struct gaim_connection *g, const char *name)
{
	if (g && g_slist_find(connections, g) && g->prpl && g->prpl->rem_permit)
		g->prpl->rem_permit(g, name);
}

void serv_rem_deny(struct gaim_connection *g, const char *name)
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

void serv_chat_invite(struct gaim_connection *g, int id, const char *message, const char *name)
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
	gchar *temp;
	gint i = 0;
	gboolean valid;
	GtkTreeIter iter;

	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(awayqueuestore), &iter);
	while(valid) {
		gtk_tree_model_get(GTK_TREE_MODEL(awayqueuestore), &iter, 0, &temp, -1);
		if(!strcmp(name, temp))
			return i;
		g_free(temp);
		
		i++;
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(awayqueuestore), &iter);
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

/*
 * woo. i'm actually going to comment this function. isn't that fun. make
 * sure to follow along, kids
 */
void serv_got_im(struct gaim_connection *gc, char *name, char *message,
				 guint32 flags, time_t mtime, gint len)
{
	char *buffy;
	char *angel;
	int plugin_return;
	int away = 0;

	struct gaim_conversation *cnv;

	/*
	 * Pay no attention to the man behind the curtain.
	 *
	 * The reason i feel okay with this is because it's useful to some
	 * plugins. Gaim doesn't ever use it itself. Besides, it's not entirely
	 * accurate; it's possible to have false negatives with most protocols.
	 * Also with some it's easy to have false positives as well. So if you're
	 * a plugin author, don't rely on this, still do your own checks. But uh.
	 * It's a start.
	 */

	if (flags & IM_FLAG_GAIMUSER)
		debug_printf("%s is a gaim user.\n", name);

	/*
	 * We should update the conversation window buttons and menu,
	 * if it exists.
	 */
	cnv = gaim_find_conversation_with_user(name, gc->user);

	/*
	 * Plugin stuff. we pass a char ** but we don't want to pass what's
	 * been given us by the prpls. So we create temp holders and pass
	 * those instead. It's basically just to avoid segfaults. Of course,
	 * if the data is binary, plugins don't see it. Bitch all you want;
	 * I really don't want you to be dealing with it.
	 */
	if (len < 0) {
		buffy = g_malloc(MAX(strlen(message) + 1, BUF_LONG));
		strcpy(buffy, message);
		angel = g_strdup(name);
		plugin_return = plugin_event(event_im_recv, gc, &angel, &buffy, &flags);

		if (!buffy || !angel || plugin_return) {
			if (buffy)
				g_free(buffy);
			if (angel)
				g_free(angel);
			return;
		}
		name = angel;
		message = buffy;
	} else {
		name = g_strdup(name);
		message = g_memdup(message, len);
	}

#if 0
	/*
	 * TiK, using TOC, sends an automated message in order to get your
	 * away message. Now, this is one of the biggest hacks I think I've
	 * seen. But, in order to be nice to TiK, we're going to give users
	 * the option to ignore it.
	 */
	if ((away_options & OPT_AWAY_TIK_HACK) && gc->away &&
		strlen(gc->away) && (len < 0) &&

	    !strcmp(message, ">>>Automated Message: Getting Away Message<<<")) {
		char *tmpmsg = stylize(awaymessage->message, MSG_LEN);
		serv_send_im(gc, name, tmpmsg, -1, IM_FLAG_AWAY);
		g_free(tmpmsg);
		g_free(name);
		g_free(message);
		return;
	}
#endif

	/*
	 * If you can't figure this out, stop reading right now.
	 * "We're not worthy! We're not worthy!"
	 */
	if ((len < 0) && (convo_options & OPT_CONVO_SEND_LINKS))
		linkify_text(message);

	/*
	 * Um. When we call gaim_conversation_write with the message we received,
	 * it's nice to pass whether or not it was an auto-response. So if it
	 * was an auto-response, we set the appropriate flag. This is just so
	 * prpls don't have to know about WFLAG_* (though some do anyway)
	 */
	if (flags & IM_FLAG_AWAY)
		away = WFLAG_AUTO;

	/*
	 * Alright. Two cases for how to handle this. Either we're away or
	 * we're not. If we're not, then it's easy. If we are, then there
	 * are three or four different ways of handling it and different
	 * things we have to do for each.
	 */
	if (gc->away) {
		time_t t;
		char *tmpmsg;
		struct buddy *b = find_buddy(gc->user, name);
		char *alias = b ? get_buddy_alias(b) : name;
		int row;
		struct queued_away_response *qar;

		time(&t);

		/*
		 * Either we're going to queue it or not. Because of the way
		 * awayness currently works, this is fucked up. It's possible
		 * for an account to be away without the imaway dialog being
		 * shown. In fact, it's possible for *all* the accounts to be
		 * away without the imaway dialog being shown. So in order for
		 * this to be queued properly, we have to make sure that the
		 * imaway dialog actually exists, first.
		 */
		if (!cnv && awayqueue && (away_options & OPT_AWAY_QUEUE)) {
			/* 
			 * Alright, so we're going to queue it. Neat, eh? :)
			 * So first we create something to store the message, and add
			 * it to our queue. Then we update the away dialog to indicate
			 * that we've queued something.
			 */
			struct queued_message *qm;
			GtkTreeIter iter;
			gchar path[10];

			qm = g_new0(struct queued_message, 1);
				g_snprintf(qm->name, sizeof(qm->name), "%s", name);
			qm->message = g_memdup(message, len == -1 ? strlen(message) + 1 : len);
			qm->gc = gc;
			qm->tm = mtime;
			qm->flags = WFLAG_RECV | away;
			qm->len = len;
			message_queue = g_slist_append(message_queue, qm);

			row = find_queue_row_by_name(qm->name);
			if (row >= 0) {
				char number[32];
				int qtotal;

				qtotal = find_queue_total_by_name(qm->name);
				g_snprintf(number, 32, _("(%d messages)"), qtotal);
				g_snprintf(path, 10, "%d", row);
				gtk_tree_model_get_iter_from_string(
								GTK_TREE_MODEL(awayqueuestore), &iter, path);
				gtk_list_store_set(awayqueuestore, &iter,
								1, number, -1);
			} else {
				gtk_tree_model_get_iter_first(GTK_TREE_MODEL(awayqueuestore), 
								&iter);
				gtk_list_store_append(awayqueuestore, &iter);
				gtk_list_store_set(awayqueuestore, &iter,
								0, qm->name,
								1, _("(1 message)"),
								-1);
			}
		} else {
			/*
			 * Ok, so we're not queuing it. Well then, we'll try to handle
			 * it normally. Some people think that ignoring it is a perfectly
			 * acceptible way to handle it. I think they're on crack, but
			 * hey, that's why it's optional.
			 */
			if (away_options & OPT_AWAY_DISCARD) {
				g_free(name);
				g_free(message);
				return;
			}

			/*
			 * Ok, so we're not ignoring it. Make sure the conversation
			 * exists and is updated (partly handled above already), play
			 * the receive sound (sound.c will take care of not playing
			 * while away), and then write it to the convo window.
			 */
			if (cnv == NULL) {
				cnv = gaim_conversation_new(GAIM_CONV_IM, name);
				gaim_conversation_set_user(cnv, gc->user);
			}

			gaim_im_write(GAIM_IM(cnv), NULL, message, len,
						  away | WFLAG_RECV, mtime);
		}

		/*
		 * Regardless of whether we queue it or not, we should send an
		 * auto-response. That is, of course, unless the horse.... no wait.
		 * Don't autorespond if:
		 *
		 *  - it's not supported on this connection
		 *  - or it's disabled
		 *  - or the away message is empty
		 *  - or we're not idle and the 'only auto respond if idle' pref
		 *    is set
		 */
		if (!(gc->flags & OPT_CONN_AUTO_RESP) ||
			(away_options & OPT_AWAY_NO_AUTO_RESP) || !strlen(gc->away) ||
			((away_options & OPT_AWAY_IDLE_RESP) && !gc->is_idle)) {

			g_free(name);
			g_free(message);
			return;
		}

		/*
		 * This used to be based on the conversation window. But um, if
		 * you went away, and someone sent you a message and got your
		 * auto-response, and then you closed the window, and then the
		 * sent you another one, they'd get the auto-response back too
		 * soon. Besides that, we need to keep track of this even if we've
		 * got a queue. So the rest of this block is just the auto-response,
		 * if necessary
		 */
		qar = find_queued_away_response_by_name(name);
		if (!qar) {
			qar = (struct queued_away_response *)g_new0(struct queued_away_response, 1);
			g_snprintf(qar->name, sizeof(qar->name), "%s", name);
			qar->sent_away = 0;
			away_time_queue = g_slist_append(away_time_queue, qar);
		}
		if ((t - qar->sent_away) < away_resend) {
			g_free(name);
			g_free(message);
			return;
		}
		qar->sent_away = t;

		/* apply default fonts and colors */
		tmpmsg = stylize(gc->away, MSG_LEN);
		serv_send_im(gc, name, away_subs(tmpmsg, alias), -1, IM_FLAG_AWAY);
		if (!cnv && awayqueue && (away_options & OPT_AWAY_QUEUE)) {
			struct queued_message *qm;
			qm = g_new0(struct queued_message, 1);
			g_snprintf(qm->name, sizeof(qm->name), "%s", name);
			qm->message = g_strdup(away_subs(tmpmsg, alias));
			qm->gc = gc;
			qm->tm = mtime;
			qm->flags = WFLAG_SEND | WFLAG_AUTO;
			qm->len = -1;
			message_queue = g_slist_append(message_queue, qm);
		} else if (cnv != NULL)
			gaim_im_write(GAIM_IM(cnv), NULL, away_subs(tmpmsg, alias),
						  len, WFLAG_SEND | WFLAG_AUTO, mtime);

		g_free(tmpmsg);
	} else {
		/*
		 * We're not away. This is easy. If the convo window doesn't
		 * exist, create and update it (if it does exist it was updated
		 * earlier), then play a sound indicating we've received it and
		 * then display it. Easy.
		 */
		if (away_options & OPT_AWAY_QUEUE_UNREAD &&
			!gaim_find_conversation(name) && docklet_count) {

			/*
			 * We're gonna queue it up and wait for the user to ask for
			 * it... probably by clicking the docklet or windows tray icon.
			 */
			struct queued_message *qm;
			qm = g_new0(struct queued_message, 1);
			g_snprintf(qm->name, sizeof(qm->name), "%s", name);
			qm->message = g_strdup(message);
			qm->gc = gc;
			qm->tm = mtime;
			qm->flags = away | WFLAG_RECV;
			qm->len = len;
			unread_message_queue = g_slist_append(unread_message_queue, qm);
		} else {
			if (cnv == NULL) {
				cnv = gaim_conversation_new(GAIM_CONV_IM, name);
				gaim_conversation_set_user(cnv, gc->user);
			}

			/* CONV XXX gaim_conversation_set_name(cnv, name); */

			gaim_im_write(GAIM_IM(cnv), NULL, message, len,
						  away | WFLAG_RECV, mtime);
			gaim_window_flash(gaim_conversation_get_window(cnv));
		}
	}

	plugin_event(event_im_displayed_rcvd, gc, name, message, flags, mtime);
	g_free(name);
	g_free(message);
}



void serv_got_update(struct gaim_connection *gc, char *name, int loggedin,
					 int evil, time_t signon, time_t idle, int type, guint caps)
{
	struct buddy *b = find_buddy(gc->user, name);

	if (signon && (gc->prpl->options & OPT_PROTO_CORRECT_TIME)) {
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
		char *who = g_strdup(b->name);
		g_snprintf(b->name, sizeof(b->name), "%s", name);
		handle_buddy_rename(b, who);
		gaim_blist_save();
		g_free(who);
	}

	if (!b->idle && idle) {
		plugin_event(event_buddy_idle, gc, b->name);
		system_log(log_idle, gc, b, OPT_LOG_BUDDY_IDLE);
	}
	if (b->idle && !idle) {
		do_pounce(gc, b->name, OPT_POUNCE_UNIDLE);
		plugin_event(event_buddy_unidle, gc, b->name);
		system_log(log_unidle, gc, b, OPT_LOG_BUDDY_IDLE);
	}

	b->idle = idle;
	b->evil = evil;

	if ((b->uc & UC_UNAVAILABLE) && !(type & UC_UNAVAILABLE)) {
		do_pounce(gc, b->name, OPT_POUNCE_UNAWAY);
		plugin_event(event_buddy_back, gc, b->name);
		system_log(log_back, gc, b, OPT_LOG_BUDDY_AWAY);
	} else if (!(b->uc & UC_UNAVAILABLE) && (type & UC_UNAVAILABLE)) {
		plugin_event(event_buddy_away, gc, b->name);
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
			plugin_event(event_buddy_signon, gc, b->name);
			system_log(log_signon, gc, b, OPT_LOG_BUDDY_SIGNON);
		}
	} else {
		if (b->present) {
			plugin_event(event_buddy_signoff, gc, b->name);
			system_log(log_signoff, gc, b, OPT_LOG_BUDDY_SIGNON);
		}
		b->present = 0;
	}

	set_buddy(gc, b);
}


void serv_got_eviled(struct gaim_connection *gc, char *name, int lev)
{
	char buf2[1024];

	plugin_event(event_warned, gc, name, lev);

	if (gc->evil >= lev) {
		gc->evil = lev;
		return;
	}

	gc->evil = lev;

	g_snprintf(buf2, sizeof(buf2),
			   _("%s has just been warned by %s.\n"
				 "Your new warning level is %d%%"),
			   gc->username,
			   ((name == NULL)? _("an anonymous person") : name), lev);

	do_error_dialog(buf2, NULL, GAIM_INFO);
}

void serv_got_typing(struct gaim_connection *gc, char *name, int timeout,
					 int state) {

	struct gaim_conversation *cnv = gaim_find_conversation(name);
	struct gaim_im *im;

	if (!cnv)
		return;

	im = GAIM_IM(cnv);

	gaim_conversation_set_user(cnv, gc->user);
	gaim_im_set_typing_state(im, state);
	gaim_im_update_typing(im);

	plugin_event(event_got_typing, gc, name);
	do_pounce(gc, name, OPT_POUNCE_TYPING);

	if (timeout > 0)
		gaim_im_start_typing_timeout(im, timeout);
}

void serv_got_typing_stopped(struct gaim_connection *gc, char *name) {

	struct gaim_conversation *c = gaim_find_conversation(name);
	struct gaim_im *im;

	if (!c)
		return;

	im = GAIM_IM(c);

	gaim_im_stop_typing_timeout(im);
	gaim_im_set_typing_state(im, NOT_TYPING);
	gaim_im_update_typing(im);
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



void serv_got_chat_invite(struct gaim_connection *g, char *name,
						  char *who, char *message, GList *data)
{
	GtkWidget *d;
	GtkWidget *label;
	GtkWidget *yesbtn;
	GtkWidget *nobtn;

	char buf2[BUF_LONG];


	plugin_event(event_chat_invited, g, who, name, message);

	if (message)
		g_snprintf(buf2, sizeof(buf2),
				   _("User '%s' invites %s to buddy chat room: '%s'\n%s"),
				   who, g->username, name, message);
	else
		g_snprintf(buf2, sizeof(buf2),
				   _("User '%s' invites %s to buddy chat room: '%s'\n"),
				   who, g->username, name);

	d = gtk_dialog_new();
	gtk_widget_realize(d);


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


	gtk_window_set_title(GTK_WINDOW(d), _("Buddy chat invite"));
	g_signal_connect(G_OBJECT(nobtn), "clicked",
					 G_CALLBACK(close_invite), d);
	g_signal_connect(G_OBJECT(yesbtn), "clicked",
					 G_CALLBACK(chat_invite_callback), d);

	gtk_widget_show(d);
}

struct gaim_conversation *serv_got_joined_chat(struct gaim_connection *gc,
											   int id, char *name)
{
	struct gaim_conversation *b;
	struct gaim_chat *chat;

	b = gaim_conversation_new(GAIM_CONV_CHAT, name);
	chat = GAIM_CHAT(b);

	gc->buddy_chats = g_slist_append(gc->buddy_chats, b);

	gaim_chat_set_id(chat, id);
	gaim_conversation_set_user(b, gc->user);
	
	if ((logging_options & OPT_LOG_CHATS) ||
		find_log_info(gaim_conversation_get_name(b))) {

		FILE *fd;
		char *filename;

		filename = (char *)malloc(100);
		g_snprintf(filename, 100, "%s.chat", gaim_conversation_get_name(b));

		fd = open_log_file(filename, TRUE);
		
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

	gaim_window_show(gaim_conversation_get_window(b));
	gaim_window_switch_conversation(gaim_conversation_get_window(b),
									gaim_conversation_get_index(b));

	plugin_event(event_chat_join, gc, id, name);

	return b;
}

void serv_got_chat_left(struct gaim_connection *g, int id)
{
	GSList *bcs;
	struct gaim_conversation *conv = NULL;
	struct gaim_chat *chat = NULL;

	for (bcs = g->buddy_chats; bcs != NULL; bcs = bcs->next) {
		conv = (struct gaim_conversation *)bcs->data;

		chat = GAIM_CHAT(conv);

		if (gaim_chat_get_id(chat) == id)
			break;

		conv = NULL;
	}

	if (!conv)
		return;

	plugin_event(event_chat_leave, g, gaim_chat_get_id(chat));

	debug_printf("Leaving room %s.\n", gaim_conversation_get_name(conv));

	g->buddy_chats = g_slist_remove(g->buddy_chats, conv);

	gaim_conversation_destroy(conv);
}

void serv_got_chat_in(struct gaim_connection *g, int id, char *who,
					  int whisper, char *message, time_t mtime)
{
	int w;
	GSList *bcs;
	struct gaim_conversation *conv = NULL;
	struct gaim_chat *chat = NULL;
	char *buf;
	char *buffy, *angel;
	int plugin_return;

	for (bcs = g->buddy_chats; bcs != NULL; bcs = bcs->next) {
		conv = (struct gaim_conversation *)bcs->data;

		chat = GAIM_CHAT(conv);

		if (gaim_chat_get_id(chat) == id)
			break;

		conv = NULL;
	}

	if (!conv)
		return;

	/*
	 * Plugin stuff. We pass a char ** but we don't want to pass what's
	 * been given us by the prpls. so we create temp holders and pass those
	 * instead. It's basically just to avoid segfaults. Of course, if the
	 * data is binary, plugins don't see it. Bitch all you want; i really
	 * don't want you to be dealing with it.
	 */

	buffy = g_malloc(MAX(strlen(message) + 1, BUF_LONG));
	strcpy(buffy, message);
	angel = g_strdup(who);
	plugin_return = plugin_event(event_chat_recv, g, gaim_chat_get_id(chat),
								 &angel, &buffy);

	if (!buffy || !angel || plugin_return) {
		if (buffy)
			g_free(buffy);
		if (angel)
			g_free(angel);
		return;
	}
	who = angel;
	message = buffy;

	buf = g_malloc(MAX(strlen(message) * 2, 8192));
	strcpy(buf, message);

	if (convo_options & OPT_CONVO_SEND_LINKS)
		linkify_text(buf);

	if (whisper)
		w = WFLAG_WHISPER;
	else
		w = 0;

	gaim_chat_write(chat, who, buf, w, mtime);

	g_free(who);
	g_free(message);
	g_free(buf);
}

static void des_popup(GtkWidget *w, GtkWidget *window)
{
	if (w == window) {
		char *u = gtk_object_get_user_data(GTK_OBJECT(window));
		g_free(u);
	}
	gtk_widget_destroy(window);
}

void serv_got_popup(char *msg, char *u, int wid, int hei)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *sw;
	GtkWidget *text;
	GtkWidget *hbox;
	GtkWidget *button;
	char *url = g_strdup(u);

	GAIM_DIALOG(window);
	gtk_window_set_role(GTK_WINDOW(window), "popup");
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, TRUE);
	gtk_window_set_title(GTK_WINDOW(window), "Gaim - Popup");
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);
	g_signal_connect(GTK_OBJECT(window), "destroy", G_CALLBACK(des_popup), window);
	gtk_object_set_user_data(GTK_OBJECT(window), url);
	gtk_widget_realize(window);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_usize(sw, wid, hei);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 5);

	text = gtk_imhtml_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(sw), text);
	gaim_setup_imhtml(text);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	button = picture_button(window, _("Close"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(des_popup), window);

	button = picture_button(window, _("More Info"), tb_search_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(open_url), url);

	gtk_widget_show_all(window);

	gtk_imhtml_append_text(GTK_IMHTML(text), msg, -1, GTK_IMHTML_NO_NEWLINE);
}
