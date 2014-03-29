/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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

#include "internal.h"
#include "glibcompat.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "imgstore.h"
#include "smiley.h"
#include "util.h"
#include "xmlnode.h"

#define PURPLE_SMILEY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_SMILEY, PurpleSmileyPrivate))

typedef struct {
	gchar *shortcut;
	gchar *path;
	gboolean is_ready;
} PurpleSmileyPrivate;

enum
{
	PROP_0,
	PROP_SHORTCUT,
	PROP_IS_READY,
	PROP_PATH,
	PROP_LAST
};

enum
{
	SIG_READY,
	SIG_LAST
};

static guint signals[SIG_LAST];
static GObjectClass *parent_class;
static GParamSpec *properties[PROP_LAST];

/*******************************************************************************
 * API implementation
 ******************************************************************************/

void
purple_smiley_set_shortcut(PurpleSmiley *smiley, const gchar *shortcut)
{
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(smiley);

	g_return_if_fail(priv != NULL);

	g_free(priv->shortcut);
	priv->shortcut = g_strdup(shortcut);
	g_object_notify_by_pspec(G_OBJECT(smiley), properties[PROP_SHORTCUT]);
}

const gchar *
purple_smiley_get_shortcut(const PurpleSmiley *smiley)
{
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(smiley);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->shortcut;
}

gboolean
purple_smiley_is_ready(const PurpleSmiley *smiley)
{
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(smiley);

	g_return_val_if_fail(priv != NULL, FALSE);

	return priv->is_ready;
}

const gchar *
purple_smiley_get_path(PurpleSmiley *smiley)
{
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(smiley);

	g_return_val_if_fail(priv != NULL, FALSE);

	return priv->path;
}


/*******************************************************************************
 * Object stuff
 ******************************************************************************/

static void
purple_smiley_init(GTypeInstance *instance, gpointer klass)
{
	PurpleSmiley *smiley = PURPLE_SMILEY(instance);
	PURPLE_DBUS_REGISTER_POINTER(smiley, PurpleSmiley);
}

static void
purple_smiley_finalize(GObject *obj)
{
	PurpleSmiley *smiley = PURPLE_SMILEY(obj);
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(smiley);

	g_free(priv->shortcut);
	g_free(priv->path);

	PURPLE_DBUS_UNREGISTER_POINTER(smiley);
}

static void
purple_smiley_get_property(GObject *object, guint par_id, GValue *value,
	GParamSpec *pspec)
{
	PurpleSmiley *smiley = PURPLE_SMILEY(object);
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(smiley);

	switch (par_id) {
		case PROP_SHORTCUT:
			g_value_set_string(value, priv->shortcut);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}

static void
purple_smiley_set_property(GObject *object, guint par_id, const GValue *value,
	GParamSpec *pspec)
{
	PurpleSmiley *smiley = PURPLE_SMILEY(object);
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(smiley);

	switch (par_id) {
		case PROP_SHORTCUT:
			g_free(priv->shortcut);
			priv->shortcut = g_strdup(g_value_get_string(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}

static void
purple_smiley_class_init(PurpleSmileyClass *klass)
{
	GObjectClass *gobj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PurpleSmileyPrivate));

	gobj_class->get_property = purple_smiley_get_property;
	gobj_class->set_property = purple_smiley_set_property;
	gobj_class->finalize = purple_smiley_finalize;

	properties[PROP_SHORTCUT] = g_param_spec_string("shortcut", "Shortcut",
		"The text-shortcut for the smiley", NULL,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(gobj_class, PROP_LAST, properties);

	signals[SIG_READY] = g_signal_new("ready", G_OBJECT_CLASS_TYPE(klass),
		0, 0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

GType
purple_smiley_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleSmileyClass),
			.class_init = (GClassInitFunc)purple_smiley_class_init,
			.instance_size = sizeof(PurpleSmiley),
			.instance_init = purple_smiley_init,
		};

		type = g_type_register_static(G_TYPE_OBJECT,
			"PurpleSmiley", &info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}
