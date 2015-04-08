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

#include "incomingcallview.h"

#include <gtk/gtk.h>
#include <call.h>
#include "utils/drawing.h"
#include <callmodel.h>

struct _IncomingCallView
{
    GtkBox parent;
};

struct _IncomingCallViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _IncomingCallViewPrivate IncomingCallViewPrivate;

struct _IncomingCallViewPrivate
{
    GtkWidget *image_incoming;
    GtkWidget *label_identity;
    GtkWidget *label_status;
    GtkWidget *button_accept_incoming;
    GtkWidget *button_reject_incoming;
    GtkWidget *button_end_call;

    QMetaObject::Connection state_change_connection;
};

G_DEFINE_TYPE_WITH_PRIVATE(IncomingCallView, incoming_call_view, GTK_TYPE_BOX);

#define INCOMING_CALL_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), INCOMING_CALL_VIEW_TYPE, IncomingCallViewPrivate))

static void
incoming_call_dispose(GObject *object)
{
    IncomingCallView *view;
    IncomingCallViewPrivate *priv;

    view = INCOMING_CALL_VIEW(object);
    priv = INCOMING_CALL_VIEW_GET_PRIVATE(view);

    QObject::disconnect(priv->state_change_connection);

    G_OBJECT_CLASS(incoming_call_view_parent_class)->dispose(object);
}

static void
incoming_call_view_init(IncomingCallView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
incoming_call_view_class_init(IncomingCallViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = incoming_call_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/incomingcallview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, image_incoming);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, label_identity);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, label_status);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, button_accept_incoming);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, button_reject_incoming);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, button_end_call);
}

GtkWidget *
incoming_call_view_new(void)
{
    return (GtkWidget *)g_object_new(INCOMING_CALL_VIEW_TYPE, NULL);
}

static void
update_state(IncomingCallView *view, Call *call)
{
    IncomingCallViewPrivate *priv = INCOMING_CALL_VIEW_GET_PRIVATE(view);

    /* change state label */
    Call::State state = call->state();

    switch(state) {
        case Call::State::NEW:
            gtk_label_set_text(GTK_LABEL(priv->label_status), "New.");
            break;
        case Call::State::ABORTED:
            gtk_label_set_text(GTK_LABEL(priv->label_status), "Aborted.");
            break;
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

    /* change button(s) displayed */
    gtk_widget_hide(priv->button_accept_incoming);
    gtk_widget_hide(priv->button_reject_incoming);
    gtk_widget_hide(priv->button_end_call);

    switch(state) {
        case Call::State::INCOMING:
            gtk_widget_show(priv->button_accept_incoming);
            gtk_widget_show(priv->button_reject_incoming);
            break;
        case Call::State::NEW:
        case Call::State::ABORTED:
        case Call::State::RINGING:
        case Call::State::CURRENT:
        case Call::State::DIALING:
        case Call::State::HOLD:
        case Call::State::FAILURE:
        case Call::State::BUSY:
        case Call::State::TRANSFERRED:
        case Call::State::TRANSF_HOLD:
        case Call::State::OVER:
        case Call::State::ERROR:
        case Call::State::CONFERENCE:
        case Call::State::CONFERENCE_HOLD:
        case Call::State::INITIALIZATION:
            gtk_widget_show(priv->button_end_call);
            break;
        case Call::State::COUNT__:
            break;
    }
}

void
incoming_call_view_set_call_info(IncomingCallView *view, const QModelIndex& idx) {
    IncomingCallViewPrivate *priv = INCOMING_CALL_VIEW_GET_PRIVATE(view);

    /* get image and frame it */
    GdkPixbuf *avatar = ring_draw_fallback_avatar(100);
    GdkPixbuf *framed_avatar = ring_frame_avatar(avatar);
    g_object_unref(avatar);
    gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_incoming), framed_avatar);
    g_object_unref(framed_avatar);

    /* get name */
    QVariant var = idx.model()->data(idx, static_cast<int>(Call::Role::Name));
    QByteArray ba_name = var.toString().toLocal8Bit();
    gtk_label_set_text(GTK_LABEL(priv->label_identity), ba_name.constData());

    /* change some things depending on call state */
    Call *call = CallModel::instance()->getCall(idx);

    update_state(view, call);

    priv->state_change_connection = QObject::connect(
        call,
        &Call::stateChanged,
        [=]() { update_state(view, call); }
    );
}
