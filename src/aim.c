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
#ifdef GAIM_PLUGINS
#ifndef _WIN32
#include <dlfcn.h>
#endif
#endif /* GAIM_PLUGINS */
#include <gtk/gtk.h>
#ifndef _WIN32
#include <gdk/gdkx.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/wait.h>
#endif /* !_WIN32 */
#include <gdk/gdk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include "prpl.h"
#include "gaim.h"
#include "gaim-socket.h"
#if HAVE_SIGNAL_H
#include <signal.h>
#endif
#include "locale.h"
#include <getopt.h>

#ifdef _WIN32
#include "win32dep.h"
#endif

static GtkWidget *name;
static GtkWidget *pass;

GList *log_conversations = NULL;
GList *buddy_pounces = NULL;
GSList *away_messages = NULL;
GList *conversations = NULL;
GSList *message_queue = NULL;
GSList *unread_message_queue = NULL;
GSList *away_time_queue = NULL;

GtkWidget *mainwindow = NULL;

int opt_away = 0;
char *opt_away_arg = NULL;
char *opt_rcfile_arg = NULL;
int opt_debug = 0;
#ifdef _WIN32
int opt_gdebug = 0;
#endif

#if HAVE_SIGNAL_H
/*
 * Lists of signals we wish to catch and those we wish to ignore.
 * Each list terminated with -1
 */
static int catch_sig_list[] = {
	SIGSEGV,
	SIGHUP,
	SIGINT,
	SIGTERM,
	SIGQUIT,
	SIGCHLD,
	-1
};

static int ignore_sig_list[] = {
	SIGPIPE,
	-1
};
#endif

void do_quit()
{
	/* captain's log, stardate... */
	system_log(log_quit, NULL, NULL, OPT_LOG_BUDDY_SIGNON | OPT_LOG_MY_SIGNON);

	/* the self destruct sequence has been initiated */
	plugin_event(event_quit);

	/* transmission ends */
	signoff_all();

	/* record what we have before we blow it away... */
	save_prefs();

#ifdef GAIM_PLUGINS
	/* jettison cargo */
	remove_all_plugins();
#endif

#ifdef USE_PERL
	/* yup, perl too */
	perl_end();
#endif

	/* and end it all... */
	gtk_main_quit();
}

static int snd_tmout;
int logins_not_muted = 1;
static void sound_timeout()
{
	logins_not_muted = 1;
	gtk_timeout_remove(snd_tmout);
}

/* we need to do this for Oscar because serv_login only starts the login
 * process, it doesn't end there. gaim_setup will be called later from
 * oscar.c, after the buddy list is made and serv_finish_login is called */
void gaim_setup(struct gaim_connection *gc)
{
	if ((sound_options & OPT_SOUND_LOGIN) && (sound_options & OPT_SOUND_SILENT_SIGNON)) {
		logins_not_muted = 0;
		snd_tmout = gtk_timeout_add(10000, (GtkFunction)sound_timeout, NULL);
	}
}


static void dologin(GtkWidget *widget, GtkWidget *w)
{
	struct aim_user *u;
	const char *username = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(name)->entry));
	const char *password = gtk_entry_get_text(GTK_ENTRY(pass));

	if (!strlen(username)) {
		do_error_dialog(_("Please enter your login."), NULL, GAIM_ERROR);
		return;
	}

	/* if there is more than one user of the same name, then fuck 
	 * them, they just have to use the account editor to sign in 
	 * the second one */

	u = find_user(username, -1);
	if (!u)
		u = new_user(username, DEFAULT_PROTO, OPT_USR_REM_PASS);
	g_snprintf(u->password, sizeof u->password, "%s", password);
	save_prefs();
	serv_login(u);
}

/* <name> is a comma-separated list of names, or NULL
   if NULL and there is at least one user defined in .gaimrc, try to login.
   if not NULL, parse <name> into separate strings, look up each one in 
   .gaimrc and, if it's there, try to login.
   returns:  0 if successful
            -1 if no user was found that had a saved password
*/
static int dologin_named(char *name)
{
	struct aim_user *u;
	char **names, **n;
	int retval = -1;

	if (name !=NULL) {	/* list of names given */
		names = g_strsplit(name, ",", 32);
		for (n = names; *n != NULL; n++) {
			u = find_user(*n, -1);
			if (u) {	/* found a user */
				if (u->options & OPT_USR_REM_PASS) {
					retval = 0;
					serv_login(u);
				}
			}
		}
		g_strfreev(names);
	} else {		/* no name given, use default */
		u = (struct aim_user *)aim_users->data;
		if (u->options & OPT_USR_REM_PASS) {
			retval = 0;
			serv_login(u);
		}
	}

	return retval;
}


static void doenter(GtkWidget *widget, GtkWidget *w)
{
	if (widget == name) {
		gtk_entry_set_text(GTK_ENTRY(pass), "");
		gtk_entry_select_region(GTK_ENTRY(GTK_COMBO(name)->entry), 0, 0);
		gtk_widget_grab_focus(pass);
	} else if (widget == pass) {
		dologin(widget, w);
	}
}


static void combo_changed(GtkWidget *w, GtkWidget *combo)
{
	const char *txt = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry));
	struct aim_user *u;

	u = find_user(txt, -1);

	if (u && u->options & OPT_USR_REM_PASS) {
		gtk_entry_set_text(GTK_ENTRY(pass), u->password);
	} else {
		gtk_entry_set_text(GTK_ENTRY(pass), "");
	}
}


static GList *combo_user_names()
{
	GSList *usr = aim_users;
	GList *tmp = NULL;
	struct aim_user *u;

	if (!usr)
		return g_list_append(NULL, "<New User>");

	while (usr) {
		u = (struct aim_user *)usr->data;
		tmp = g_list_append(tmp, u->username);
		usr = usr->next;
	}

	return tmp;
}

void show_login()
{
	GdkPixbuf *icon;
	GtkWidget *image;
	GtkWidget *vbox;
	GtkWidget *button;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *vbox2;
	GList *tmp;

	/* Do we already have a main window opened? If so, bring it back, baby... ribs... yeah */
	if (mainwindow) {
			gtk_window_present(GTK_WINDOW(mainwindow));
			return;
	}

	mainwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_role(GTK_WINDOW(mainwindow), "login");
	gtk_window_set_policy(GTK_WINDOW(mainwindow), FALSE, FALSE, TRUE);
	gtk_window_set_title(GTK_WINDOW(mainwindow), _("Gaim - Login"));
	gtk_widget_realize(mainwindow);
	gdk_window_set_group(mainwindow->window, mainwindow->window);
	gtk_container_set_border_width(GTK_CONTAINER(mainwindow), 5);
	gtk_signal_connect(GTK_OBJECT(mainwindow), "delete_event", 
					GTK_SIGNAL_FUNC(do_quit), mainwindow);


	icon = gaim_pixbuf(NULL, "gaim.png");
	if (icon) {
			gtk_window_set_icon(GTK_WINDOW(mainwindow), icon);
			gdk_pixbuf_unref(icon);
	}

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(mainwindow), vbox);

	image = gaim_pixmap(NULL, "logo.png");
	gtk_box_pack_start(GTK_BOX(vbox), image, FALSE, FALSE, 0);

	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 5);

	label = gtk_label_new(_("Screen Name:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);

	name = gtk_combo_new();
	tmp = combo_user_names();
	gtk_combo_set_popdown_strings(GTK_COMBO(name), tmp);
	g_list_free(tmp);
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(name)->entry), "activate",
			   GTK_SIGNAL_FUNC(doenter), mainwindow);
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(name)->entry), "changed",
			   GTK_SIGNAL_FUNC(combo_changed), name);
	gtk_box_pack_start(GTK_BOX(vbox2), name, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, TRUE, 0);

	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 5);

	label = gtk_label_new(_("Password:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);

	pass = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(pass), FALSE);
	gtk_signal_connect(GTK_OBJECT(pass), "activate", GTK_SIGNAL_FUNC(doenter), mainwindow);
	gtk_box_pack_start(GTK_BOX(vbox2), pass, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, TRUE, 0);

	/* Now for the button box */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 5);

	/* And now for the buttons */
	button = gaim_pixbuf_button("Accounts", "accounts.png", GAIM_BUTTON_VERTICAL);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(account_editor), mainwindow);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

#ifdef NO_MULTI
	gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
#endif

	button = gaim_pixbuf_button("Settings", "preferences.png", GAIM_BUTTON_VERTICAL);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(show_prefs), mainwindow);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	button = gaim_pixbuf_button("Sign On", "signon.png", GAIM_BUTTON_VERTICAL);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(dologin), mainwindow);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

#ifdef _WIN32
	/* Register newly created window with systray module */
	wgaim_created_loginwin(GTK_WIDGET(mainwindow));
#endif

	/* Now grab the focus that we need */
	if (aim_users) {
		struct aim_user *c = (struct aim_user *)aim_users->data;
		if (c->options & OPT_USR_REM_PASS) {
			combo_changed(NULL, name);
			gtk_widget_grab_focus(button);
		} else {
			gtk_widget_grab_focus(pass);
		}
	} else {
		gtk_widget_grab_focus(name);
	}

	/* And raise the curtain! */
	gtk_widget_show_all(mainwindow);

}

#if HAVE_SIGNAL_H
void sighandler(int sig)
{
	switch (sig) {
	case SIGHUP:
		debug_printf("caught signal %d\n", sig);
		signoff_all(NULL, NULL);
		break;
	case SIGSEGV:
		core_quit();
#ifndef DEBUG
		fprintf(stderr, "Gaim has segfaulted and attempted to dump a core file.\n"
			"This is a bug in the software and has happened through\n"
			"no fault of your own.\n\n"
			"It is possible that this bug is already fixed in CVS.\n"
			"You can get a tarball of CVS from the Gaim website, at\n"
			WEBSITE "gaim-CVS.tar.gz.\n\n"
			"If you are already using CVS, or can reproduce the crash\n"
			"using the CVS version, please notify the gaim maintainers\n"
			"by reporting a bug at\n"
			WEBSITE "bug.php\n\n"
			"Please make sure to specify what you were doing at the time,\n"
			"and post the backtrace from the core file. If you do not know\n"
			"how to get the backtrace, please get instructions at\n"
			WEBSITE "gdb.php. If you need further\n"
			"assistance, please IM either RobFlynn or SeanEgn and\n"
			"they can help you.\n");
#else
		fprintf(stderr, "Oh no!  Segmentation fault!\n");
		g_on_error_query (g_get_prgname());
		exit(1);
#endif
		abort();
		break;
	case SIGCHLD:
		clean_pid();
#if HAVE_SIGNAL_H
		signal(SIGCHLD, sighandler);    /* restore signal catching on this one! */
#endif
		break;
	default:
		debug_printf("caught signal %d\n", sig);
		signoff_all(NULL, NULL);
#ifdef GAIM_PLUGINS
		remove_all_plugins();
#endif
		if (gtk_main_level())
			gtk_main_quit();
		core_quit();
		exit(0);
	}
}
#endif

#ifndef _WIN32
static gboolean socket_readable(GIOChannel *source, GIOCondition cond, gpointer ud)
{
	guchar type;
	guchar subtype;
	guint32 len;
	guchar *data;
	guint32 x;

	debug_printf("Core says: ");
	g_io_channel_read(source, &type, sizeof(type), &x);
	if (x == 0) {
		debug_printf("CORE IS GONE!\n");
		g_io_channel_close(source);
		return FALSE;
	}
	debug_printf("%d ", type);
	g_io_channel_read(source, &subtype, sizeof(subtype), &x);
	if (x == 0) {
		debug_printf("CORE IS GONE!\n");
		g_io_channel_close(source);
		return FALSE;
	}
	debug_printf("%d ", subtype);
	g_io_channel_read(source, (guchar *)&len, sizeof(len), &x);
	if (x == 0) {
		debug_printf("CORE IS GONE!\n");
		g_io_channel_close(source);
		return FALSE;
	}
	debug_printf("(%d bytes)\n", len);

	data = g_malloc(len);
	g_io_channel_read(source, data, len, &x);
	if (x != len) {
		debug_printf("CORE IS GONE! (read %d/%d bytes)\n", x, len);
		g_free(data);
		g_io_channel_close(source);
		return FALSE;
	}

	g_free(data);
	return TRUE;
}

static int ui_main()
{
	GIOChannel *channel;
	int UI_fd;
	char name[256];
	GList *icons = NULL;
	GdkPixbuf *icon = NULL;

	/* use the nice PNG icon for all the windows */
	icon = gdk_pixbuf_new_from_file(DATADIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "gaim.png",NULL);
	if (icon) {
		icons = g_list_append(icons,icon);
		gtk_window_set_default_icon_list(icons);
		g_object_unref(G_OBJECT(icon));
	} else {
		debug_printf("Failed to load icon from %s" G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "gaim.png\n",DATADIR);
	}

	g_snprintf(name, sizeof(name), "%s" G_DIR_SEPARATOR_S "gaim_%s.%d", g_get_tmp_dir(), g_get_user_name(), gaim_session);

	UI_fd = gaim_connect_to_session(0);
	if (UI_fd < 0)
		return 1;

	channel = g_io_channel_unix_new(UI_fd);
	g_io_add_watch(channel, G_IO_IN | G_IO_HUP | G_IO_ERR, socket_readable, NULL);
	return 0;
}
#endif /* _WIN32 */

static void set_first_user(char *name)
{
	struct aim_user *u;

	u = find_user(name, -1);

	if (!u) {		/* new user */
		u = g_new0(struct aim_user, 1);
		g_snprintf(u->username, sizeof(u->username), "%s", name);
		u->protocol = DEFAULT_PROTO;
		aim_users = g_slist_prepend(aim_users, u);
	} else {		/* user already exists */
		aim_users = g_slist_remove(aim_users, u);
		aim_users = g_slist_prepend(aim_users, u);
	}
	save_prefs();
}

#ifdef _WIN32
/* WIN32 print and log handlers */

static void gaim_dummy_print( const gchar* string ) {
        return;
}

static void gaim_dummy_log_handler (const gchar    *domain,
				    GLogLevelFlags  flags,
				    const gchar    *msg,
				    gpointer        user_data) {
        return;
}

static void gaim_log_handler (const gchar    *domain,
			      GLogLevelFlags  flags,
			      const gchar    *msg,
			      gpointer        user_data) {
        debug_printf("%s - %s\n", domain, msg);
	g_log_default_handler(domain, flags, msg, user_data);
}
#endif /* _WIN32 */

/* FUCKING GET ME A TOWEL! */
#ifdef _WIN32
int gaim_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	int opt_acct = 0, opt_help = 0, opt_version = 0, opt_login = 0, opt_nologin = 0, dologin_ret = -1;
	char *opt_user_arg = NULL, *opt_login_arg = NULL;
#if HAVE_SIGNAL_H
	int sig_indx;	/* for setting up signal catching */
	sigset_t sigset;
	void (*prev_sig_disp)();
#endif
	int opt, opt_user = 0;
	int i;

	struct option long_options[] = {
		{"acct", no_argument, NULL, 'a'},
		/*{"away", optional_argument, NULL, 'w'}, */
		{"help", no_argument, NULL, 'h'},
		/*{"login", optional_argument, NULL, 'l'}, */
		{"loginwin", no_argument, NULL, 'n'},
		{"user", required_argument, NULL, 'u'},
		{"file", required_argument, NULL, 'f'},
		{"debug", no_argument, NULL, 'd'},
		{"version", no_argument, NULL, 'v'},
		{0, 0, 0, 0}
	};

#ifdef DEBUG
	opt_debug = 1;
#endif

#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);
#endif

#if HAVE_SIGNAL_H
	/* Let's not violate any PLA's!!!! */
	/* jseymour: whatever the fsck that means */
	/* Robot101: for some reason things like gdm like to block     *
	 * useful signals like SIGCHLD, so we unblock all the ones we  *
	 * declare a handler for. thanks JSeymour and Vann.            */
	if (sigemptyset(&sigset)) {
		char errmsg[BUFSIZ];
		sprintf(errmsg, "Warning: couldn't initialise empty signal set");
		perror(errmsg);
	}
	for(sig_indx = 0; catch_sig_list[sig_indx] != -1; ++sig_indx) {
		if((prev_sig_disp = signal(catch_sig_list[sig_indx], sighandler)) == SIG_ERR) {
			char errmsg[BUFSIZ];
			sprintf(errmsg, "Warning: couldn't set signal %d for catching",
				catch_sig_list[sig_indx]);
			perror(errmsg);
		}
		if(sigaddset(&sigset, catch_sig_list[sig_indx])) {
			char errmsg[BUFSIZ];
			sprintf(errmsg, "Warning: couldn't include signal %d for unblocking",
				catch_sig_list[sig_indx]);
			perror(errmsg);
		}
	}
	for(sig_indx = 0; ignore_sig_list[sig_indx] != -1; ++sig_indx) {
		if((prev_sig_disp = signal(ignore_sig_list[sig_indx], SIG_IGN)) == SIG_ERR) {
			char errmsg[BUFSIZ];
			sprintf(errmsg, "Warning: couldn't set signal %d to ignore",
				ignore_sig_list[sig_indx]);
			perror(errmsg);
		}
	}

	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL)) {
		char errmsg[BUFSIZ];
		sprintf(errmsg, "Warning: couldn't unblock signals");
		perror(errmsg);
	}		
#endif

	for (i = 0; i < argc; i++) {
		/* --login option */
		if (strstr(argv[i], "--l") == argv[i]) {
			char *equals;
			opt_login = 1;
			if ((equals = strchr(argv[i], '=')) != NULL) {
				/* --login=NAME */
				opt_login_arg = g_strdup(equals + 1);
				if (strlen(opt_login_arg) == 0) {
					g_free(opt_login_arg);
					opt_login_arg = NULL;
				}
			} else if (i + 1 < argc && argv[i + 1][0] != '-') {
				/* --login NAME */
				opt_login_arg = g_strdup(argv[i + 1]);
				strcpy(argv[i + 1], " ");
			}
			strcpy(argv[i], " ");
		}
		/* -l option */
		else if (strstr(argv[i], "-l") == argv[i]) {
			opt_login = 1;
			if (strlen(argv[i]) > 2) {
				/* -lNAME */
				opt_login_arg = g_strdup(argv[i] + 2);
			} else if (i + 1 < argc && argv[i + 1][0] != '-') {
				/* -l NAME */
				opt_login_arg = g_strdup(argv[i + 1]);
				strcpy(argv[i + 1], " ");
			}
			strcpy(argv[i], " ");
		}
		/* --away option */
		else if (strstr(argv[i], "--aw") == argv[i]) {
			char *equals;
			opt_away = 1;
			if ((equals = strchr(argv[i], '=')) != NULL) {
				/* --away=MESG */
				opt_away_arg = g_strdup(equals + 1);
				if (strlen(opt_away_arg) == 0) {
					g_free(opt_away_arg);
					opt_away_arg = NULL;
				}
			} else if (i + 1 < argc && argv[i + 1][0] != '-') {
				/* --away MESG */
				opt_away_arg = g_strdup(argv[i + 1]);
				strcpy(argv[i + 1], " ");
			}
			strcpy(argv[i], " ");
		}
		/* -w option */
		else if (strstr(argv[i], "-w") == argv[i]) {
			opt_away = 1;
			if (strlen(argv[i]) > 2) {
				/* -wMESG */
				opt_away_arg = g_strdup(argv[i] + 2);
			} else if (i + 1 < argc && argv[i + 1][0] != '-') {
				/* -w MESG */
				opt_away_arg = g_strdup(argv[i + 1]);
				strcpy(argv[i + 1], " ");
			}
			strcpy(argv[i], " ");
		}
	}
	/*
	   if (opt_login) {
	   printf ("--login given with arg %s\n",
	   opt_login_arg ? opt_login_arg : "NULL");
	   exit(0);
	   }
	 */

	gtk_set_locale();
	gtk_init(&argc, &argv);

	/* scan command-line options */
	opterr = 1;
	while ((opt = getopt_long(argc, argv,
#ifndef _WIN32
				  "adhu:f:vn", 
#else
				  "adghu:f:vn", 
#endif
				  long_options, NULL)) != -1) {
		switch (opt) {
		case 'u':	/* set user */
			opt_user = 1;
			opt_user_arg = g_strdup(optarg);
			break;
		case 'a':	/* account editor */
			opt_acct = 1;
			break;
		case 'd':	/* debug */
			opt_debug = 1;
			break;
		case 'f':
			opt_rcfile_arg = g_strdup(optarg);
			break;
		case 'v':	/* version */
			opt_version = 1;
			break;
		case 'h':	/* help */
			opt_help = 1;
			break;
		case 'n':       /* don't autologin */
			opt_nologin = 1;
			break;
#ifdef _WIN32
		case 'g':       /* debug GTK and GLIB */
			opt_gdebug = 1;
			break;
#endif
		case '?':
		default:
			show_usage(1, argv[0]);
			return 0;
			break;
		}
	}

#ifdef _WIN32
	/* We don't want a console window.. */
	/*
	 *  Any calls to the glib logging functions, result in a call to AllocConsole().
	 *  ME and 98 will in such cases produce a console window (2000 not), despite
	 *  being built as a windows app rather than a console app.  So we should either
	 *  ignore messages by setting dummy log handlers, or redirect messages.
	 *  This requires setting handlers for all domains (any lib which uses g_logging).
	 */
	
	g_log_set_handler ("Gdk", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
			   (opt_gdebug ? gaim_log_handler : gaim_dummy_log_handler),
			   NULL);
	g_log_set_handler ("Gtk", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
			   (opt_gdebug ? gaim_log_handler : gaim_dummy_log_handler),
			   NULL);
	g_log_set_handler ("GLib", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
			   (opt_gdebug ? gaim_log_handler : gaim_dummy_log_handler),
			   NULL);
	g_log_set_handler ("GModule", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
			   (opt_gdebug ? gaim_log_handler : gaim_dummy_log_handler),
			   NULL);
	g_log_set_handler ("GLib-GObject", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
			   (opt_gdebug ? gaim_log_handler : gaim_dummy_log_handler),
			   NULL);
	g_log_set_handler ("GThread", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
			   (opt_gdebug ? gaim_log_handler : gaim_dummy_log_handler),
			   NULL);

	/* g_print also makes a call to AllocConsole(), therefore a handler needs to be
	   set here aswell */
	if(!opt_debug)
		g_set_print_handler( gaim_dummy_print );

#endif

	/* show help message */
	if (opt_help) {
		show_usage(0, argv[0]);
		return 0;
	}
	/* show version message */
	if (opt_version) {
		printf("Gaim %s\n",VERSION);
		return 0;
	}
	
#if GAIM_PLUGINS || USE_PERL
	gaim_probe_plugins();
#endif

#ifdef _WIN32
	/* Various win32 initializations */
	wgaim_init();
#endif

	load_prefs();
	core_main();
#ifndef _WIN32
	ui_main();
#endif

	/* set the default username */
	if (opt_user_arg != NULL) {
		set_first_user(opt_user_arg);
		g_free(opt_user_arg);
		opt_user_arg = NULL;
	}

	if (misc_options & OPT_MISC_DEBUG)
		show_debug();

	static_proto_init();

	/* deal with --login */
	if (opt_login) {
		dologin_ret = dologin_named(opt_login_arg);
		if (opt_login_arg != NULL) {
			g_free(opt_login_arg);
			opt_login_arg = NULL;
		}
	}

	if (!opt_acct && !opt_nologin && gaim_session == 0)
		auto_login();

	if (opt_acct) {
		account_editor(NULL, NULL);
	} else if ((dologin_ret == -1) && !connections)
		show_login();

	gtk_main();
	core_quit();
	/* don't need ui_quit here because ui doesn't create anything */
#ifdef _WIN32
	wgaim_cleanup();
#endif
	return 0;

}
