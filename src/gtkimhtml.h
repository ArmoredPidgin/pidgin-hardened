/**
 * @file gtkimhtml.h GTK+ IM/HTML rendering component
 * @ingroup gtkui
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * under the terms of the GNU General Public License as published by
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
#ifndef _GAIM_GTKIMHTML_H_
#define _GAIM_GTKIMHTML_H_

#include <gdk/gdk.h>
#include <gtk/gtktextview.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkimage.h>

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************
 * @name Structures
 **************************************************************************/
/*@{*/

#define GTK_TYPE_IMHTML            (gtk_imhtml_get_type ())
#define GTK_IMHTML(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_IMHTML, GtkIMHtml))
#define GTK_IMHTML_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_IMHTML, GtkIMHtmlClass))
#define GTK_IS_IMHTML(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_IMHTML))
#define GTK_IS_IMHTML_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IMHTML))
#define GTK_IMHTML_SCALABLE(obj)   ((GtkIMHtmlScalable *)obj)

typedef struct _GtkIMHtml			GtkIMHtml;
typedef struct _GtkIMHtmlClass		GtkIMHtmlClass;
typedef struct _GtkIMHtmlFontDetail	GtkIMHtmlFontDetail;	/* The five elements contained in a FONT tag */
typedef struct _GtkSmileyTree		GtkSmileyTree;
typedef struct _GtkIMHtmlSmiley		GtkIMHtmlSmiley;
typedef struct _GtkIMHtmlScalable	GtkIMHtmlScalable;
typedef struct _GtkIMHtmlImage		GtkIMHtmlImage;
typedef struct _GtkIMHtmlHr			GtkIMHtmlHr;
typedef struct _GtkIMHtmlFuncs		GtkIMHtmlFuncs;

typedef enum {
	GTK_IMHTML_BOLD =      1 << 0,
	GTK_IMHTML_ITALIC =    1 << 1,
	GTK_IMHTML_UNDERLINE = 1 << 2,
	GTK_IMHTML_GROW =      1 << 3,
	GTK_IMHTML_SHRINK =    1 << 4,
	GTK_IMHTML_FACE =      1 << 5,
	GTK_IMHTML_FORECOLOR = 1 << 6,
	GTK_IMHTML_BACKCOLOR = 1 << 7,
	GTK_IMHTML_LINK =      1 << 8,
	GTK_IMHTML_IMAGE =     1 << 9,
	GTK_IMHTML_SMILEY =    1 << 10,
	GTK_IMHTML_LINKDESC =  1 << 11,
	GTK_IMHTML_STRIKE =    1 << 12,
	GTK_IMHTML_ALL =      -1
} GtkIMHtmlButtons;

struct _GtkIMHtml {
	GtkTextView text_view;
	GtkTextBuffer *text_buffer;
	GtkTextMark *scrollpoint;
	GdkCursor *hand_cursor;
	GdkCursor *arrow_cursor;
	GdkCursor *text_cursor;
	GHashTable *smiley_data;
	GtkSmileyTree *default_smilies;
	char *protocol_name;

	gboolean show_comments;

	gboolean html_shortcuts;
	gboolean smiley_shortcuts;

	GtkWidget *tip_window;
	char *tip;
	guint tip_timer;

	GList *scalables;
	GdkRectangle old_rect;

	gchar *search_string;

	gboolean editable;
	GtkIMHtmlButtons format_functions;
	gboolean wbfo;	/* Whole buffer formatting only. */

	gint insert_offset;

	struct {
		gboolean bold:1;
		gboolean italic:1;
		gboolean underline:1;
		gboolean strike:1;
		gchar *forecolor;
		gchar *backcolor;
		gchar *fontface;
		int fontsize;
		GtkTextTag *link;
	} edit;

	double zoom;
	int original_fsize;

	char *clipboard_text_string;
	char *clipboard_html_string;

	GSList *im_images;
	GtkIMHtmlFuncs *funcs;
};

struct _GtkIMHtmlClass {
	GtkTextViewClass parent_class;

	void (*url_clicked)(GtkIMHtml *, const gchar *);
	void (*buttons_update)(GtkIMHtml *, GtkIMHtmlButtons);
	void (*toggle_format)(GtkIMHtml *, GtkIMHtmlButtons);
	void (*clear_format)(GtkIMHtml *);
	void (*update_format)(GtkIMHtml *);
};

struct _GtkIMHtmlFontDetail {
	gushort size;
	gchar *face;
	gchar *fore;
	gchar *back;
	gchar *sml;
	gboolean underline;
};

struct _GtkSmileyTree {
	GString *values;
	GtkSmileyTree **children;
	GtkIMHtmlSmiley *image;
};

struct _GtkIMHtmlSmiley {
	gchar *smile;
	gchar *file;
	GdkPixbufAnimation *icon;
	gboolean hidden;
};

struct _GtkIMHtmlScalable {
	void (*scale)(struct _GtkIMHtmlScalable *, int, int);
	void (*add_to)(struct _GtkIMHtmlScalable *, GtkIMHtml *, GtkTextIter *);
	void (*free)(struct _GtkIMHtmlScalable *);
};

struct _GtkIMHtmlImage {
	GtkIMHtmlScalable scalable;
	GtkImage *image;
	GdkPixbuf *pixbuf;
	GtkTextMark *mark;
	gchar *filename;
	int width;
	int height;
	int id;
	GtkWidget *filesel;
};

struct _GtkIMHtmlHr {
	GtkIMHtmlScalable scalable;
	GtkWidget *sep;
};

typedef enum {
	GTK_IMHTML_NO_COLOURS    = 1 << 0,
	GTK_IMHTML_NO_FONTS      = 1 << 1,
	GTK_IMHTML_NO_COMMENTS   = 1 << 2, /* Remove */
	GTK_IMHTML_NO_TITLE      = 1 << 3,
	GTK_IMHTML_NO_NEWLINE    = 1 << 4,
	GTK_IMHTML_NO_SIZES      = 1 << 5,
	GTK_IMHTML_NO_SCROLL     = 1 << 6,
	GTK_IMHTML_RETURN_LOG    = 1 << 7,
	GTK_IMHTML_USE_POINTSIZE = 1 << 8
} GtkIMHtmlOptions;

typedef gpointer    (*GtkIMHtmlGetImageFunc)        (int id);
typedef gpointer    (*GtkIMHtmlGetImageDataFunc)    (gpointer i);
typedef size_t      (*GtkIMHtmlGetImageSizeFunc)    (gpointer i);
typedef const char *(*GtkIMHtmlGetImageFilenameFunc)(gpointer i);
typedef void        (*GtkIMHtmlImageRefFunc)        (int id);
typedef void        (*GtkIMHtmlImageUnrefFunc)      (int id);

struct _GtkIMHtmlFuncs {
	GtkIMHtmlGetImageFunc image_get;
	GtkIMHtmlGetImageDataFunc image_get_data;
	GtkIMHtmlGetImageSizeFunc image_get_size;
	GtkIMHtmlGetImageFilenameFunc image_get_filename;
	GtkIMHtmlImageRefFunc image_ref;
	GtkIMHtmlImageUnrefFunc image_unref;
};

/*@}*/

/**************************************************************************
 * @name GTK+ IM/HTML rendering component API
 **************************************************************************/
/*@{*/

/**
 * Returns the GType object for an IM/HTML widget.
 *
 * @return The GType for an IM/HTML widget.
 */
GType gtk_imhtml_get_type(void);

/**
 * Creates and returns a new GTK IM/HTML widget.
 *
 * @return The GTK IM/HTML widget created.
 */
GtkWidget *gtk_imhtml_new(void *, void *);

/**
 * Associates a smiley with a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param sml    The name of the smiley category.
 * @param smiley The GtkIMSmiley to associate.
 */
void gtk_imhtml_associate_smiley(GtkIMHtml *imhtml, gchar *sml, GtkIMHtmlSmiley *smiley);

/**
 * Removes all smileys associated with a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 */
void gtk_imhtml_remove_smileys(GtkIMHtml *imhtml);

/**
 * Sets the function callbacks to use with a GTK IM/HTML instance.
 *
 * @param imhtml The GTK IM/HTML.
 * @param f      The GtkIMHTMLFuncs struct containing the functions to use.
 */
void gtk_imhtml_set_funcs(GtkIMHtml *imhtml, GtkIMHtmlFuncs *f);

/**
 * Enables or disables showing the contents of HTML comments in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param show   @c TRUE if comments should be shown, or @c FALSE otherwise.
 */
void gtk_imhtml_show_comments(GtkIMHtml *imhtml, gboolean show);

/**
 * Enables or disables formatting shortcut keys in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param allow  @c TRUE if shortcut keys are allowed, or @c FALSE otherwise.
 */
void gtk_imhtml_html_shortcuts(GtkIMHtml *imhtml, gboolean allow);

/**
 * Enables or disables smiley insertion shortcut keys in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param allow  @c TRUE if shortcut keys are allowed, or @c FALSE otherwise.
 */
void gtk_imhtml_smiley_shortcuts(GtkIMHtml *imhtml, gboolean allow);

/**
 * Associates a protocol name with a GTK IM/HTML.
 *
 * @param imhtml        The GTK IM/HTML.
 * @param protocol_name The protocol name to associate with the IM/HTML.
 */
void gtk_imhtml_set_protocol_name(GtkIMHtml *imhtml, const gchar *protocol_name);

/**
 * Appends HTML formatted text to a GTK IM/HTML.
 *
 * @param imhtml  The GTK IM/HTML.
 * @param text    The formatted text to append.
 * @param options A GtkIMHtmlOptions object indicating insert behavior.
 */
#define gtk_imhtml_append_text(imhtml, text, options) \
 gtk_imhtml_append_text_with_images(imhtml, text, options, NULL)

/**
 * Appends HTML formatted text to a GTK IM/HTML.
 *
 * @param imhtml  The GTK IM/HTML.
 * @param text    The formatted text to append.
 * @param options A GtkIMHtmlOptions object indicating insert behavior.
 * @param unused  Use @c NULL value.
 */
void gtk_imhtml_append_text_with_images(GtkIMHtml *imhtml,
                                         const gchar *text,
                                         GtkIMHtmlOptions options,
                                         GSList *unused);

/**
 * Inserts HTML formatted text to a GTK IM/HTML at a given iter.
 *
 * @param imhtml  The GTK IM/HTML.
 * @param text    The formatted text to append.
 * @param options A GtkIMHtmlOptions object indicating insert behavior.
 * @param iter    A GtkTextIter in the GTK IM/HTML at which to insert text.
 */
void gtk_imhtml_insert_html_at_iter(GtkIMHtml        *imhtml,
                                    const gchar      *text,
                                    GtkIMHtmlOptions  options,
                                    GtkTextIter      *iter);

/**
 * Scrolls a GTK IM/HTML to the end of its contents.
 *
 * @param imhtml  The GTK IM/HTML.
 */
void gtk_imhtml_scroll_to_end(GtkIMHtml *imhtml);

/**
 * Purges the contents from a GTK IM/HTML and resets formatting.
 *
 * @param imhtml  The GTK IM/HTML.
 */
void gtk_imhtml_clear(GtkIMHtml *imhtml);

/**
 * Scrolls a GTK IM/HTML up by one page.
 *
 * @param imhtml  The GTK IM/HTML.
 */
void gtk_imhtml_page_up(GtkIMHtml *imhtml);

/**
 * Scrolls a GTK IM/HTML down by one page.
 *
 * @param imhtml  The GTK IM/HTML.
 */
void gtk_imhtml_page_down(GtkIMHtml *imhtml);

/**
 * Scales the font sizes in a GTK IM/HTML by a given factor.
 *
 * @param imhtml  The GTK IM/HTML.
 * @param zoom    The factor by which to scale the font sizes.
 */
void gtk_imhtml_font_zoom(GtkIMHtml *imhtml, double zoom);

/**
 * Creates and returns an new GTK IM/HTML scalable object.
 *
 * @return A new IM/HTML Scalable object.
 */
GtkIMHtmlScalable *gtk_imhtml_scalable_new();

/**
 * Creates and returns an new GTK IM/HTML scalable object with an image.
 *
 * @param img      A GdkPixbuf of the image to add.
 * @param filename The filename to associate with the image.
 * @param id       The id to associate with the image.
 *
 * @return A new IM/HTML Scalable object with an image.
 */
GtkIMHtmlScalable *gtk_imhtml_image_new(GdkPixbuf *img, const gchar *filename, int id);

/**
 * Destroys and frees a GTK IM/HTML scalable image.
 *
 * @param scale The GTK IM/HTML scalable.
 */
void gtk_imhtml_image_free(GtkIMHtmlScalable *scale);

/**
 * Rescales a GTK IM/HTML scalable image to a given size.
 *
 * @param scale  The GTK IM/HTML scalable.
 * @param width  The new width.
 * @param height The new height.
 */
void gtk_imhtml_image_scale(GtkIMHtmlScalable *scale, int width, int height);

/**
 * Adds a GTK IM/HTML scalable image to a given GTK IM/HTML at a given iter.
 *
 * @param scale  The GTK IM/HTML scalable.
 * @param imhtml The GTK IM/HTML.
 * @param iter   The GtkTextIter at which to add the scalable.
 */
void gtk_imhtml_image_add_to(GtkIMHtmlScalable *scale, GtkIMHtml *imhtml, GtkTextIter *iter);

/**
 * Creates and returns an new GTK IM/HTML scalable with a horizontal rule.
 *
 * @return A new IM/HTML Scalable object with an image.
 */
GtkIMHtmlScalable *gtk_imhtml_hr_new();

/**
 * Destroys and frees a GTK IM/HTML scalable horizontal rule.
 *
 * @param scale The GTK IM/HTML scalable.
 */
void gtk_imhtml_hr_free(GtkIMHtmlScalable *scale);

/**
 * Rescales a GTK IM/HTML scalable horizontal rule to a given size.
 *
 * @param scale  The GTK IM/HTML scalable.
 * @param width  The new width.
 * @param height The new height.
 */
void gtk_imhtml_hr_scale(GtkIMHtmlScalable *scale, int width, int height);

/**
 * Adds a GTK IM/HTML scalable horizontal rule to a given GTK IM/HTML at
 * a given iter.
 *
 * @param scale  The GTK IM/HTML scalable.
 * @param imhtml The GTK IM/HTML.
 * @param iter   The GtkTextIter at which to add the scalable.
 */
void gtk_imhtml_hr_add_to(GtkIMHtmlScalable *scale, GtkIMHtml *imhtml, GtkTextIter *iter);

/**
 * Finds and highlights a given string in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param text   The string to search for.
 *
 * @return @c TRUE if a search was performed, or @c FALSE if not.
 */
gboolean gtk_imhtml_search_find(GtkIMHtml *imhtml, const gchar *text);

/**
 * Clears the highlighting from a prior search in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 */
void gtk_imhtml_search_clear(GtkIMHtml *imhtml);

/**
 * Enables or disables editing in a GTK IM/HTML.
 *
 * @param imhtml   The GTK IM/HTML.
 * @param editable @c TRUE to make the widget editable, or @c FALSE otherwise.
 */
void gtk_imhtml_set_editable(GtkIMHtml *imhtml, gboolean editable);

/**
 * Enables or disables whole buffer formatting only (wbfo) in a GTK IM/HTML.
 * In this mode formatting options to the buffer take effect for the entire
 * buffer instead of specific text.
 *
 * @param imhtml The GTK IM/HTML.
 * @param wbfo   @c TRUE to enable the mode, or @c FALSE otherwise.
 */
void gtk_imhtml_set_whole_buffer_formatting_only(GtkIMHtml *imhtml, gboolean wbfo);

/**
 * Indicates which formatting functions to enable and disable in a GTK IM/HTML.
 *
 * @param imhtml  The GTK IM/HTML.
 * @param buttons A GtkIMHtmlButtons bitmask indicating which functions to use.
 */
void gtk_imhtml_set_format_functions(GtkIMHtml *imhtml, GtkIMHtmlButtons buttons);

/**
 * Returns which formatting functions are enabled in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 *
 * @return A GtkIMHtmlButtons bitmask indicating which functions to are enabled.
 */
GtkIMHtmlButtons gtk_imhtml_get_format_functions(GtkIMHtml *imhtml);

/**
 * Sets each boolean to TRUE if that formatting option is enabled at the
 * current position in a GTK IM/HTML.
 *
 * @param imhtml    The GTK IM/HTML.
 * @param bold      A reference to a boolean for bold.
 * @param italic    A reference to a boolean for italic.
 * @param underline A reference to a boolean for underline.
 */
void gtk_imhtml_get_current_format(GtkIMHtml *imhtml, gboolean *bold, gboolean *italic, gboolean *underline);

/**
 * Returns a string containing the selected font face at the current position
 * in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 *
 * @return A string containg the font face or @c NULL if none is set.
 */
char *gtk_imhtml_get_current_fontface(GtkIMHtml *imhtml);

/**
 * Returns a string containing the selected foreground color at the current
 * position in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 *
 * @return A string containg the foreground color or @c NULL if none is set.
 */
char *gtk_imhtml_get_current_forecolor(GtkIMHtml *imhtml);

/**
 * Returns a string containing the selected background color at the current
 * position in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 *
 * @return A string containg the background color or @c NULL if none is set.
 */
char *gtk_imhtml_get_current_backcolor(GtkIMHtml *imhtml);

/**
 * Returns a integer containing the selected HTML font size at the current
 * position in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 *
 * @return The HTML font size.
 */
gint gtk_imhtml_get_current_fontsize(GtkIMHtml *imhtml);

/**
 * Checks whether a GTK IM/HTML is marked as editable.
 *
 * @param imhtml The GTK IM/HTML.
 *
 * @return @c TRUE if the IM/HTML is editable, or @c FALSE otherwise.
 */
gboolean gtk_imhtml_get_editable(GtkIMHtml *imhtml);

/**
 * Toggles bold at the cursor location or selection in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 *
 * @return @c TRUE if bold was turned on, or @c FALSE if it was turned off.
 */
gboolean gtk_imhtml_toggle_bold(GtkIMHtml *imhtml);

/**
 * Toggles italic at the cursor location or selection in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 *
 * @return @c TRUE if italic was turned on, or @c FALSE if it was turned off.
 */
gboolean gtk_imhtml_toggle_italic(GtkIMHtml *imhtml);

/**
 * Toggles underline at the cursor location or selection in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 *
 * @return @c TRUE if underline was turned on, or @c FALSE if it was turned off.
 */
gboolean gtk_imhtml_toggle_underline(GtkIMHtml *imhtml);

/**
 * Toggles strikethrough at the cursor location or selection in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 *
 * @return @c TRUE if strikethrough was turned on, or @c FALSE if it was turned off.
 */
gboolean gtk_imhtml_toggle_strike(GtkIMHtml *imhtml);

/**
 * Toggles a foreground color at the current location or selection in a GTK
 * IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param color  The HTML-style color, or @c NULL or "" to clear the color.
 *
 * @return @c TRUE if a color was set, or @c FALSE if it was cleared.
 */
gboolean gtk_imhtml_toggle_forecolor(GtkIMHtml *imhtml, const char *color);

/**
 * Toggles a background color at the current location or selection in a GTK
 * IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param color  The HTML-style color, or @c NULL or "" to clear the color.
 *
 * @return @c TRUE if a color was set, or @c FALSE if it was cleared.
 */
gboolean gtk_imhtml_toggle_backcolor(GtkIMHtml *imhtml, const char *color);

/**
 * Toggles a font face at the current location or selection in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param face   The font face name, or @c NULL or "" to clear the font.
 *
 * @return @c TRUE if a font name was set, or @c FALSE if it was cleared.
 */
gboolean gtk_imhtml_toggle_fontface(GtkIMHtml *imhtml, const char *face);

/**
 * Toggles a link tag with the given URL at the current location or selection
 * in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param url    The URL for the link or @c NULL to terminate the link.
 */
void gtk_imhtml_toggle_link(GtkIMHtml *imhtml, const char *url);

/**
 * Inserts a link to the given url at the given GtkTextMark in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param mark   The GtkTextMark to insert the link at.
 * @param url    The URL for the link.
 * @param text   The string to use for the link description.
 */
void gtk_imhtml_insert_link(GtkIMHtml *imhtml, GtkTextMark *mark, const char *url, const char *text);

/**
 * Inserts a smiley at the current location or selection in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param sml    The category of the smiley.
 * @param smiley The text of the smiley to insert.
 */
void gtk_imhtml_insert_smiley(GtkIMHtml *imhtml, const char *sml, char *smiley);
/**
 * Inserts a smiley at the given iter in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param sml    The category of the smiley.
 * @param smiley The text of the smiley to insert.
 * @param iter   The GtkTextIter in the IM/HTML to insert the smiley at.
 */
void gtk_imhtml_insert_smiley_at_iter(GtkIMHtml *imhtml, const char *sml, char *smiley, GtkTextIter *iter);

/**
 * Inserts the IM/HTML scalable image with the given id at the given iter in a
 * GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param id     The id of the IM/HTML scalable.
 * @param iter   The GtkTextIter in the IM/HTML to insert the image at.
 */
void gtk_imhtml_insert_image_at_iter(GtkIMHtml *imhtml, int id, GtkTextIter *iter);

/**
 * Sets the font size at the current location or selection in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param size   The HTML font size to use.
 */
void gtk_imhtml_font_set_size(GtkIMHtml *imhtml, gint size);

/**
 * Decreases the font size by 1 at the current location or selection in a GTK
 * IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 */
void gtk_imhtml_font_shrink(GtkIMHtml *imhtml);

/**
 * Increases the font size by 1 at the current location or selection in a GTK
 * IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 */
void gtk_imhtml_font_grow(GtkIMHtml *imhtml);

/**
 * Returns the HTML formatted contents between two iters in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param start  The GtkTextIter indicating the start point in the IM/HTML.
 * @param end    The GtkTextIter indicating the end point in the IM/HTML.
 *
 * @return A string containing the HTML formatted text.
 */
char *gtk_imhtml_get_markup_range(GtkIMHtml *imhtml, GtkTextIter *start, GtkTextIter *end);

/**
 * Returns the entire HTML formatted contents of a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 *
 * @return A string containing the HTML formatted text.
 */
char *gtk_imhtml_get_markup(GtkIMHtml *imhtml);

/**
 * Returns a null terminated array of pointers to null terminated strings, each
 * string for each line.  g_strfreev() should be called to free it when done.
 *
 * @param imhtml The GTK IM/HTML.
 *
 * @return A null terminated array of null terminated HTML formatted strings.
 */
char **gtk_imhtml_get_markup_lines(GtkIMHtml *imhtml);

/**
 * Returns the entire unformatted (plain text) contents of a GTK IM/HTML
 * between two iters in a GTK IM/HTML.
 *
 * @param imhtml The GTK IM/HTML.
 * @param start  The GtkTextIter indicating the start point in the IM/HTML.
 * @param stop   The GtkTextIter indicating the end point in the IM/HTML.
 *
 * @return A string containing the unformatted text.
 */
char *gtk_imhtml_get_text(GtkIMHtml *imhtml, GtkTextIter *start, GtkTextIter *stop);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_GTKIMHTML_H_ */
