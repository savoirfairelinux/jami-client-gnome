/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
 *  Author: Alexandre Viau <alexandre.viau@savoirfairelinux.com>
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

#include "accountadddeviceview.h"

#include <gtk/gtk.h>
#include <account.h>
#include <accountmodel.h>

#include <glib/gi18n.h>

struct _AccountAddDeviceView
{
    GtkBox parent;
};

struct _AccountAddDeviceViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountAddDeviceViewPrivate AccountAddDeviceViewPrivate;

struct _AccountAddDeviceViewPrivate
{
    Account   *account;
    GtkWidget *button_generate_pin;
    GtkWidget *button_cancel;
    GtkWidget *entry_password;
    GtkWidget *label_pin;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountAddDeviceView, account_adddevice_view, GTK_TYPE_BOX);

#define ACCOUNT_ADDDEVICE_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_ADDDEVICE_VIEW_TYPE, AccountAddDeviceViewPrivate))

/* signals */
enum {
    ADD_DEVICE_CANCELED,
    LAST_SIGNAL
};
static guint account_adddevice_view_signals[LAST_SIGNAL] = { 0 };

static void
account_adddevice_view_dispose(GObject *object)
{
    G_OBJECT_CLASS(account_adddevice_view_parent_class)->dispose(object);
}

static void
account_adddevice_view_init(AccountAddDeviceView *self)
{
    gtk_widget_init_template(GTK_WIDGET(self));
}

static void
account_adddevice_view_class_init(AccountAddDeviceViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_adddevice_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountadddeviceview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAddDeviceView, button_generate_pin);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAddDeviceView, button_cancel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAddDeviceView, entry_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAddDeviceView, label_pin);

    /* add signals */
    account_adddevice_view_signals[ADD_DEVICE_CANCELED] = g_signal_new("add-device-canceled",
                 G_TYPE_FROM_CLASS(klass),
                 (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
                 0,
                 nullptr,
                 nullptr,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
}

static void
cancel_clicked(AccountAddDeviceView *view)
{
    g_signal_emit(G_OBJECT(view), account_adddevice_view_signals[ADD_DEVICE_CANCELED], 0);
}

static void
generate_pin_clicked(AccountAddDeviceView *view)
{
    g_return_if_fail(IS_ACCOUNT_ADDDEVICE_VIEW(view));
    AccountAddDeviceViewPrivate *priv = ACCOUNT_ADDDEVICE_VIEW_GET_PRIVATE(view);

    auto password = QString(gtk_entry_get_text(GTK_ENTRY(priv->entry_password)));

    // Get the pin
    QString pin = priv->account->deviceInitializationPin(password);
    gtk_label_set_text(GTK_LABEL(priv->label_pin), pin.toStdString().c_str());
}

static void
build_adddevice_view(AccountAddDeviceView *view)
{
    g_return_if_fail(IS_ACCOUNT_ADDDEVICE_VIEW(view));
    AccountAddDeviceViewPrivate *priv = ACCOUNT_ADDDEVICE_VIEW_GET_PRIVATE(view);

    g_signal_connect_swapped(priv->button_cancel, "clicked", G_CALLBACK(cancel_clicked), view);
    g_signal_connect_swapped(priv->button_generate_pin, "clicked", G_CALLBACK(generate_pin_clicked), view);
}

GtkWidget *
account_adddevice_view_new(Account *account)
{
    gpointer view = g_object_new(ACCOUNT_ADDDEVICE_VIEW_TYPE, NULL);

    AccountAddDeviceViewPrivate *priv = ACCOUNT_ADDDEVICE_VIEW_GET_PRIVATE(view);
    priv->account = account;

    build_adddevice_view(ACCOUNT_ADDDEVICE_VIEW(view));

    return (GtkWidget *)view;
}
