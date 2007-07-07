/**
 * @file gntslider.h Slider API
 * @ingroup gnt
 */
/*
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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

#ifndef GNT_SLIDER_H
#define GNT_SLIDER_H

#include "gntwidget.h"
#include "gnt.h"
#include "gntlabel.h"

#define GNT_TYPE_SLIDER             (gnt_slider_get_gtype())
#define GNT_SLIDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_SLIDER, GntSlider))
#define GNT_SLIDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_SLIDER, GntSliderClass))
#define GNT_IS_SLIDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_SLIDER))
#define GNT_IS_SLIDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_SLIDER))
#define GNT_SLIDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_SLIDER, GntSliderClass))

#define GNT_SLIDER_FLAGS(obj)                (GNT_SLIDER(obj)->priv.flags)
#define GNT_SLIDER_SET_FLAGS(obj, flags)     (GNT_SLIDER_FLAGS(obj) |= flags)
#define GNT_SLIDER_UNSET_FLAGS(obj, flags)   (GNT_SLIDER_FLAGS(obj) &= ~(flags))

typedef struct _GntSlider			GntSlider;
typedef struct _GntSliderPriv		GntSliderPriv;
typedef struct _GntSliderClass		GntSliderClass;

struct _GntSlider
{
	GntWidget parent;

	gboolean vertical;

	int max;        /* maximum value */
	int min;        /* minimum value */
	int step;       /* amount to change at each step */
	int current;    /* current value */
};

struct _GntSliderClass
{
	GntWidgetClass parent;

	void (*changed)(GntSlider *slider, int);
	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * @return The GType for GntSlider
 */
GType gnt_slider_get_gtype(void);

#define gnt_hslider_new(max, min) gnt_slider_new(FALSE, max, min)
#define gnt_vslider_new(max, min) gnt_slider_new(TRUE, max, min)

/**
 * Create a new slider.
 *
 * @param orient A vertical slider is created if @c TRUE, otherwise the slider is horizontal.
 * @param max    The maximum value for the slider
 * @param min    The minimum value for the slider
 *
 * @return  The newly created slider
 */
GntWidget * gnt_slider_new(gboolean orient, int max, int min);

/**
 * Set the range of the slider.
 *
 * @param slider  The slider
 * @param max     The maximum value
 * @param min     The minimum value
 */
void gnt_slider_set_range(GntSlider *slider, int max, int min);

/**
 * Sets the amount of change at each step.
 * 
 * @param slider  The slider
 * @param step    The amount for each ste
 */
void gnt_slider_set_step(GntSlider *slider, int step);

/**
 * Advance the slider forward or backward.
 *
 * @param slider   The slider
 * @param steps    The number of amounts to change, positive to change
 *                 forward, negative to change backward
 *
 * @return   The value of the slider after the change
 */
int gnt_slider_advance_step(GntSlider *slider, int steps);

/**
 * Set the current value for the slider.
 *
 * @param slider  The slider
 * @param value   The current value
 */
void gnt_slider_set_value(GntSlider *slider, int value);

/**
 * Update a label with the value of the slider whenever the value changes.
 *
 * @param slider   The slider
 * @param label    The label to update
 */
void gnt_slider_reflect_label(GntSlider *slider, GntLabel *label);

G_END_DECLS

#endif /* GNT_SLIDER_H */
