/**
 * @file gtkaccount.c Account Editor dialog
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2002-2003, Christian Hammond <chipx86@gnupdate.org>
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
 */
#include "gtkaccount.h"
#include "account.h"
#include "prefs.h"
#include "stock.h"
#include "gtkblist.h"

enum
{
	COLUMN_ICON,
	COLUMN_SCREENNAME,
	COLUMN_ONLINE,
	COLUMN_AUTOLOGIN,
	COLUMN_PROTOCOL,
	COLUMN_DATA,
	NUM_COLUMNS
};

typedef struct
{
	GtkWidget *window;

	GtkListStore *model;

} AccountsDialog;

static AccountsDialog *accounts_dialog = NULL;

static char *
proto_name(int proto)
{
	GaimPlugin *p = gaim_find_prpl(proto);

	return ((p && p->info->name) ? _(p->info->name) : _("Unknown"));
}

static gint
__window_destroy_cb(GtkWidget *w, GdkEvent *event, void *unused)
{
	g_free(accounts_dialog);
	accounts_dialog = NULL;

	return FALSE;
}

static gboolean
__configure_cb(GtkWidget *w, GdkEventConfigure *event, AccountsDialog *dialog)
{
	if (GTK_WIDGET_VISIBLE(w)) {
		gaim_prefs_set_int("/gaim/gtk/accounts/dialog/width",  event->width);
		gaim_prefs_set_int("/gaim/gtk/accounts/dialog/height", event->height);
	}

	return FALSE;
}

static void
__add_account_cb(GtkWidget *w, AccountsDialog *dialog)
{

}

static void
__modify_account_cb(GtkWidget *w, AccountsDialog *dialog)
{

}

static void
__delete_account_cb(GtkWidget *w, AccountsDialog *dialog)
{

}

static void
__close_accounts_cb(GtkWidget *w, AccountsDialog *dialog)
{

}

static void
__online_cb(GtkCellRendererToggle *renderer, gchar *path_str, gpointer data)
{
	
}

static void
__autologin_cb(GtkCellRendererToggle *renderer, gchar *path_str,
			   gpointer data)
{
	
}

static void
__add_columns(GtkWidget *treeview, AccountsDialog *dialog)
{
	GtkCellRenderer *renderer;

	/* Protocol Icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
												-1, "",
												renderer,
												"pixbuf", COLUMN_ICON,
												NULL);

	/* Screennames */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
												-1, _("Screenname"),
												renderer,
												"text", COLUMN_SCREENNAME,
												NULL);

	/* Online? */
	renderer = gtk_cell_renderer_toggle_new();
	
	g_signal_connect(G_OBJECT(renderer), "toggled",
					 G_CALLBACK(__online_cb), dialog);

	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
												-1, _("Online"),
												renderer,
												"text", COLUMN_ONLINE,
												NULL);


	/* Auto-login? */
	renderer = gtk_cell_renderer_toggle_new();

	g_signal_connect(G_OBJECT(renderer), "toggled",
					 G_CALLBACK(__autologin_cb), dialog);

	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
												-1, _("Auto-login"),
												renderer,
												"text", COLUMN_AUTOLOGIN,
												NULL);


	/* Protocol description */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
												-1, _("Protocol"),
												renderer,
												"text", COLUMN_PROTOCOL,
												NULL);
}

static void
__populate_accounts_list(AccountsDialog *dialog)
{
	GList *l;
	GaimAccount *account;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scale;

	gtk_list_store_clear(dialog->model);

	for (l = gaim_accounts_get_all(); l != NULL; l = l->next) {
		account = l->data;

		pixbuf = create_prpl_icon(account);
		scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);

		gtk_list_store_append(dialog->model, &iter);
		gtk_list_store_set(dialog->model, &iter,
				COLUMN_ICON, scale,
				COLUMN_SCREENNAME, gaim_account_get_username(account),
				COLUMN_ONLINE, gaim_account_is_connected(account),
				COLUMN_AUTOLOGIN, FALSE,
				COLUMN_PROTOCOL, proto_name(gaim_account_get_protocol(account)),
				COLUMN_DATA, account,
				-1);

		g_object_unref(G_OBJECT(pixbuf));
		g_object_unref(G_OBJECT(scale));
	}
}

static GtkWidget *
__create_accounts_list(AccountsDialog *dialog)
{
	GtkWidget *sw;
	GtkWidget *treeview;

	/* Create the scrolled window. */
	sw = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_AUTOMATIC,
								   GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
										GTK_SHADOW_IN);
	gtk_widget_show(sw);

	/* Create the list model. */
	dialog->model = gtk_list_store_new(NUM_COLUMNS, G_TYPE_POINTER,
									   G_TYPE_STRING, G_TYPE_BOOLEAN,
									   G_TYPE_BOOLEAN, G_TYPE_STRING,
									   G_TYPE_POINTER);

	/* And now the actual treeview */
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_selection_set_mode(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)),
			GTK_SELECTION_MULTIPLE);

	__add_columns(treeview, dialog);

	gtk_container_add(GTK_CONTAINER(sw), treeview);
	gtk_widget_show(treeview);

	__populate_accounts_list(dialog);

	return sw;
}

void
gaim_gtk_account_dialog_show(void)
{
	AccountsDialog *dialog;
	GtkWidget *win;
	GtkWidget *vbox;
	GtkWidget *bbox;
	GtkWidget *sw;
	GtkWidget *sep;
	GtkWidget *button;
	int width, height;

	if (accounts_dialog != NULL)
		return;

	accounts_dialog = dialog = g_new0(AccountsDialog, 1);

	width  = gaim_prefs_get_int("/gaim/gtk/accounts/dialog/width");
	height = gaim_prefs_get_int("/gaim/gtk/accounts/dialog/height");

	win = dialog->window;

	GAIM_DIALOG(win);
	gtk_window_set_default_size(GTK_WINDOW(win), width, height);
	gtk_window_set_role(GTK_WINDOW(win), "accounts");
	gtk_window_set_title(GTK_WINDOW(win), "Accounts");
	gtk_container_set_border_width(GTK_CONTAINER(win), 12);

	g_signal_connect(G_OBJECT(win), "delete_event",
					 G_CALLBACK(__window_destroy_cb), accounts_dialog);
	g_signal_connect(G_OBJECT(win), "configure_event",
					 G_CALLBACK(__configure_cb), accounts_dialog);

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(win), vbox);
	gtk_widget_show(vbox);

	/* Setup the scrolled window that will contain the list of accounts. */
	sw = __create_accounts_list(dialog);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	/* Separator... */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	/* Button box. */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), 6);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	/* Add button */
	button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(__add_account_cb), dialog);

	/* Modify button */
	button = gtk_button_new_from_stock(GAIM_STOCK_MODIFY);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(__modify_account_cb), dialog);

	/* Delete button */
	button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(__delete_account_cb), dialog);

	/* Close button */
	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(__close_accounts_cb), dialog);

	gtk_widget_show(win);
}

