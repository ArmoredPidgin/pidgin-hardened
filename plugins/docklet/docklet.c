/*
 * System tray icon (aka docklet) plugin for Gaim
 *
 * Copyright (C) 2002-3 Robert McQueen <robot101@debian.org>
 * Copyright (C) 2003 Herman Bloggs <hermanator12002@yahoo.com>
 * Inspired by a similar plugin by:
 *  John (J5) Palmieri <johnp@martianrock.com>
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
#include "internal.h"
#include "gtkgaim.h"

#include "core.h"
#include "conversation.h"
#include "debug.h"
#include "prefs.h"
#include "signals.h"
#include "sound.h"
#include "version.h"

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkft.h"
#include "gtkplugin.h"
#include "gtkprefs.h"
#include "gtksavedstatuses.h"
#include "gtksound.h"
#include "gtkutils.h"
#include "gtkstock.h"
#include "docklet.h"

#include "gaim.h"
#include "gtkdialogs.h"

#define DOCKLET_PLUGIN_ID "gtk-docklet"

#ifndef DOCKLET_TOOLTIP_LINE_LIMIT
#define DOCKLET_TOOLTIP_LINE_LIMIT 5
#endif

/* globals */
GaimPlugin *handle = NULL;
static struct docklet_ui_ops *ui_ops = NULL;
static DockletStatus status = DOCKLET_STATUS_OFFLINE;
static gboolean enable_join_chat = FALSE;
static guint docklet_blinking_timer = 0;
static gboolean visibility_manager = FALSE;

/**************************************************************************
 * docklet status and utility functions
 **************************************************************************/
static gboolean
docklet_blink_icon()
{
	static gboolean blinked = FALSE;
	gboolean ret = FALSE; /* by default, don't keep blinking */

	blinked = !blinked;

	switch (status) {
		case DOCKLET_STATUS_ONLINE_PENDING:
		case DOCKLET_STATUS_AWAY_PENDING:
			if (blinked) {
				if (ui_ops && ui_ops->blank_icon)
					ui_ops->blank_icon();
			} else {
				if (ui_ops && ui_ops->update_icon)
					ui_ops->update_icon(status);
			}
			ret = TRUE; /* keep blinking */
			break;
		default:
			docklet_blinking_timer = 0;
			blinked = FALSE;
			break;
	}

	return ret;
}

static GList *
get_pending_list(guint max)
{
	const char *im = gaim_prefs_get_string("/plugins/gtk/docklet/blink_im");
	const char *chat = gaim_prefs_get_string("/plugins/gtk/docklet/blink_chat");
	GList *l_im = NULL;
	GList *l_chat = NULL;

	if (im != NULL && strcmp(im, "always") == 0) {
		l_im = gaim_gtk_conversations_find_unseen_list(GAIM_CONV_TYPE_IM,
													   GAIM_UNSEEN_TEXT,
													   FALSE, max);
	} else if (im != NULL && strcmp(im, "hidden") == 0) {
		l_im = gaim_gtk_conversations_find_unseen_list(GAIM_CONV_TYPE_IM,
													   GAIM_UNSEEN_TEXT,
													   TRUE, max);
	}

	if (chat != NULL && strcmp(chat, "always") == 0) {
		l_chat = gaim_gtk_conversations_find_unseen_list(GAIM_CONV_TYPE_CHAT,
													   GAIM_UNSEEN_TEXT,
													   FALSE, max);
	} else if (chat != NULL && strcmp(chat, "nick") == 0) {
		l_chat = gaim_gtk_conversations_find_unseen_list(GAIM_CONV_TYPE_CHAT,
													   GAIM_UNSEEN_NICK,
													   FALSE, max);
	}

	if (l_im != NULL && l_chat != NULL)
		return g_list_concat(l_im, l_chat);
	else if (l_im != NULL)
		return l_im;
	else
		return l_chat;
}

static gboolean
docklet_update_status()
{
	GList *convs;
	GList *l;
	int count;
	DockletStatus newstatus = DOCKLET_STATUS_OFFLINE;
	gboolean pending = FALSE;

	/* determine if any ims have unseen messages */
	convs = get_pending_list(DOCKLET_TOOLTIP_LINE_LIMIT);

	if (convs != NULL) {
		pending = TRUE;

		/* set tooltip if messages are pending */
		if (ui_ops->set_tooltip) {
			GString *tooltip_text = g_string_new("");
			for (l = convs, count = 0 ; l != NULL ; l = l->next, count++) {
				if (GAIM_IS_GTK_CONVERSATION(l->data)) {
					GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION((GaimConversation *)l->data);
					if (count == DOCKLET_TOOLTIP_LINE_LIMIT - 1)
						g_string_append(tooltip_text, _("Right-click for more unread messages...\n"));
					else
						g_string_append_printf(tooltip_text,
							ngettext("%d unread message from %s\n", "%d unread messages from %s\n", gtkconv->unseen_count),
							gtkconv->unseen_count,
							gtk_label_get_text(GTK_LABEL(gtkconv->tab_label)));
				}
			}

			/* get rid of the last newline */
			if (tooltip_text->len > 0)
				tooltip_text = g_string_truncate(tooltip_text, tooltip_text->len - 1);

			ui_ops->set_tooltip(tooltip_text->str);

			g_string_free(tooltip_text, TRUE);
		}

		g_list_free(convs);

	} else if (ui_ops->set_tooltip) {
		ui_ops->set_tooltip(NULL);
	}

	/* iterate through all accounts and determine which
	 * status to show in the tray icon based on the following
	 * ranks (highest encountered rank will be used):
	 *
	 *     1) OFFLINE
	 *     2) ONLINE
	 *     3) ONLINE_PENDING
	 *     4) AWAY
	 *     5) AWAY_PENDING
	 *     6) CONNECTING
	 */
	for(l = gaim_accounts_get_all(); l != NULL; l = l->next) {
		DockletStatus tmpstatus = DOCKLET_STATUS_OFFLINE;

		GaimAccount *account = (GaimAccount*)l->data;
		GaimStatus *account_status;

		if (!gaim_account_get_enabled(account, GAIM_GTK_UI))
			continue;

		if (gaim_account_is_disconnected(account))
			continue;

		account_status = gaim_account_get_active_status(account);

		if (gaim_account_is_connecting(account)) {
			tmpstatus = DOCKLET_STATUS_CONNECTING;
		} else if (gaim_status_is_online(account_status)) {
			if (!gaim_status_is_available(account_status)) {
				if (pending)
					tmpstatus = DOCKLET_STATUS_AWAY_PENDING;
				else
					tmpstatus = DOCKLET_STATUS_AWAY;
			}
			else {
				if (pending)
					tmpstatus = DOCKLET_STATUS_ONLINE_PENDING;
				else
					tmpstatus = DOCKLET_STATUS_ONLINE;
			}
		}

		if (tmpstatus > newstatus)
			newstatus = tmpstatus;
	}

	/* update the icon if we changed status */
	if (status != newstatus) {
		status = newstatus;

		if (ui_ops && ui_ops->update_icon)
			ui_ops->update_icon(status);

		/* and schedule the blinker function if messages are pending */
		if ((status == DOCKLET_STATUS_ONLINE_PENDING
				|| status == DOCKLET_STATUS_AWAY_PENDING)
			&& docklet_blinking_timer == 0) {
				docklet_blinking_timer = g_timeout_add(500, docklet_blink_icon, &handle);
		}
	}

	return FALSE; /* for when we're called by the glib idle handler */
}

static gboolean
online_account_supports_chat()
{
	GList *c = NULL;
	c = gaim_connections_get_all();

	while(c != NULL) {
		GaimConnection *gc = c->data;
		GaimPluginProtocolInfo *prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
		if (prpl_info != NULL && prpl_info->chat_info != NULL)
			return TRUE;
		c = c->next;
	}

	return FALSE;
}

/**************************************************************************
 * callbacks and signal handlers
 **************************************************************************/
static void 
gaim_quit_cb() 
{
	/* TODO: confirm quit while pending */
}

static void
docklet_update_status_cb(void *data)
{
	docklet_update_status();
}

static void
docklet_prefs_cb(const char *name, GaimPrefType type,
				 gpointer val, gpointer data)
{
	docklet_update_status();
}

static void
docklet_conv_updated_cb(GaimConversation *conv, GaimConvUpdateType type)
{
	if (type == GAIM_CONV_UPDATE_UNSEEN)
		docklet_update_status();
}

static void
docklet_signed_on_cb(GaimConnection *gc)
{
	if (!enable_join_chat) {
		if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info != NULL)
			enable_join_chat = TRUE;
	}
	docklet_update_status();
}

static void
docklet_signed_off_cb(GaimConnection *gc)
{
	if (enable_join_chat) {
		if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info != NULL)
			enable_join_chat = online_account_supports_chat();
	}
	docklet_update_status();
}

/**************************************************************************
 * docklet pop-up menu
 **************************************************************************/
static void
docklet_toggle_mute(GtkWidget *toggle, void *data)
{
	gaim_prefs_set_bool("/gaim/gtk/sound/mute", GTK_CHECK_MENU_ITEM(toggle)->active);
}

static void
docklet_toggle_blist(GtkWidget *toggle, void *data)
{
	gaim_blist_set_visible(GTK_CHECK_MENU_ITEM(toggle)->active);
}

#ifdef _WIN32
/* This is a workaround for a bug in windows GTK+. Clicking outside of the
   menu does not get rid of it, so instead we get rid of it as soon as the
   pointer leaves the menu. */
static gboolean 
hide_docklet_menu(gpointer data)
{
	if (data != NULL) {
		gtk_menu_popdown(GTK_MENU(data));
	}
	return FALSE;
}

static gboolean
docklet_menu_leave_enter(GtkWidget *menu, GdkEventCrossing *event, void *data)
{
	static guint hide_docklet_timer = 0;
	if (event->type == GDK_LEAVE_NOTIFY && event->detail == GDK_NOTIFY_ANCESTOR) {
		gaim_debug(GAIM_DEBUG_INFO, "docklet", "menu leave-notify-event\n");
		/* Add some slop so that the menu doesn't annoyingly disappear when mousing around */
		if (hide_docklet_timer == 0) {
			hide_docklet_timer = gaim_timeout_add(500,
					hide_docklet_menu, menu);
		}
	} else if (event->type == GDK_ENTER_NOTIFY && event->detail == GDK_NOTIFY_ANCESTOR) {
		gaim_debug(GAIM_DEBUG_INFO, "docklet", "menu enter-notify-event\n");
		if (hide_docklet_timer != 0) {
			/* Cancel the hiding if we reenter */

			gaim_timeout_remove(hide_docklet_timer);
			hide_docklet_timer = 0;
		}
	}
	return FALSE;
}
#endif

static void
show_custom_status_editor_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	gaim_gtk_status_editor_show(NULL);
}

static void
activate_status_primitive_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	GaimStatusPrimitive primitive;
	GaimSavedStatus *saved_status;

	primitive = GPOINTER_TO_INT(user_data);

	/* Create a new transient saved status */
	saved_status = gaim_savedstatus_new(NULL, primitive);
	gaim_savedstatus_set_type(saved_status, primitive);

	/* Set the status for each account */
	gaim_savedstatus_activate(saved_status);
}

static void
activate_saved_status_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	time_t creation_time;
	GaimSavedStatus *saved_status;

	creation_time = GPOINTER_TO_INT(user_data);
	saved_status = gaim_savedstatus_find_by_creation_time(creation_time);
	if (saved_status != NULL)
		gaim_savedstatus_activate(saved_status);
}

static GtkWidget *
docklet_status_submenu()
{
	GtkWidget *submenu, *menuitem;
	GList *popular_statuses, *cur;

	submenu = gtk_menu_new();
	menuitem = gtk_menu_item_new_with_label(_("Change Status"));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

	gaim_new_item_from_stock(submenu, _("Available"),
		GAIM_STOCK_ICON_AWAY, G_CALLBACK(activate_status_primitive_cb),
		GINT_TO_POINTER(GAIM_STATUS_AVAILABLE), 0, 0, NULL);

	gaim_new_item_from_stock(submenu, _("Away"),
		GAIM_STOCK_ICON_AWAY, G_CALLBACK(activate_status_primitive_cb),
		GINT_TO_POINTER(GAIM_STATUS_AWAY), 0, 0, NULL);

	gaim_new_item_from_stock(submenu, _("Invisible"),
		GAIM_STOCK_ICON_AWAY, G_CALLBACK(activate_status_primitive_cb),
		GINT_TO_POINTER(GAIM_STATUS_INVISIBLE), 0, 0, NULL);

	gaim_new_item_from_stock(submenu, _("Offline"),
		GAIM_STOCK_ICON_AWAY, G_CALLBACK(activate_status_primitive_cb),
		GINT_TO_POINTER(GAIM_STATUS_OFFLINE), 0, 0, NULL);

	popular_statuses = gaim_savedstatuses_get_popular(6);
	if (popular_statuses != NULL)
		gaim_separator(submenu);
	for (cur = popular_statuses; cur != NULL; cur = cur->next)
	{
		GaimSavedStatus *saved_status = cur->data;
		time_t creation_time = gaim_savedstatus_get_creation_time(saved_status);
		gaim_new_item_from_stock(submenu,
			gaim_savedstatus_get_title(saved_status),
			GAIM_STOCK_ICON_AWAY, G_CALLBACK(activate_saved_status_cb),
			GINT_TO_POINTER(creation_time), 0, 0, NULL);
	}
	g_list_free(popular_statuses);

	gaim_separator(submenu);

	gaim_new_item_from_stock(submenu, _("Custom Status..."), GAIM_STOCK_ICON_AWAY, G_CALLBACK(show_custom_status_editor_cb), NULL, 0, 0, NULL);
	gaim_new_item_from_stock(submenu, _("Saved Status..."), GAIM_STOCK_ICON_AWAY, G_CALLBACK(gaim_gtk_status_window_show), NULL, 0, 0, NULL);

	return menuitem;
}

static void
docklet_menu() {
	static GtkWidget *menu = NULL;
	GtkWidget *menuitem;

	if (menu) {
		gtk_widget_destroy(menu);
	}

	menu = gtk_menu_new();

	menuitem = gtk_check_menu_item_new_with_label(_("Show Buddy List"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), gaim_prefs_get_bool("/gaim/gtk/blist/list_visible"));
	g_signal_connect(G_OBJECT(menuitem), "toggled", G_CALLBACK(docklet_toggle_blist), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = gtk_menu_item_new_with_label(_("Unread Messages"));

	if (status == DOCKLET_STATUS_ONLINE_PENDING || status == DOCKLET_STATUS_AWAY_PENDING) {
		GtkWidget *submenu = gtk_menu_new();
		GList *l = get_pending_list(0);
		if (l == NULL) {
			gtk_widget_set_sensitive(menuitem, FALSE);
			gaim_debug_warning("docklet",
				"status indicates messages pending, but no conversations with unseen messages were found.");
		} else {
			gaim_gtk_conversations_fill_menu(submenu, l);
			g_list_free(l);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
		}
	} else {
		gtk_widget_set_sensitive(menuitem, FALSE);
	}
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	gaim_separator(menu);

	menuitem = gaim_new_item_from_stock(menu, _("New Message..."), GAIM_STOCK_IM, G_CALLBACK(gaim_gtkdialogs_im), NULL, 0, 0, NULL);
	if (status == DOCKLET_STATUS_OFFLINE)
		gtk_widget_set_sensitive(menuitem, FALSE);

	menuitem = docklet_status_submenu();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	gaim_separator(menu);

	gaim_new_item_from_stock(menu, _("Accounts"), GAIM_STOCK_ACCOUNTS, G_CALLBACK(gaim_gtk_accounts_window_show), NULL, 0, 0, NULL);
	gaim_new_item_from_stock(menu, _("Plugins"), GAIM_STOCK_PLUGIN, G_CALLBACK(gaim_gtk_plugin_dialog_show), NULL, 0, 0, NULL);
	gaim_new_item_from_stock(menu, _("Preferences"), GTK_STOCK_PREFERENCES, G_CALLBACK(gaim_gtk_prefs_show), NULL, 0, 0, NULL);

	gaim_separator(menu);

	menuitem = gtk_check_menu_item_new_with_label(_("Mute Sounds"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), gaim_prefs_get_bool("/gaim/gtk/sound/mute"));
	if (!strcmp(gaim_prefs_get_string("/gaim/gtk/sound/method"), "none"))
		gtk_widget_set_sensitive(GTK_WIDGET(menuitem), FALSE);
	g_signal_connect(G_OBJECT(menuitem), "toggled", G_CALLBACK(docklet_toggle_mute), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	gaim_separator(menu);

	/* TODO: need a submenu to change status, this needs to "link" 
	 * to the status in the buddy list gtkstatusbox
	 */

	gaim_new_item_from_stock(menu, _("Quit"), GTK_STOCK_QUIT, G_CALLBACK(gaim_core_quit), NULL, 0, 0, NULL);

#ifdef _WIN32
	g_signal_connect(menu, "leave-notify-event", G_CALLBACK(docklet_menu_leave_enter), NULL);
	g_signal_connect(menu, "enter-notify-event", G_CALLBACK(docklet_menu_leave_enter), NULL);
#endif
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
				   ui_ops->position_menu,
				   NULL, 0, gtk_get_current_event_time());
}

/**************************************************************************
 * public api for ui_ops
 **************************************************************************/
void
docklet_clicked(int button_type)
{
	switch (button_type) {
		case 1:
			if (status == DOCKLET_STATUS_ONLINE_PENDING || status == DOCKLET_STATUS_AWAY_PENDING) {
				GList *l = get_pending_list(1);
				if (l != NULL) {
					gaim_gtkconv_present_conversation((GaimConversation *)l->data);
					g_list_free(l);
				}
			} else {
				gaim_gtk_blist_toggle_visibility();
			}
			break;
		case 3:
			docklet_menu();
			break;
	}
}

void
docklet_embedded()
{
	if (!visibility_manager) {
		gaim_gtk_blist_visibility_manager_add();
		visibility_manager = TRUE;
	}
	docklet_update_status();
	if (ui_ops && ui_ops->update_icon)
		ui_ops->update_icon(status);
}

void
docklet_remove()
{
	if (visibility_manager) {
		gaim_gtk_blist_visibility_manager_remove();
		visibility_manager = FALSE;
	}
}

void
docklet_unload()
{
	gaim_plugin_unload(handle);
}

void
docklet_set_ui_ops(struct docklet_ui_ops *ops)
{
	ui_ops = ops;
}

/**************************************************************************
 * plugin glue
 **************************************************************************/
static gboolean
plugin_load(GaimPlugin *plugin)
{
	void *conn_handle = gaim_connections_get_handle();
	void *conv_handle = gaim_conversations_get_handle();
	void *accounts_handle = gaim_accounts_get_handle();
	void *core_handle = gaim_get_core();

	gaim_debug(GAIM_DEBUG_INFO, "docklet", "plugin loaded\n");

	handle = plugin;

	docklet_ui_init();
	if (ui_ops && ui_ops->create)
		ui_ops->create();
	gaim_signal_connect(conn_handle, "signed-on",
						plugin, GAIM_CALLBACK(docklet_signed_on_cb), NULL);
	gaim_signal_connect(conn_handle, "signed-off",
						plugin, GAIM_CALLBACK(docklet_signed_off_cb), NULL);
	gaim_signal_connect(accounts_handle, "account-status-changed",
						plugin, GAIM_CALLBACK(docklet_update_status_cb), NULL);
	gaim_signal_connect(conv_handle, "received-im-msg",
						plugin, GAIM_CALLBACK(docklet_update_status_cb), NULL);
	gaim_signal_connect(conv_handle, "conversation-created",
						plugin, GAIM_CALLBACK(docklet_update_status_cb), NULL);
	gaim_signal_connect(conv_handle, "deleting-conversation",
						plugin, GAIM_CALLBACK(docklet_update_status_cb), NULL);
	gaim_signal_connect(conv_handle, "conversation-updated",
						plugin, GAIM_CALLBACK(docklet_conv_updated_cb), NULL);

	gaim_signal_connect(core_handle, "quitting",
						plugin, GAIM_CALLBACK(gaim_quit_cb), NULL);

	gaim_prefs_connect_callback(plugin, "/plugins/gtk/docklet/blink_im",
								docklet_prefs_cb, NULL);
	gaim_prefs_connect_callback(plugin, "/plugins/gtk/docklet/blink_chat",
								docklet_prefs_cb, NULL);

	enable_join_chat = online_account_supports_chat();

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	if (ui_ops && ui_ops->destroy)
		ui_ops->destroy();

	/* remove callbacks */
	gaim_signals_disconnect_by_handle(handle);
	gaim_prefs_disconnect_by_handle(handle);

	gaim_debug(GAIM_DEBUG_INFO, "docklet", "plugin unloaded\n");

	return TRUE;
}

static GtkWidget *
plugin_config_frame(GaimPlugin *plugin)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkSizeGroup *sg;
	GtkWidget *dd;

	frame = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 12);

	vbox = gaim_gtk_make_frame(frame, _("Blink tray icon for unread..."));
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	dd = gaim_gtk_prefs_dropdown(vbox, _("_Instant Messages:"),
							GAIM_PREF_STRING, "/plugins/gtk/docklet/blink_im",
							_("Never"), "never",
							_("In hidden conversations"), "hidden",
							_("Always"), "always",
							NULL);
	gtk_size_group_add_widget(sg, dd);

	dd = gaim_gtk_prefs_dropdown(vbox, _("C_hat Messages:"),
							GAIM_PREF_STRING, "/plugins/gtk/docklet/blink_chat",
							_("Never"), "never",
							_("When my nick is said"), "nick",
							_("Always"), "always",
							NULL);
	gtk_size_group_add_widget(sg, dd);

	gtk_widget_show_all(frame);
	return frame;
}

static GaimGtkPluginUiInfo ui_info =
{
	plugin_config_frame,
	0 /* page_num (Reserved) */
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	GAIM_GTK_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	DOCKLET_PLUGIN_ID,                                /**< id             */
	N_("System Tray Icon"),                           /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Displays an icon for Gaim in the system tray."),
	                                                  /**  description    */
	N_("Displays a system tray icon (in GNOME, KDE, or Windows for example) "
	   "to show the current status of Gaim, allow fast access to commonly "
	   "used functions, and to toggle display of the buddy list. "
	   "Also provides options to blink for unread messages."),
	                                                  /** author          */
	"Robert McQueen <robot101@debian.org>"
	"\n\t\t\tCasey Harkins <charkins@users.sf.net>",
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL
};

static void
plugin_init(GaimPlugin *plugin)
{
	gaim_prefs_add_none("/plugins/gtk/docklet");
	gaim_prefs_add_string("/plugins/gtk/docklet/blink_im", "hidden");
	gaim_prefs_add_string("/plugins/gtk/docklet/blink_chat", "never");
}

GAIM_INIT_PLUGIN(docklet, plugin_init, info)
