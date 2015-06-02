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

#include "videowindow.h"

#include <gtk/gtk.h>
#include <call.h>
#include "video_widget.h"
#include <video/previewmanager.h>

struct _VideoWindow
{
    GtkWindow parent;
};

struct _VideoWindowClass
{
    GtkWindowClass parent_class;
};

typedef struct _VideoWindowPrivate VideoWindowPrivate;

struct _VideoWindowPrivate
{
    GtkWidget *video_widget;

    QMetaObject::Connection local_renderer_connection;
    QMetaObject::Connection remote_renderer_connection;
};

G_DEFINE_TYPE_WITH_PRIVATE(VideoWindow, video_window, GTK_TYPE_WINDOW);

#define VIDEO_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), VIDEO_WINDOW_TYPE, VideoWindowPrivate))

static void
video_window_dispose(GObject *object)
{
    VideoWindow *self;
    VideoWindowPrivate *priv;

    self = VIDEO_WINDOW(object);
    priv = VIDEO_WINDOW_GET_PRIVATE(self);

    QObject::disconnect(priv->local_renderer_connection);
    QObject::disconnect(priv->remote_renderer_connection);

    G_OBJECT_CLASS(video_window_parent_class)->dispose(object);
}

static void
video_window_init(G_GNUC_UNUSED VideoWindow *self)
{
    // gtk_widget_init_template(GTK_WIDGET(self));
}

static void
video_window_class_init(VideoWindowClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = video_window_dispose;
}

GtkWidget *
video_window_new(Call *call, GtkWindow *parent)
{
    GtkWidget *self = (GtkWidget *)g_object_new(VIDEO_WINDOW_TYPE, NULL);
    VideoWindowPrivate *priv = VIDEO_WINDOW_GET_PRIVATE(self);

    gtk_window_set_decorated(GTK_WINDOW(self), FALSE);
    if (parent)
        gtk_window_set_transient_for(GTK_WINDOW(self), parent);

    /* video widget */
    priv->video_widget = video_widget_new();
    gtk_container_add(GTK_CONTAINER(self), priv->video_widget);
    gtk_widget_show_all(priv->video_widget);

    /* check if we already have a renderer */
    video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                   call->videoRenderer(),
                                   VIDEO_RENDERER_REMOTE);

    /* callback for remote renderer */
    priv->remote_renderer_connection = QObject::connect(
        call,
        &Call::videoStarted,
        [=](Video::Renderer *renderer) {
            video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                           renderer,
                                           VIDEO_RENDERER_REMOTE);
        }
    );

    /* local renderer */
    if (Video::PreviewManager::instance()->isPreviewing())
        video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                       Video::PreviewManager::instance()->previewRenderer(),
                                       VIDEO_RENDERER_LOCAL);

    /* callback for local renderer */
    priv->local_renderer_connection = QObject::connect(
        Video::PreviewManager::instance(),
        &Video::PreviewManager::previewStarted,
        [=](Video::Renderer *renderer) {
            video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                           renderer,
                                           VIDEO_RENDERER_LOCAL);
        }
    );

    return self;
}
