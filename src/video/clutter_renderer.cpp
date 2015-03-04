/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "clutter_renderer.h"

// #include <gdk-pixbuf/gdk-pixbuf.h>
#include <video/renderer.h>
#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

struct _ClutterRenderer {
    GObject parent;
};

struct _ClutterRendererClass {
    GObjectClass parent_class;
};

typedef struct _ClutterRendererPrivate ClutterRendererPrivate;

/* Our private member structure */
struct _ClutterRendererPrivate {
    ClutterActor    *texture;
    Video::Renderer *renderer;
    QMetaObject::Connection frame_updated_connection;
};

G_DEFINE_TYPE_WITH_PRIVATE(ClutterRenderer, clutter_renderer, G_TYPE_OBJECT);

#define CLUTTER_RENDERER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_RENDERER_TYPE, ClutterRendererPrivate))

enum
{
    PROP_0,
    PROP_DRAWAREA,
    PROP_RENDERER,
    PROP_LAST
};

static GParamSpec *properties[PROP_LAST] = { NULL, };

static void clutter_renderer_finalize(GObject *);
static void clutter_renderer_dispose(GObject *);
static void clutter_renderer_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void clutter_renderer_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

static void
clutter_renderer_class_init(ClutterRendererClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = clutter_renderer_finalize;
    gobject_class->dispose = clutter_renderer_dispose;
    gobject_class->get_property = clutter_renderer_get_property;
    gobject_class->set_property = clutter_renderer_set_property;

    properties[PROP_DRAWAREA] = g_param_spec_pointer("texture", "Texture",
                                                     "Pointer to the Clutter Texture area",
                                                     (GParamFlags)(G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));

    properties[PROP_RENDERER] = g_param_spec_pointer("renderer", "Renderer",
                                                     "Pointer libRingClient Video::Renderer",
                                                     (GParamFlags)(G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_properties(gobject_class,
            PROP_LAST,
            properties);
}

static void
clutter_renderer_get_property(GObject *object, guint prop_id,
                              GValue *value, GParamSpec *pspec)
{
    ClutterRenderer *renderer = CLUTTER_RENDERER(object);
    ClutterRendererPrivate *priv = CLUTTER_RENDERER_GET_PRIVATE(renderer);

    switch (prop_id) {
        case PROP_DRAWAREA:
            g_value_set_pointer(value, priv->texture);
            break;
        case PROP_RENDERER:
            g_value_set_pointer(value, priv->renderer);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
clutter_renderer_set_property(GObject *object, guint prop_id,
                              const GValue *value, GParamSpec *pspec)
{
    ClutterRenderer *renderer = CLUTTER_RENDERER(object);
    ClutterRendererPrivate *priv = CLUTTER_RENDERER_GET_PRIVATE(renderer);

    switch (prop_id) {
        case PROP_DRAWAREA:
            priv->texture = (ClutterActor *)g_value_get_pointer(value);
            break;
        case PROP_RENDERER:
            priv->renderer = (Video::Renderer *)g_value_get_pointer(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}


static void
clutter_renderer_init(G_GNUC_UNUSED ClutterRenderer *self)
{
    /* nothign to do */
}

static void
clutter_renderer_finalize(GObject *obj)
{
    // ClutterRenderer *self = CLUTTER_RENDERER(obj);

    /* nothing to do */

    /* Chain up to the parent class */
    G_OBJECT_CLASS(clutter_renderer_parent_class)->finalize(obj);
}

static void
clutter_renderer_dispose(GObject *obj)
{
    ClutterRenderer *self = CLUTTER_RENDERER(obj);
    ClutterRendererPrivate *priv = CLUTTER_RENDERER_GET_PRIVATE(self);

    QObject::disconnect(priv->frame_updated_connection);

    /* Chain up to the parent class */
    G_OBJECT_CLASS(clutter_renderer_parent_class)->dispose(obj);
}


static void
clutter_renderer_render_to_texture(ClutterRenderer *self)
{
    ClutterRendererPrivate *priv = CLUTTER_RENDERER_GET_PRIVATE(self);

    if (!CLUTTER_IS_ACTOR(priv->texture)) {
        g_warning("texture is not a clutter actor");
        return;
    }

    const QByteArray& data = priv->renderer->currentFrame();
    QSize res = priv->renderer->size();

    g_debug("image w: %d, h: %d, size: %d", res.width(), res.height(), data.size());

    const gint BPP = 4;
    const gint ROW_STRIDE = BPP * res.width();

    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data(
                            (guchar *)data.constData(),
                            GDK_COLORSPACE_RGB,
                            TRUE,
                            8,
                            res.width(),
                            res.height(),
                            4*res.width(),
                            NULL,
                            NULL);

    /* update the clutter texture */
    GError *error = NULL;
    clutter_texture_set_from_rgb_data(CLUTTER_TEXTURE(priv->texture),
            (guchar*) gdk_pixbuf_get_pixels(pixbuf),
            gdk_pixbuf_get_has_alpha (pixbuf),
            gdk_pixbuf_get_width (pixbuf),
            gdk_pixbuf_get_height (pixbuf),
            gdk_pixbuf_get_rowstride (pixbuf),
            BPP,
            CLUTTER_TEXTURE_RGB_FLAG_BGR,
            &error);
    if (error) {
        g_warning("error rendering texture to clutter: %s", error->message);
        g_error_free(error);
    }

    g_object_unref(pixbuf);
}

/**
 * clutter_renderer_new:
 *
 * Create a new #ClutterRenderer instance.
 */
ClutterRenderer *
clutter_renderer_new(ClutterActor *texture, Video::Renderer *renderer)
{
    ClutterRenderer *rend = (ClutterRenderer *)g_object_new(CLUTTER_RENDERER_TYPE, "texture", texture,
                                                            "renderer", renderer, NULL);

    ClutterRendererPrivate *priv = CLUTTER_RENDERER_GET_PRIVATE(rend);

    priv->frame_updated_connection = QObject::connect(
        renderer,
        &Video::Renderer::frameUpdated,
        [=]() {
            clutter_renderer_render_to_texture(rend);
        }
    );

    return rend;
}
