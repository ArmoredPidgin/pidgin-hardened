/*
 * Mouse gestures plugin for Gaim
 *
 * Copyright (C) 2003 Christian Hammond.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#include "config.h"

#include "gaim.h"
#include "gstroke.h"
#include "gtkconv.h"
#include "gtkplugin.h"

#define GESTURES_PLUGIN_ID "gtk-gestures"

static void
stroke_close(GtkWidget *widget, void *data)
{
	struct gaim_conversation *conv;
	struct gaim_gtk_conversation *gtkconv;

	conv = (struct gaim_conversation *)data;

	/* Double-check */
	if (!GAIM_IS_GTK_CONVERSATION(conv))
		return;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	gstroke_cleanup(gtkconv->imhtml);
	gaim_conversation_destroy(conv);
}

static void
stroke_prev_tab(GtkWidget *widget, void *data)
{
	struct gaim_conversation *conv;
	struct gaim_window *win;
	unsigned int index;

	conv  = (struct gaim_conversation *)data;
	win   = gaim_conversation_get_window(conv);
	index = gaim_conversation_get_index(conv);

	if (index == 0)
		index = gaim_window_get_conversation_count(win) - 1;
	else
		index--;

	gaim_window_switch_conversation(win, index);
}

static void
stroke_next_tab(GtkWidget *widget, void *data)
{
	struct gaim_conversation *conv;
	struct gaim_window *win;
	unsigned int index;

	conv  = (struct gaim_conversation *)data;
	win   = gaim_conversation_get_window(conv);
	index = gaim_conversation_get_index(conv);

	if (index == gaim_window_get_conversation_count(win) - 1)
		index = 0;
	else
		index++;

	gaim_window_switch_conversation(win, index);
}

void
stroke_new_win(GtkWidget *widget, void *data)
{
	struct gaim_window *new_win, *old_win;
	struct gaim_conversation *conv;

	conv    = (struct gaim_conversation *)data;
	old_win = gaim_conversation_get_window(conv);

	if (gaim_window_get_conversation_count(old_win) <= 1)
		return;

	new_win = gaim_window_new();

	gaim_window_remove_conversation(old_win, gaim_conversation_get_index(conv));
	gaim_window_add_conversation(new_win, conv);

	gaim_window_show(new_win);
}

static void
attach_signals(struct gaim_conversation *conv)
{
	struct gaim_gtk_conversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	gstroke_enable(gtkconv->imhtml);
	gstroke_signal_connect(gtkconv->imhtml, "14789",  stroke_close,    conv);
	gstroke_signal_connect(gtkconv->imhtml, "1456",   stroke_close,    conv);
	gstroke_signal_connect(gtkconv->imhtml, "1489",   stroke_close,    conv);
	gstroke_signal_connect(gtkconv->imhtml, "74123",  stroke_next_tab, conv);
	gstroke_signal_connect(gtkconv->imhtml, "7456",   stroke_next_tab, conv);
	gstroke_signal_connect(gtkconv->imhtml, "96321",  stroke_prev_tab, conv);
	gstroke_signal_connect(gtkconv->imhtml, "9654",   stroke_prev_tab, conv);
	gstroke_signal_connect(gtkconv->imhtml, "25852",  stroke_new_win,  conv);
}

static void
new_conv_cb(char *who)
{
	struct gaim_conversation *conv;

	conv = gaim_find_conversation(who);

	if (conv == NULL || !GAIM_IS_GTK_CONVERSATION(conv))
		return;

	attach_signals(conv);
}

#if 0
static void
mouse_button_menu_cb(GtkMenuItem *item, gpointer data)
{
	int button = (int)data;

	gstroke_set_mouse_button(button + 2);
}
#endif

static void
toggle_draw_cb(GtkToggleButton *toggle, gpointer data)
{
	gstroke_set_draw_strokes(!gstroke_draw_strokes());
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	struct gaim_conversation *conv;
	GList *l;

	for (l = gaim_get_conversations(); l != NULL; l = l->next) {
		conv = (struct gaim_conversation *)l->data;

		if (!GAIM_IS_GTK_CONVERSATION(conv))
			continue;

		attach_signals(conv);
	}

	gaim_signal_connect(plugin, event_new_conversation, new_conv_cb, NULL);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	struct gaim_conversation *conv;
	struct gaim_gtk_conversation *gtkconv;
	GList *l;

	for (l = gaim_get_conversations(); l != NULL; l = l->next) {
		conv = (struct gaim_conversation *)l->data;

		if (!GAIM_IS_GTK_CONVERSATION(conv))
			continue;

		gtkconv = GAIM_GTK_CONVERSATION(conv);

		gstroke_cleanup(gtkconv->imhtml);
	}

	return TRUE;
}

static GtkWidget *
get_config_frame(GaimPlugin *plugin)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *toggle;
#if 0
	GtkWidget *opt;
	GtkWidget *menu, *item;
#endif

	/* Outside container */
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 12);

	/* Configuration frame */
	vbox = make_frame(ret, _("Mouse Gestures Configuration"));

#if 0
	/* Mouse button drop-down menu */
	menu = gtk_menu_new();
	opt = gtk_option_menu_new();

	item = gtk_menu_item_new_with_label(_("Middle mouse button"));
	g_signal_connect(G_OBJECT(item), "activate",
					 G_CALLBACK(mouse_button_menu_cb), opt);
	gtk_menu_append(menu, item);

	item = gtk_menu_item_new_with_label(_("Right mouse button"));
	g_signal_connect(G_OBJECT(item), "activate",
					 G_CALLBACK(mouse_button_menu_cb), opt);
	gtk_menu_append(menu, item);

	gtk_box_pack_start(GTK_BOX(vbox), opt, FALSE, FALSE, 0);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(opt),
								gstroke_get_mouse_button() - 2);
#endif

	/* "Visual gesture display" checkbox */
	toggle = gtk_check_button_new_with_mnemonic(_("_Visual gesture display"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
								 gstroke_draw_strokes());
	g_signal_connect(G_OBJECT(toggle), "toggled",
					 G_CALLBACK(toggle_draw_cb), NULL);

	gtk_widget_show_all(ret);

	return ret;
}

static GaimGtkPluginUiInfo ui_info =
{
	get_config_frame
};

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	GAIM_GTK_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	GESTURES_PLUGIN_ID,                               /**< id             */
	N_("Mouse Gestures"),                             /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Provides support for mouse gestures"),
	                                                  /**  description    */
	N_("Allows support for mouse gestures in conversation windows.\n"
	   "Drag the middle mouse button to perform certain actions:\n\n"
	   "Drag down and then to the right to close a conversation.\n"
	   "Drag up and then to the left to switch to the previous "
	   "conversation.\n"
	   "Drag up and then to the right to switch to the next "
	   "conversation."),
	"Christian Hammond <chipx86@gnupdate.org>",       /**< author         */
	WEBSITE,                                          /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
__init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(gestures, __init_plugin, info);
