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
 * ----------------
 * The Plug-in plug
 *
 * Plugin support is currently being maintained by Mike Saraf
 * msaraf@dwc.edu
 *
 * Well, I didn't see any work done on it for a while, so I'm going to try
 * my hand at it. - Eric warmenhoven@yahoo.com
 *
 */

#ifdef GAIM_PLUGINS

#include <string.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "gaim.h"

#include <dlfcn.h>

/* ------------------ Local Variables -------------------------*/

static GtkWidget *plugin_dialog = NULL;
static GList     *plugins = NULL;

static GtkWidget *pluglist;
static GtkWidget *plugtext;

/* --------------- Function Declarations -------------------- */

       void load_plugin  (GtkWidget *, gpointer);
       void unload_plugin(GtkWidget *, gpointer);
       void show_plugins (GtkWidget *, gpointer);

static void destroy_plugins  (GtkWidget *, gpointer);
static void load_which_plugin(GtkWidget *, gpointer);
static void unload           (GtkWidget *, gpointer);
static void list_clicked     (GtkWidget *, struct gaim_plugin *);

/* ------------------ Code Below ---------------------------- */

static void destroy_plugins(GtkWidget *w, gpointer data) {
	if (plugin_dialog)
		gtk_widget_destroy(plugin_dialog);
	plugin_dialog = NULL;
}

void load_plugin(GtkWidget *w, gpointer data)
{
	char *buf = g_malloc(BUF_LEN);
 
	if (!plugin_dialog) {
		plugin_dialog = gtk_file_selection_new("Gaim - Plugin List");

		gtk_file_selection_hide_fileop_buttons(
					GTK_FILE_SELECTION(plugin_dialog));

	if(getenv("PLUGIN_DIR") == NULL) {
		g_snprintf(buf, BUF_LEN - 1, "%s/%s", getenv("HOME"), PLUGIN_DIR);
	} else {
		g_snprintf(buf, BUF_LEN - 1, "%s/", getenv("PLUGIN_DIR"));
	}

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(plugin_dialog), buf);
	gtk_signal_connect(GTK_OBJECT(plugin_dialog), "destroy",
			GTK_SIGNAL_FUNC(destroy_plugins), plugin_dialog);

	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(plugin_dialog)->ok_button),
			"clicked", GTK_SIGNAL_FUNC(load_which_plugin), NULL);
    
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(plugin_dialog)->cancel_button),
			"clicked", GTK_SIGNAL_FUNC(destroy_plugins), NULL);

	}

	g_free(buf);
	gtk_widget_show(plugin_dialog);
	gdk_window_raise(plugin_dialog->window);   
}

void load_which_plugin(GtkWidget *w, gpointer data) {
	struct gaim_plugin *plug;
	void (*gaim_plugin_init)();
	char *(*cfunc)();
	int (*nfunc)();
	char *error;

	plug = g_malloc(sizeof *plug);
	plug->filename = gtk_file_selection_get_filename(
					GTK_FILE_SELECTION(plugin_dialog));
	/* do NOT OR with RTLD_GLOBAL, otherwise plugins may conflict
	 * (it's really just a way to work around other people's bad
	 * programming, by not using RTLD_GLOBAL :P ) */
	plug->handle = dlopen(plug->filename, RTLD_LAZY);
	if (!plug->handle) {
		error = dlerror();
		do_error_dialog(error, "Plugin Error");
		g_free(plug);
		return;
	}

	if (plugin_dialog)
		gtk_widget_destroy(plugin_dialog);
	plugin_dialog = NULL;

	gaim_plugin_init = dlsym(plug->handle, "gaim_plugin_init");
	if ((error = dlerror()) != NULL) {
		do_error_dialog(error, "Plugin Error");
		dlclose(plug->handle);
		g_free(plug);
		return;
	}

	plugins = g_list_append(plugins, plug);
	(*gaim_plugin_init)();

	cfunc = dlsym(plug->handle, "name");
	if ((error = dlerror()) == NULL)
		plug->name = (*cfunc)();
	else
		plug->name = NULL;

	cfunc = dlsym(plug->handle, "description");
	if ((error = dlerror()) == NULL)
		plug->description = (*cfunc)();
	else
		plug->description = NULL;
}

void unload_plugin(GtkWidget *w, gpointer data) {
	/* FIXME */
}

void show_plugins(GtkWidget *w, gpointer data) {
	/* most of this code was shamelessly stolen from prefs.c */
	GtkWidget *window;
	GtkWidget *page;
	GtkWidget *topbox;
	GtkWidget *botbox;
	GtkWidget *sw;
	GtkWidget *label;
	GtkWidget *list_item;
	GtkWidget *sw2;
	GtkWidget *add;
	GtkWidget *remove;
	GList     *plugs = plugins;
	struct gaim_plugin *p;
	gchar buffer[1024];

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_widget_realize(window);
	aol_icon(window->window);
	gtk_container_border_width(GTK_CONTAINER(window), 10);
	gtk_window_set_title(GTK_WINDOW(window), "Gaim - Plugins");

	page = gtk_vbox_new(FALSE, 0);
	topbox = gtk_hbox_new(FALSE, 0);
	botbox = gtk_hbox_new(FALSE, 0);

	sw2 = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw2),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	pluglist = gtk_list_new();
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw2), pluglist);
	gtk_box_pack_start(GTK_BOX(topbox), sw2, TRUE, TRUE, 0);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	plugtext = gtk_text_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(sw), plugtext);
	gtk_box_pack_start(GTK_BOX(topbox), sw, TRUE, TRUE, 0);
	gtk_text_set_word_wrap(GTK_TEXT(plugtext), TRUE);
	gtk_text_set_editable(GTK_TEXT(plugtext), FALSE);

	add = gtk_button_new_with_label("Load Plugin");
	gtk_signal_connect(GTK_OBJECT(add), "clicked",
			   GTK_SIGNAL_FUNC(load_plugin), NULL);
	gtk_box_pack_start(GTK_BOX(botbox), add, TRUE, FALSE, 5);

	remove = gtk_button_new_with_label("Unload Plugin");
	gtk_signal_connect(GTK_OBJECT(remove), "clicked",
			   GTK_SIGNAL_FUNC(unload), pluglist);
	gtk_box_pack_start(GTK_BOX(botbox), remove, TRUE, FALSE, 5);

	gtk_box_pack_start(GTK_BOX(page), topbox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(page), botbox, FALSE, FALSE, 0);

	if (plugs != NULL) {
		p = (struct gaim_plugin *)plugs->data;
		g_snprintf(buffer, sizeof(buffer), "%s", p->filename);
		gtk_text_insert(GTK_TEXT(plugtext), NULL, NULL, NULL, buffer, -1);
	}

	while (plugs) {
		p = (struct gaim_plugin *)plugs->data;
		label = gtk_label_new(p->filename);
		list_item = gtk_list_item_new();
		gtk_container_add(GTK_CONTAINER(list_item), label);
		gtk_signal_connect(GTK_OBJECT(list_item), "select",
				   GTK_SIGNAL_FUNC(list_clicked), p);
		gtk_object_set_user_data(GTK_OBJECT(list_item), p);

		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(pluglist), list_item);
		gtk_widget_show(list_item);

		plugs = plugs->next;
	}

	gtk_widget_show(page);
	gtk_widget_show(topbox);
	gtk_widget_show(botbox);
	gtk_widget_show(sw);
	gtk_widget_show(sw2);
	gtk_widget_show(pluglist);
	gtk_widget_show(plugtext);
	gtk_widget_show(add);
	gtk_widget_show(remove);

	gtk_container_add(GTK_CONTAINER(window), page);
	gtk_widget_show(window);
}

void unload(GtkWidget *w, gpointer data) {
	GList *i;
	struct gaim_plugin *p;
	void (*gaim_plugin_remove)();
	char *error;

	i = GTK_LIST(pluglist)->selection;

	p = gtk_object_get_user_data(GTK_OBJECT(i->data));

	g_list_remove(plugins, p);

	gaim_plugin_remove = dlsym(p->handle, "gaim_plugin_remove");
	if ((error = dlerror()) == NULL)
		(*gaim_plugin_remove)();
	dlclose(p->handle);
	g_free(p);
}

void list_clicked(GtkWidget *w, struct gaim_plugin *p) {
	gchar buffer[2048];
	guint text_len;

	text_len = gtk_text_get_length(GTK_TEXT(plugtext));
	gtk_text_set_point(GTK_TEXT(plugtext), 0);
	gtk_text_forward_delete(GTK_TEXT(plugtext), text_len);

	g_snprintf(buffer, sizeof buffer, "%s\n%s", p->name, p->description);
	gtk_text_insert(GTK_TEXT(plugtext), NULL, NULL, NULL, buffer, -1);
}

#endif /* GAIM_PLUGINS */
