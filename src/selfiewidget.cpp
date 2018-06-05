/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
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
#include "selfiewidget.h"
#include "selfieview.h"

struct _SelfieWidget
{
    GtkWindow parent;
};

struct _SelfieWidgetClass
{
    GtkWindowClass parent_class;
};

typedef struct _SelfieWidgetPrivate SelfieWidgetPrivate;

struct _SelfieWidgetPrivate
{
    GtkWidget *selfie_view;
};

/* signals */
enum {
    RESPONSE,
    LAST_SIGNAL
};

static guint selfie_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE(SelfieWidget, selfie_widget, GTK_TYPE_WINDOW);
#define SELFIE_WIDGET_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SELFIE_WIDGET_TYPE, SelfieWidgetPrivate))

static void
selfie_widget_dispose(GObject *object)
{
    SelfieWidgetPrivate *priv = SELFIE_WIDGET_GET_PRIVATE(object);
    G_OBJECT_CLASS(selfie_widget_parent_class)->dispose(object);
}

static void
selfie_widget_finalize(GObject *object)
{
    G_OBJECT_CLASS(selfie_widget_parent_class)->finalize(object);
}

GtkWidget*
selfie_widget_new(void)
{
    return (GtkWidget *) g_object_new(SELFIE_WIDGET_TYPE, NULL);
}

static void
selfie_widget_class_init(SelfieWidgetClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = selfie_widget_finalize;
    G_OBJECT_CLASS(klass)->dispose = selfie_widget_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass), "/cx/ring/RingGnome/selfiewidget.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieWidget, selfie_view);

    // Define custom signals
    selfie_signals[RESPONSE] = g_signal_new ("response",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);
}

static void
on_send_signal(SelfieWidget *self) {
    g_signal_emit(self, selfie_signals[RESPONSE], 0);
}

static void
on_quit_signal(SelfieWidget *self) {
    gtk_widget_destroy(GTK_WIDGET(self));
}

static void
selfie_widget_init(SelfieWidget *self)
{
    SelfieWidgetPrivate *priv = SELFIE_WIDGET_GET_PRIVATE(self);
    gtk_widget_init_template(GTK_WIDGET(self));

    g_signal_connect_swapped(priv->selfie_view, "quit_action", G_CALLBACK(on_quit_signal), self);
    g_signal_connect_swapped(priv->selfie_view, "send_action", G_CALLBACK(on_send_signal), self);

    gtk_window_set_title(GTK_WINDOW(self), "Record a message");
    gtk_window_set_modal(GTK_WINDOW(self), TRUE);
}

void
selfie_widget_set_peer_name(SelfieWidget *self, std::string name)
{
    SelfieWidgetPrivate *priv = SELFIE_WIDGET_GET_PRIVATE(self);
    std::string title = "Record a message for " + name;
    gtk_window_set_title(GTK_WINDOW(self), title.c_str());
}

const char *
selfie_widget_get_filename(SelfieWidget *self)
{
    SelfieWidgetPrivate *priv = SELFIE_WIDGET_GET_PRIVATE(self);
    return selfie_view_get_save_file(SELFIE_VIEW(priv->selfie_view));
}
