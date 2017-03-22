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

#include "contactrequestcontentview.h"

// System
#include <gtk/gtk.h>

// LRC
#include <certificate.h>
#include <contactrequest.h>
#include <accountmodel.h>
#include <pendingcontactrequestmodel.h>

/**
 * gtk structure
 */
struct _ContactRequestContentView
{
    GtkBox parent;
};

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
};

G_DEFINE_TYPE_WITH_PRIVATE(ContactRequestContentView, contact_request_content_view, GTK_TYPE_BOX);

#define CONTACT_REQUEST_CONTENT_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CONTACT_REQUEST_CONTENT_VIEW_TYPE, ContactRequestContentViewPrivate))

/**
 * signals
 */

enum {
    HIDE_VIEW_CLICKED,
    LAST_SIGNAL
};

static guint contact_request_content_view_signals[LAST_SIGNAL] = { 0 };

/**
 * gtk dispose function
 */
static void
contact_request_content_view_dispose(GObject *object)
{
    ContactRequestContentView *self;
    ContactRequestContentViewPrivate *priv;

    self = CONTACT_REQUEST_CONTENT_VIEW(object);
    priv = CONTACT_REQUEST_CONTENT_VIEW_GET_PRIVATE(self);

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
 * gtk clicked callback to ignore contact request
 */
static void
button_close_contact_request_content_view_clicked(G_GNUC_UNUSED GtkWidget *widget, ContactRequestContentView *self)
{
    g_signal_emit(G_OBJECT(self), contact_request_content_view_signals[HIDE_VIEW_CLICKED], 0);
}

/**
 * gtk init function
 */
static void
contact_request_content_view_init(ContactRequestContentView *self)
{
    gtk_widget_init_template(GTK_WIDGET(self));

    ContactRequestContentViewPrivate *priv = CONTACT_REQUEST_CONTENT_VIEW_GET_PRIVATE(self);

    g_signal_connect(priv->button_ignore_contact_request, "clicked", G_CALLBACK(button_ignore_contact_request_clicked), self);
    g_signal_connect(priv->button_accept_contact_request, "clicked", G_CALLBACK(button_accept_contact_request_clicked), self);
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ContactRequestContentView, button_close_contact_request_content_view);

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
    ContactRequestContentViewPrivate *priv = CONTACT_REQUEST_CONTENT_VIEW_GET_PRIVATE(self);
    priv->contactRequest = contact_request;

    gtk_label_set_text(GTK_LABEL(priv->label_peer), contact_request->certificate()->remoteId().toStdString().c_str());
}

/**
 * gtk new function
 */
GtkWidget*
contact_request_content_view_new(ContactRequest *contact_request)
{
    gpointer self = g_object_new(CONTACT_REQUEST_CONTENT_VIEW_TYPE, NULL);
    ContactRequestContentViewPrivate *priv = CONTACT_REQUEST_CONTENT_VIEW_GET_PRIVATE(self);

    build_contact_request_content(CONTACT_REQUEST_CONTENT_VIEW(self), contact_request);

    return (GtkWidget*)self;
}
