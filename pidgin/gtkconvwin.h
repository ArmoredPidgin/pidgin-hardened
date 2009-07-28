/**
 * @file gtkconvwin.h GTK+ Conversation Window API
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#ifndef _PIDGIN_CONVERSATION_WINDOW_H_
#define _PIDGIN_CONVERSATION_WINDOW_H_

typedef struct _PidginWindow       PidginWindow;


/**************************************************************************
 * @name Structures
 **************************************************************************/
/*@{*/

/**
 * A GTK+ representation of a graphical window containing one or more
 * conversations.
 */
struct _PidginWindow
{
	GtkWidget *window;           /**< The window.                      */
	GtkWidget *notebook;         /**< The notebook of conversations.   */
	GList *gtkconvs;

	struct
	{
/* Some necessary functions were only added in 2.6.0 */
		GtkWidget *menubar;

#if GTK_CHECK_VERSION(2,6,0)
		GtkAction *view_log;

		GtkAction *send_file;
		GtkAction *add_pounce;
		GtkAction *get_info;
		GtkAction *invite;

		GtkAction *alias;
		GtkAction *block;
		GtkAction *unblock;
		GtkAction *add;
		GtkAction *remove;

		GtkAction *insert_link;
		GtkAction *insert_image;

		GtkAction *logging;
		GtkAction *sounds;
		GtkAction *show_formatting_toolbar;
		GtkAction *show_timestamps;
#else
		GtkWidget *view_log;

		GtkWidget *send_file;
		GtkWidget *add_pounce;
		GtkWidget *get_info;
		GtkWidget *invite;

		GtkWidget *alias;
		GtkWidget *block;
		GtkWidget *unblock;
		GtkWidget *add;
		GtkWidget *remove;

		GtkWidget *insert_link;
		GtkWidget *insert_image;

		GtkWidget *logging;
		GtkWidget *sounds;
		GtkWidget *show_formatting_toolbar;
		GtkWidget *show_timestamps;
#endif
		GtkWidget *show_icon;

		GtkWidget *send_to;

		GtkWidget *tray;

		GtkWidget *typing_icon;

#if GTK_CHECK_VERSION(2,6,0)
		GtkUIManager *ui;
#else
		GtkItemFactory *item_factory;
#endif

	} menu;

	struct
	{
		GtkWidget *search;

	} dialogs;

	/* Tab dragging stuff. */
	gboolean in_drag;
	gboolean in_predrag;

	gint drag_tab;

	gint drag_min_x, drag_max_x, drag_min_y, drag_max_y;

	gint drag_motion_signal;
	gint drag_leave_signal;

	/* Media menu options. */
#if GTK_CHECK_VERSION(2,6,0)
	GtkAction *audio_call;
	GtkAction *video_call;
	GtkAction *audio_video_call;
#else
	GtkWidget *audio_call;
	GtkWidget *video_call;
	GtkWidget *audio_video_call;
#endif
};

/*@}*/

/**************************************************************************
 * @name GTK+ Conversation Window API
 **************************************************************************/
/*@{*/

PidginWindow * pidgin_conv_window_new(void);
void pidgin_conv_window_destroy(PidginWindow *win);
GList *pidgin_conv_windows_get_list(void);
void pidgin_conv_window_show(PidginWindow *win);
void pidgin_conv_window_hide(PidginWindow *win);
void pidgin_conv_window_raise(PidginWindow *win);
void pidgin_conv_window_switch_gtkconv(PidginWindow *win, PidginConversation *gtkconv);
void pidgin_conv_window_add_gtkconv(PidginWindow *win, PidginConversation *gtkconv);
void pidgin_conv_window_remove_gtkconv(PidginWindow *win, PidginConversation *gtkconv);
PidginConversation *pidgin_conv_window_get_gtkconv_at_index(const PidginWindow *win, int index);
PidginConversation *pidgin_conv_window_get_active_gtkconv(const PidginWindow *win);
PurpleConversation *pidgin_conv_window_get_active_conversation(const PidginWindow *win);
gboolean pidgin_conv_window_is_active_conversation(const PurpleConversation *conv);
gboolean pidgin_conv_window_has_focus(PidginWindow *win);
PidginWindow *pidgin_conv_window_get_at_xy(int x, int y);
GList *pidgin_conv_window_get_gtkconvs(PidginWindow *win);
guint pidgin_conv_window_get_gtkconv_count(PidginWindow *win);

PidginWindow *pidgin_conv_window_first_with_type(PurpleConversationType type);
PidginWindow *pidgin_conv_window_last_with_type(PurpleConversationType type);

/*@}*/

/**************************************************************************
 * @name GTK+ Conversation Placement API
 **************************************************************************/
/*@{*/

typedef void (*PidginConvPlacementFunc)(PidginConversation *);

GList *pidgin_conv_placement_get_options(void);
void pidgin_conv_placement_add_fnc(const char *id, const char *name, PidginConvPlacementFunc fnc);
void pidgin_conv_placement_remove_fnc(const char *id);
const char *pidgin_conv_placement_get_name(const char *id);
PidginConvPlacementFunc pidgin_conv_placement_get_fnc(const char *id);
void pidgin_conv_placement_set_current_func(PidginConvPlacementFunc func);
PidginConvPlacementFunc pidgin_conv_placement_get_current_func(void);
void pidgin_conv_placement_place(PidginConversation *gtkconv);

/*@}*/

#endif /* _PIDGIN_CONVERSATION_WINDOW_H_ */
