/**************************************************************
**
** GaimGnomeAppletMgr
** Author - Quinticent (John Palmieri: johnp@martianrock.com)
**
** Purpose - Takes over the task of managing the GNOME applet
**           code and provides a centralized codebase for
**	     GNOME integration for Gaim.
**
**
** gaim
**
** Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#ifdef USE_APPLET
#include <string.h>
#include <gdk_imlib.h>
#include "gaim.h"
#include "gnome_applet_mgr.h"

enum gaim_user_states MRI_user_status; 

gboolean buddy_created = FALSE;
gboolean applet_draw_open = FALSE;
GtkWidget *applet_popup = NULL;

gchar GAIM_GNOME_OFFLINE_ICON[255] = GAIM_GNOME_PENGUIN_OFFLINE;
gchar GAIM_GNOME_CONNECT_ICON[255] = GAIM_GNOME_PENGUIN_CONNECT;
gchar GAIM_GNOME_ONLINE_ICON[255] = GAIM_GNOME_PENGUIN_ONLINE;

GtkWidget *applet;
GtkWidget *appletframe;
GtkWidget *status_label;

GtkWidget *icon;
GdkPixmap *icon_offline_pm=NULL;
GdkPixmap *icon_offline_bm=NULL;

GdkPixmap *icon_online_pm=NULL;
GdkPixmap *icon_online_bm=NULL;

GdkPixmap *icon_connect_pm=NULL;
GdkPixmap *icon_connect_bm=NULL;

GdkPixmap *icon_msg_pending_pm=NULL;
GdkPixmap *icon_msg_pending_bm=NULL;

GdkPixmap *icon_away_pm=NULL;
GdkPixmap *icon_away_bm=NULL;

static GtkAllocation get_applet_pos(gboolean);

/***************************************************************
**
** function load_applet_icon
** visibility - private
**
** input:
**	name - the name of the file to load
**	height, width - the height and width 
**					  that the icon should be
**					 scaled to.
**
** output:
**	TRUE - success
**      FALSE - failure
**	pm - a GdkPixmap structure that the icon is loaded into
**      bm - a GdkBitmap structure that the icon's transparancy
**		mask is loaded into
**
** description - loads an icon from 
**                    /usr/share/pixmap/gaim/gnome/
**                    and scales it using imlib
**
****************************************************************/

gboolean load_applet_icon( const char *name, int height, int width, GdkPixmap **pm, GdkBitmap **bm ){
	gboolean result = TRUE;
	char *path;
	GdkImlibImage *im;

	path = gnome_pixmap_file(name);	

	im=gdk_imlib_load_image( path );
	
	if ((*pm)!=NULL)
		gdk_imlib_free_pixmap((*pm));
	
	if( im!= NULL ){
		gdk_imlib_render(im,width,height);
	
		(*pm) = gdk_imlib_move_image(im);
		(*bm) = gdk_imlib_move_mask(im);	
		
	} else {
		result = FALSE;
		sprintf(debug_buff,_("file not found: %s\n"),path);
		debug_print(debug_buff);
	}
	
	free(path);
	return result;
}

/***************************************************************
**
** function update_applet
** visibility - private
**
** input:
**	ap - if not NULL, was called from update_pixmaps, and
**		should update them
**
** description - takes care of swapping status icons and 
**			  updating the status label
**
****************************************************************/ 

gboolean update_applet( gpointer *ap ){
     static enum gaim_user_states old_user_status = offline;
     
     if( MRI_user_status != old_user_status || ap){

         switch( MRI_user_status ){
      		case offline:
      			gtk_pixmap_set( GTK_PIXMAP(icon),
                           icon_offline_pm,
                           icon_offline_bm );
         	       gtk_label_set( GTK_LABEL(status_label), _MSG_OFFLINE_ );
		       applet_set_tooltips(_("Offilne. Click to bring up login box."));
      		break;
      		case signing_on:
      			gtk_pixmap_set( GTK_PIXMAP(icon),
                           icon_connect_pm,
                           icon_connect_bm );   
      			gtk_label_set( GTK_LABEL(status_label), _MSG_CONNECT_ );
			applet_set_tooltips(_("Attempting to sign on...."));
      		break;
      		case online:
      			gtk_pixmap_set( GTK_PIXMAP(icon),
                           icon_online_pm,
                           icon_online_bm );                
                   
                	gtk_label_set( GTK_LABEL(status_label), _MSG_ONLINE_ );
      		break;
      
      		case unread_message_pending:
      			gtk_pixmap_set( GTK_PIXMAP(icon),
                           icon_msg_pending_pm,
                           icon_msg_pending_bm );   
      			gtk_label_set( GTK_LABEL(status_label), "msg" );
      		break;
      		case away:
      			gtk_pixmap_set( GTK_PIXMAP(icon),
                           icon_online_pm,
                           icon_online_bm );   
      			gtk_label_set( GTK_LABEL(status_label), _("Away") );
      		break;
      	}
      	old_user_status = MRI_user_status;
      }
      return TRUE;

}

void update_pixmaps() {
	if (display_options & OPT_DISP_DEVIL_PIXMAPS) {
		sprintf(GAIM_GNOME_OFFLINE_ICON, "%s",  GAIM_GNOME_DEVIL_OFFLINE);
		sprintf(GAIM_GNOME_CONNECT_ICON, "%s",  GAIM_GNOME_DEVIL_CONNECT);
		sprintf(GAIM_GNOME_ONLINE_ICON, "%s",  GAIM_GNOME_DEVIL_ONLINE);
	} else {
		sprintf(GAIM_GNOME_OFFLINE_ICON, "%s",  GAIM_GNOME_PENGUIN_OFFLINE);
		sprintf(GAIM_GNOME_CONNECT_ICON, "%s",  GAIM_GNOME_PENGUIN_CONNECT);
		sprintf(GAIM_GNOME_ONLINE_ICON, "%s",  GAIM_GNOME_PENGUIN_ONLINE);
	}
	load_applet_icon( GAIM_GNOME_OFFLINE_ICON, 32, 34,
			&icon_offline_pm, &icon_offline_bm );
	load_applet_icon( GAIM_GNOME_CONNECT_ICON, 32, 34,
			&icon_connect_pm, &icon_connect_bm );
	load_applet_icon( GAIM_GNOME_ONLINE_ICON, 32, 34,
			&icon_online_pm, &icon_online_bm );
	update_applet((gpointer *)applet);
}


/***************************************************************
**
** function make_buddy
** visibility - private
**
** description - If buddylist is not created create it
**                    else show the buddy list
**
****************************************************************/ 
void make_buddy(void) {
        set_applet_draw_open();        
	if( !buddy_created ){
		show_buddy_list();
		buddy_created = TRUE;
	} else {
		gnome_buddy_show();
	}	
	build_edit_tree();
	build_permit_tree();
	
}

/***************************************************************
**
** function applet_show_login
** visibility - private
**
** input:
**
**
** description - I guess it shows the login dialog
**
****************************************************************/
extern GtkWidget *mainwindow;
void applet_show_login(AppletWidget *widget, gpointer data) {
        show_login();
	if (general_options & OPT_GEN_NEAR_APPLET) {
		GtkAllocation a = get_applet_pos(FALSE);
		gtk_widget_set_uposition(mainwindow, a.x, a.y);
	}
}

void applet_do_signon(AppletWidget *widget, gpointer data) {
	show_login();
	if (general_options & OPT_GEN_REMEMBER_PASS)
		dologin();
}

void insert_applet_away() {
	GList *awy = away_messages;
	struct away_message *a;
	char  *awayname;

	applet_widget_register_callback_dir(APPLET_WIDGET(applet),
		"away/",
		_("Away"));
	applet_widget_register_callback(APPLET_WIDGET(applet),
		"away/new",
		_("New Away Message"),
		(AppletCallbackFunc)create_away_mess,
		NULL);

	while(awy) {
		a = (struct away_message *)awy->data;

		awayname = g_malloc(sizeof *awayname * (6 + strlen(a->name)));
		awayname[0] = '\0';
		strcat(awayname, "away/");
		strcat(awayname, a->name);
		applet_widget_register_callback(APPLET_WIDGET(applet),
			awayname,
			a->name,
			(AppletCallbackFunc)do_away_message,
			a);

		awy = awy->next;
		free(awayname);
	}
}

void remove_applet_away() {
	GList *awy = away_messages;
	struct away_message *a;
	char  *awayname;

	applet_widget_unregister_callback(APPLET_WIDGET(applet), "away/new");

	while (awy) {
		a = (struct away_message *)awy->data;

		awayname = g_malloc(sizeof *awayname * (6 + strlen(a->name)));
		awayname[0] = '\0';
		strcat(awayname, "away/");
		strcat(awayname, a->name);
		applet_widget_unregister_callback(APPLET_WIDGET(applet), awayname);

		awy = awy->next;
		free(awayname);
	}
	applet_widget_unregister_callback_dir(APPLET_WIDGET(applet), "away/");
	applet_widget_unregister_callback(APPLET_WIDGET(applet), "away");
}

/***************************************************************
**
** function applet_show_about
** visibility - public
**
**
** description - takes care of creating and
**                    displaying the about box
**
****************************************************************/
void applet_show_about(AppletWidget *widget, gpointer data) {
  
  const gchar *authors[] = {"Mark Spencer <markster@marko.net>",
                            "Jim Duchek <jimduchek@ou.edu>",
                            "Rob Flynn <rflynn@blueridge.net>",
			    "Eric Warmenhoven <warmenhoven@yahoo.com>",
			    "Syd Logan",
                            NULL};

  GtkWidget *about=gnome_about_new(_("GAIM"),
				   _(VERSION),
				   _(""),
				   authors,
				   "",
				   NULL);
  gtk_widget_show(about);
}

/***************************************************************
**
** function AppletCancelLogin (name should be changed to 
**									applet_cancel_login)
** visibility - public
**
** description - called when user cancels login
**
****************************************************************/ 
void AppletCancelLogon(){
  applet_widget_unregister_callback(APPLET_WIDGET(applet),"signoff");
  applet_widget_register_callback(APPLET_WIDGET(applet),
				  "signon",
				  _("Signon"),
				  applet_do_signon,
				  NULL);
}

/***************************************************************
**
** function get_applet_pos
** visibility - private
**
** output:
**	GtKAllocation - a Gtk struct that holds the 
**						position of the dialog
**
** description - returns the x,y position the buddy list should
** 				should be placed based on the position
**                      of the applet and the orientation
**				of the Gnome panel.
**
****************************************************************/ 
GtkAllocation get_applet_pos(gboolean for_blist) {
    gint x,y,pad;
    GtkRequisition buddy_req, applet_req;
    GtkAllocation result;
    GNOME_Panel_OrientType orient = applet_widget_get_panel_orient( APPLET_WIDGET(applet) );
    pad = 5;
    gdk_window_get_position(gtk_widget_get_parent_window(appletframe), &x, &y);
    if (for_blist)
	    buddy_req = gnome_buddy_get_dimentions();
    else
	    buddy_req = mainwindow->requisition;
    applet_req = appletframe->requisition;
   switch( orient ){
   	case ORIENT_UP:
   		result.x=x;
   		result.y=y-(buddy_req.height+pad);
   	break;
	case ORIENT_DOWN:
   		result.x=x; 
   		result.y=y+applet_req.height+pad; 
   	
   	break;
   	case ORIENT_LEFT:
   		result.x=x-(buddy_req.width + pad );
   		result.y=y;
   	break;
   	case ORIENT_RIGHT:
   		result.x=x+applet_req.width+pad;
   		result.y=y;
   	break;
   } 
   
   
   return result;
}



void createOfflinePopup(){
	applet_show_login( APPLET_WIDGET(applet), NULL );
}


void createSignonPopup(){
	applet_draw_open = FALSE;
}


void createOnlinePopup(){
    GtkAllocation al;
    make_buddy();
    al  = get_applet_pos(TRUE);  
    gnome_buddy_set_pos(  al.x, al.y );
}


void createPendingPopup(){
    applet_draw_open = FALSE;
}


void createAwayPopup(){
     createOnlinePopup();
}


void closeOfflinePopup(){
	cancel_logon();
	set_applet_draw_closed();
}


void closeSignonPopup(){

}


void closeOnlinePopup(){
    set_applet_draw_closed();
    applet_destroy_buddy();
}


void closePendingPopup(){
    applet_draw_open = FALSE;
}


void closeAwayPopup(){
	closeOnlinePopup();
}

void AppletClicked( GtkWidget *sender, GdkEventButton *ev, gpointer data ){
	if (!ev || ev->button != 1 || ev->type != GDK_BUTTON_PRESS)
		return;
        
	if( applet_draw_open ){
	  	switch( MRI_user_status ){
			case offline:
				closeOfflinePopup();
			break;
			case signing_on:
				closeSignonPopup();
			break;
			case online:
				closeOnlinePopup();
				
			break;
			case unread_message_pending:
				closePendingPopup();
			break;
			case away:
				closeAwayPopup();
			break;
		}     
	} else {
		set_applet_draw_open();
		switch( MRI_user_status ){
			case offline:
				createOfflinePopup();
			break;
			case signing_on:
				createSignonPopup();
			break;
			case online:
				createOnlinePopup();
			break;
			case unread_message_pending:
				createPendingPopup();
			break;
			case away:
				createAwayPopup();
			break;
		}
		
				
	}
}


/***************************************************************
**
** Initialize GNOME stuff
**
****************************************************************/

gint init_applet_mgr(int argc, char *argv[]) {
	GtkWidget *vbox;
	
	GtkStyle *label_style;
	GdkFont *label_font = NULL;

        applet_widget_init("GAIM",VERSION,argc,argv,NULL,0,NULL);
        
        /*init imlib for graphics*/ 
        gdk_imlib_init();
        gtk_widget_push_visual(gdk_imlib_get_visual());
        gtk_widget_push_colormap(gdk_imlib_get_colormap());

        applet=applet_widget_new("gaim_applet");
        if(!applet) g_error(_("Can't create GAIM applet!"));
	gtk_widget_set_events(applet, gtk_widget_get_events(applet) |
				GDK_BUTTON_PRESS_MASK);

        appletframe = gtk_frame_new(NULL);
        
        gtk_widget_set_usize(appletframe, 48, 48);
        
	
	/*load offline icon*/
	load_applet_icon( GAIM_GNOME_OFFLINE_ICON, 32, 32,
			&icon_offline_pm, &icon_offline_bm );
 
 	/*load connecting icon*/
 	load_applet_icon( GAIM_GNOME_CONNECT_ICON, 32, 32,
			&icon_connect_pm, &icon_connect_bm );
							 
	/*load online icon*/
	load_applet_icon( GAIM_GNOME_ONLINE_ICON, 32, 32,
			&icon_online_pm, &icon_online_bm );
 	
 	/*icon_away and icon_msg_pennding need to be implemented*/		
		
	icon=gtk_pixmap_new(icon_offline_pm,icon_offline_bm);
	
	update_applet(NULL);
	gtk_timeout_add( 1500, (GtkFunction)update_applet, NULL );
	
	vbox = gtk_vbox_new(FALSE,0);
	
	gtk_box_pack_start(GTK_BOX(vbox), icon, FALSE, TRUE, 0);
	
	status_label = gtk_label_new(_("Offline"));
	/*set this label's font*/
	label_style = gtk_widget_get_style( status_label );
	
	label_font = gdk_font_load( _MSG_FONT_ );
	         
	
	if( label_font != NULL ){
		label_style->font = label_font; 
		gtk_widget_set_style( status_label, label_style );
	} else {
		sprintf(debug_buff, _("Font does not exist") );
		debug_print(debug_buff);
	}
	
	gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, TRUE, 0);
	
	gtk_container_add( GTK_CONTAINER(appletframe), vbox );
	applet_widget_add(APPLET_WIDGET(applet), appletframe);
	
	gtk_widget_show( status_label );
	gtk_widget_show( vbox );
	gtk_widget_show( appletframe );
	        
	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "about",
					      GNOME_STOCK_MENU_ABOUT,
					      _("About..."),
					      applet_show_about,
					      NULL);
					      
	gtk_signal_connect( GTK_OBJECT(applet), "button_press_event", GTK_SIGNAL_FUNC( AppletClicked), NULL);

        gtk_widget_show(icon);
        gtk_widget_show(applet);
        return 0;
}

void setUserState( enum gaim_user_states state ){
	MRI_user_status = state; 
	update_applet(NULL);
}

enum gaim_user_states getUserState(){
	return MRI_user_status;
}

void set_applet_draw_open(){
	applet_draw_open = TRUE;
}

void set_applet_draw_closed(){
	applet_draw_open = FALSE;
}

void applet_set_tooltips(char *msg) {
	applet_widget_set_tooltip(APPLET_WIDGET(applet), msg);
}

#endif /*USE_APPLET*/
