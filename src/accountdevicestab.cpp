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

#include "accountdevicestab.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <account.h>
#include <codecmodel.h>
#include "models/gtkqsortfiltertreemodel.h"
#include "utils/models.h"

static constexpr const char* ADD_DEVICE_VIEW_NAME       = "add-device";
static constexpr const char* GENERATED_PIN_VIEW_NAME    = "generated-pin";
static constexpr const char* MANAGE_DEVICES_VIEW_NAME   = "manage-devices";

struct _AccountDevicesTab
{
    GtkBox parent;
};

struct _AccountDevicesTabClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountDevicesTabPrivate AccountDevicesTabPrivate;

struct _AccountDevicesTabPrivate
{
    Account   *account;
    GtkWidget *stack_account_devices;

    /* generated_pin view */
    GtkWidget *generated_pin;
    GtkWidget *label_generated_pin;
    GtkWidget *button_generated_pin_ok;

    /* manage_devices view */
    GtkWidget *manage_devices;
    GtkWidget *button_add_device;

    /* add_device view */
    GtkWidget *add_device;
    GtkWidget *button_generate_pin;
    GtkWidget *button_add_device_cancel;
    GtkWidget *entry_password;

};

G_DEFINE_TYPE_WITH_PRIVATE(AccountDevicesTab, account_devices_tab, GTK_TYPE_BOX);

#define ACCOUNT_DEVICES_TAB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_DEVICES_TAB_TYPE, AccountDevicesTabPrivate))

static void
account_devices_tab_dispose(GObject *object)
{
    G_OBJECT_CLASS(account_devices_tab_parent_class)->dispose(object);
}

static void
account_devices_tab_init(AccountDevicesTab *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
account_devices_tab_class_init(AccountDevicesTabClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_devices_tab_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountdevicestab.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, stack_account_devices);

    /* generated_pin view */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, generated_pin);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, label_generated_pin);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, button_generated_pin_ok);

    /* manage_devices view */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, manage_devices);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, button_add_device);

    /* add_device view */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, add_device);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, button_generate_pin);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, button_add_device_cancel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, entry_password);
}

static void
show_manage_devices_view(AccountDevicesTab *view)
{
    g_return_if_fail(IS_ACCOUNT_DEVICES_TAB(view));
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);
    auto manage_devices = gtk_stack_get_child_by_name(GTK_STACK(priv->stack_account_devices), MANAGE_DEVICES_VIEW_NAME);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_devices), manage_devices);
}

static void
add_device_clicked(G_GNUC_UNUSED GtkButton *button, AccountDevicesTab *view)
{
    g_return_if_fail(IS_ACCOUNT_DEVICES_TAB(view));
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);

    gtk_widget_show_all(priv->add_device);
    if (!gtk_stack_get_child_by_name(GTK_STACK(priv->stack_account_devices), ADD_DEVICE_VIEW_NAME))
    {
        gtk_stack_add_named(GTK_STACK(priv->stack_account_devices), priv->add_device, ADD_DEVICE_VIEW_NAME);
    }
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_devices), priv->add_device);
}

static void
add_device_cancel_clicked(G_GNUC_UNUSED GtkButton *button, AccountDevicesTab *view)
{
    show_manage_devices_view(view);
}

static void
show_generated_pin_view(AccountDevicesTab *view)
{
    g_return_if_fail(IS_ACCOUNT_DEVICES_TAB(view));
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);

    gtk_widget_show_all(priv->generated_pin);
    if (!gtk_stack_get_child_by_name(GTK_STACK(priv->stack_account_devices), GENERATED_PIN_VIEW_NAME))
    {
        gtk_stack_add_named(GTK_STACK(priv->stack_account_devices), priv->generated_pin, GENERATED_PIN_VIEW_NAME);
    }
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_devices), priv->generated_pin);
}

static void
generate_pin_clicked(G_GNUC_UNUSED GtkButton *button, AccountDevicesTab *view)
{
    g_return_if_fail(IS_ACCOUNT_DEVICES_TAB(view));
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);

    auto password = QString(gtk_entry_get_text(GTK_ENTRY(priv->entry_password)));

    // Get the pin
    QString pin = priv->account->deviceInitializationPin(password);

    gtk_label_set_text(GTK_LABEL(priv->label_generated_pin), pin.toStdString().c_str());
    show_generated_pin_view(view);
}

static void
generated_pin_ok_clicked(G_GNUC_UNUSED GtkButton *button, AccountDevicesTab *view)
{
    show_manage_devices_view(view);
}

static void
build_tab_view(AccountDevicesTab *view)
{
    g_return_if_fail(IS_ACCOUNT_DEVICES_TAB(view));
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);

    g_signal_connect(priv->button_add_device, "clicked", G_CALLBACK(add_device_clicked), view);
    g_signal_connect(priv->button_add_device_cancel, "clicked", G_CALLBACK(add_device_cancel_clicked), view);
    g_signal_connect(priv->button_generate_pin, "clicked", G_CALLBACK(generate_pin_clicked), view);
    g_signal_connect(priv->button_generated_pin_ok, "clicked", G_CALLBACK(generated_pin_ok_clicked), view);

    /* Show manage-devices view */
    gtk_stack_add_named(GTK_STACK(priv->stack_account_devices), priv->manage_devices, MANAGE_DEVICES_VIEW_NAME);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_devices), priv->manage_devices);
}

GtkWidget *
account_devices_tab_new(Account *account)
{
    g_return_val_if_fail(account != NULL, NULL);

    gpointer view = g_object_new(ACCOUNT_DEVICES_TAB_TYPE, NULL);

    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);
    priv->account = account;

    build_tab_view(ACCOUNT_DEVICES_TAB(view));

    return (GtkWidget *)view;
}
