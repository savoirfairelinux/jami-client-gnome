/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
 *  Author: Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>
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
 */

// Client
#include "contactrequestcontentview.h"
#include "native/pixbufmanipulator.h"

// System
#include <gtk/gtk.h>

// LRC
#include <accountmodel.h>
#include <certificate.h>
#include <contactmethod.h>
#include <contactrequest.h>
#include <globalinstances.h>
#include <pendingcontactrequestmodel.h>
#include <person.h>

/**
 * gtk structure
 */
struct _ContactRequestContentView
{
    GtkBox parent;
};

/**
 * signals
 */

enum {
    HIDE_VIEW_CLICKED,
    LAST_SIGNAL
};

guint contact_request_content_view_signals[LAST_SIGNAL];

/**
 * gtk class structure
 */
struct _ContactRequestContentViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _ContactRequestContentViewPrivate ContactRequestContentViewPrivate;

/**
 * gtk private structure
 */
struct _ContactRequestContentViewPrivate
{
    ContactRequest* contactRequest;
    Account*      account;

    GtkWidget*    label_peer;
    GtkWidget*    button_ignore_contact_request;
    GtkWidget*    button_accept_contact_request;
    GtkWidget*    button_close_contact_request_content_view;
    GtkWidget*    button_block_contact_request;
    GtkWidget*    image_peer;
    GtkWidget*    label_bestId;
};

G_DEFINE_TYPE_WITH_PRIVATE(ContactRequestContentView, contact_request_content_view, GTK_TYPE_BOX);

#define CONTACT_REQUEST_CONTENT_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CONTACT_REQUEST_CONTENT_VIEW_TYPE, ContactRequestContentViewPrivate))

/**
 * gtk dispose function
 */
static void
contact_request_content_view_dispose(GObject *object)
{
    G_OBJECT_CLASS(contact_request_content_view_parent_class)->dispose(object);
}

/**
 * gtk clicked callback to ignore contact request
 */
static void
button_ignore_contact_request_clicked(G_GNUC_UNUSED GtkWidget *widget, ContactRequestContentView *self)
{
    auto priv = CONTACT_REQUEST_CONTENT_VIEW_GET_PRIVATE(self);

    priv->contactRequest->discard();

}

/**
 * gtk clicked callback to accept contact request
 */
static void
button_accept_contact_request_clicked(G_GNUC_UNUSED GtkWidget *widget, ContactRequestContentView *self)
{
    auto priv = CONTACT_REQUEST_CONTENT_VIEW_GET_PRIVATE(self);

    priv->contactRequest->accept();
}

/**
 * gtk clicked callback to close the view
 */
static void
button_close_contact_request_content_view_clicked(G_GNUC_UNUSED GtkWidget *widget, ContactRequestContentView *self)
{
    g_signal_emit(G_OBJECT(self), contact_request_content_view_signals[HIDE_VIEW_CLICKED], 0);
}

/**
 * gtk clicked callback to block contact request
 */
static void
button_block_contact_request_clicked(G_GNUC_UNUSED GtkWidget *widget, ContactRequestContentView *self)
{
    auto priv = CONTACT_REQUEST_CONTENT_VIEW_GET_PRIVATE(self);

    priv->contactRequest->block();
}

/**
 * gtk init function
 */
static void
contact_request_content_view_init(ContactRequestContentView *self)
{
    gtk_widget_init_template(GTK_WIDGET(self));

    auto priv = CONTACT_REQUEST_CONTENT_VIEW_GET_PRIVATE(self);

    g_signal_connect(priv->button_ignore_contact_request, "clicked", G_CALLBACK(button_ignore_contact_request_clicked), self);
    g_signal_connect(priv->button_accept_contact_request, "clicked", G_CALLBACK(button_accept_contact_request_clicked), self);
    g_signal_connect(priv->button_block_contact_request, "clicked", G_CALLBACK(button_block_contact_request_clicked), self);
    g_signal_connect(priv->button_close_contact_request_content_view, "clicked", G_CALLBACK(button_close_contact_request_content_view_clicked), self);
}

static void
contact_request_content_view_class_init(ContactRequestContentViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = contact_request_content_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/contactrequestcontentview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ContactRequestContentView, label_peer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ContactRequestContentView, button_ignore_contact_request);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ContactRequestContentView, button_accept_contact_request);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ContactRequestContentView, button_block_contact_request);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ContactRequestContentView, button_close_contact_request_content_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ContactRequestContentView, image_peer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ContactRequestContentView, label_bestId);

    contact_request_content_view_signals[HIDE_VIEW_CLICKED] = g_signal_new (
        "hide-view-clicked",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

}

/**
 * gtk build function
 */
void
build_contact_request_content(ContactRequestContentView *self, ContactRequest *contact_request)
{
    auto priv = CONTACT_REQUEST_CONTENT_VIEW_GET_PRIVATE(self);
    priv->contactRequest = contact_request;
    auto person = contact_request->peer();

    /* get photo */
    QVariant photo = GlobalInstances::pixmapManipulator().contactPhoto(person, QSize(60, 60), false);
    std::shared_ptr<GdkPixbuf> image = photo.value<std::shared_ptr<GdkPixbuf>>();
    gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_peer), image.get());

    /* get name */
    auto name_std = person->formattedName().toStdString();
    gtk_label_set_text(GTK_LABEL(priv->label_peer), name_std.c_str());

    /* get contact best id, if different from name */
    auto contactId_std = person->phoneNumbers()[0]->getBestId().toStdString();
    if (name_std != contactId_std) {
        gtk_label_set_text(GTK_LABEL(priv->label_bestId), contactId_std.c_str());
        gtk_widget_show(priv->label_bestId);
    }
}

/**
 * gtk new function
 */
GtkWidget*
contact_request_content_view_new(ContactRequest *contact_request)
{
    auto self = GTK_WIDGET(g_object_new(CONTACT_REQUEST_CONTENT_VIEW_TYPE, nullptr));

    build_contact_request_content(CONTACT_REQUEST_CONTENT_VIEW(self), contact_request);

    return self;
}
