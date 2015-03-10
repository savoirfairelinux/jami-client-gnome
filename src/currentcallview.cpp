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

#include "currentcallview.h"

#include <gtk/gtk.h>
#include <call.h>
#include <callmodel.h>
#include "utils/drawing.h"
#include "video/video_widget.h"
#include <video/previewmanager.h>

struct _CurrentCallView
{
    GtkBox parent;
};

struct _CurrentCallViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _CurrentCallViewPrivate CurrentCallViewPrivate;

struct _CurrentCallViewPrivate
{
    GtkWidget *image_peer;
    GtkWidget *label_identity;
    GtkWidget *label_status;
    GtkWidget *label_duration;
    GtkWidget *frame_video;
    GtkWidget *video_widget;
    GtkWidget *button_hangup;

    Video::Renderer *remote_renderer;

    QMetaObject::Connection state_change_connection;
    QMetaObject::Connection call_details_connection;
    QMetaObject::Connection renderer_connection;
};

G_DEFINE_TYPE_WITH_PRIVATE(CurrentCallView, current_call_view, GTK_TYPE_BOX);

#define CURRENT_CALL_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CURRENT_CALL_VIEW_TYPE, CurrentCallViewPrivate))

static void
current_call_view_dispose(GObject *object)
{
    CurrentCallView *view;
    CurrentCallViewPrivate *priv;

    view = CURRENT_CALL_VIEW(object);
    priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    QObject::disconnect(priv->state_change_connection);
    QObject::disconnect(priv->call_details_connection);
    QObject::disconnect(priv->renderer_connection);

    G_OBJECT_CLASS(current_call_view_parent_class)->dispose(object);
}

static void
current_call_view_init(CurrentCallView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
current_call_view_class_init(CurrentCallViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = current_call_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/currentcallview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, image_peer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_identity);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_status);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_duration);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, frame_video);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, button_hangup);
}

GtkWidget *
current_call_view_new(void)
{
    return (GtkWidget *)g_object_new(CURRENT_CALL_VIEW_TYPE, NULL);
}

static void
update_state(CurrentCallView *view, Call *call)
{
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    /* change state label */
    Call::State state = call->state();

    switch(state) {
        case Call::State::INCOMING:
            gtk_label_set_text(GTK_LABEL(priv->label_status), "Incoming...");
            break;
        case Call::State::RINGING:
            gtk_label_set_text(GTK_LABEL(priv->label_status), "Ringing...");
            break;
        case Call::State::CURRENT:
            /* note: shouldn't be displayed, as the view should change */
            gtk_label_set_text(GTK_LABEL(priv->label_status), "In progress.");
            break;
        case Call::State::DIALING:
            gtk_label_set_text(GTK_LABEL(priv->label_status), "Dialing...");
            break;
        case Call::State::HOLD:
            /* note: shouldn't be displayed, as the view should change */
            gtk_label_set_text(GTK_LABEL(priv->label_status), "On hold.");
            break;
        case Call::State::FAILURE:
            gtk_label_set_text(GTK_LABEL(priv->label_status), "Failed.");
            break;
        case Call::State::BUSY:
            gtk_label_set_text(GTK_LABEL(priv->label_status), "Busy.");
            break;
        case Call::State::TRANSFERRED:
            /* note: shouldn't be displayed, as the view should change */
            gtk_label_set_text(GTK_LABEL(priv->label_status), "Transfered.");
            break;
        case Call::State::TRANSF_HOLD:
            /* note: shouldn't be displayed, as the view should change */
            gtk_label_set_text(GTK_LABEL(priv->label_status), "Transfer hold.");
            break;
        case Call::State::OVER:
            /* note: shouldn't be displayed, as the view should change */
            gtk_label_set_text(GTK_LABEL(priv->label_status), "Over.");
            break;
        case Call::State::ERROR:
            gtk_label_set_text(GTK_LABEL(priv->label_status), "Error.");
            break;
        case Call::State::CONFERENCE:
            /* note: shouldn't be displayed, as the view should change */
            gtk_label_set_text(GTK_LABEL(priv->label_status), "Conference.");
            break;
        case Call::State::CONFERENCE_HOLD:
            /* note: shouldn't be displayed, as the view should change */
            gtk_label_set_text(GTK_LABEL(priv->label_status), "Conference hold.");
            break;
        case Call::State::INITIALIZATION:
            gtk_label_set_text(GTK_LABEL(priv->label_status), "Initialization...");
            break;
        case Call::State::COUNT__:
        break;
    }
}

static void
update_details(CurrentCallView *view, Call *call)
{
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    /* update call duration */
    QByteArray ba_length = call->length().toLocal8Bit();
    gtk_label_set_text(GTK_LABEL(priv->label_duration), ba_length.constData());
}

static gboolean
set_remote_renderer(CurrentCallView *view)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(view), FALSE);

    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    video_widget_set_remote_renderer(VIDEO_WIDGET(priv->video_widget), priv->remote_renderer);

    return FALSE; /* do not call again */
}

void
current_call_view_set_call_info(CurrentCallView *view, const QModelIndex& idx) {
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    /* get image and frame it */
    GdkPixbuf *avatar = ring_draw_fallback_avatar(50);
    GdkPixbuf *framed_avatar = ring_frame_avatar(avatar);
    g_object_unref(avatar);
    gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_peer), framed_avatar);
    g_object_unref(framed_avatar);

    /* get name */
    QVariant var = idx.model()->data(idx, static_cast<int>(Call::Role::Name));
    QByteArray ba_name = var.toString().toLocal8Bit();
    gtk_label_set_text(GTK_LABEL(priv->label_identity), ba_name.constData());

    /* change some things depending on call state */
    Call *call = CallModel::instance()->getCall(idx);

    update_state(view, call);
    update_details(view, call);

    priv->state_change_connection = QObject::connect(
        call,
        &Call::stateChanged,
        [=]() { update_state(view, call); }
    );

    priv->call_details_connection = QObject::connect(
        call,
        static_cast<void (Call::*)(void)>(&Call::changed),
        [=]() { update_details(view, call); }
    );

    /* video widget */
    priv->video_widget = video_widget_new();
    gtk_container_add(GTK_CONTAINER(priv->frame_video), priv->video_widget);
    gtk_widget_show_all(priv->frame_video);

    /* check if we already have a renderer */
    priv->remote_renderer = call->videoRenderer();
    set_remote_renderer(view);

    /* callback for video renderer */
    priv->renderer_connection = QObject::connect(
        call,
        &Call::videoStarted,
        [=](Video::Renderer *renderer) {
            priv->remote_renderer = renderer;

            g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                            (GSourceFunc)set_remote_renderer,
                            view,
                            NULL);
        }
    );

    /* preview renderer */
    video_widget_set_local_renderer(VIDEO_WIDGET(priv->video_widget),
                                    Video::PreviewManager::instance()->previewRenderer());
}
