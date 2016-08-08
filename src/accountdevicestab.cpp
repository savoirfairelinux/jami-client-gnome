
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
#include "accountadddeviceview.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <account.h>
#include <codecmodel.h>
#include "models/gtkqsortfiltertreemodel.h"
#include "utils/models.h"

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
}

static gboolean
generate_pin_dialog(GtkWidget *widget)
{
    gboolean response = FALSE;
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
                            (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                            GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                            _("To generate a pin, enter your password:\n"
                            "Note: To add a new device, it must have internet access. Clicking on the "
                            "button below will generate a pin that will be valid for 5 minutes. "
                            "It must be entered on your new device within that delay."));

    gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Generate pin", GTK_RESPONSE_OK);

    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);

    /* get parent window so we can center on it */
    GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(widget));
    if (gtk_widget_is_toplevel(parent)) {
        gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    }

    switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
        case GTK_RESPONSE_OK:
            response = TRUE;
            break;
        default:
            response = FALSE;
            break;
    }

    gtk_widget_destroy(dialog);

    return response;
}

static void
pin_dialog(GtkWidget *widget, const gchar *pin)
{
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
                            (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                            GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK,
                            _("Your pin is: %s.\n It must be entered on your new device within"
                               " 5 minutes."),
                            pin);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);

    /* get parent window so we can center on it */
    GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(widget));
    if (gtk_widget_is_toplevel(parent)) {
        gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    }

    gtk_dialog_run(GTK_DIALOG(dialog));

    gtk_widget_destroy(dialog);
}

static void
close_add_device_view(AccountDevicesTab *view)
{
    g_return_if_fail(IS_ACCOUNT_DEVICES_TAB(view));
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);

    auto adddevice_view = gtk_stack_get_child_by_name(GTK_STACK(priv->stack_account_devices), "add-device");
    if (adddevice_view)
    {
        gtk_container_remove(GTK_CONTAINER(priv->stack_account_devices), adddevice_view);
    }
}

static void
add_device_clicked(G_GNUC_UNUSED GtkButton *button, AccountDevicesTab *view)
{
    g_return_if_fail(IS_ACCOUNT_DEVICES_TAB(view));
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);

    auto old_view = gtk_stack_get_visible_child(GTK_STACK(priv->stack_account_devices));

    /* Create the adddevice box */
    GtkWidget *box_devices = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    /* Add the adddevice view */
    GtkWidget *adddevice_view = account_adddevice_view_new(priv->account);
    gtk_container_add(GTK_CONTAINER(box_devices), adddevice_view);

    /* Connect signals */
    g_signal_connect_swapped(adddevice_view, "add-device-canceled", G_CALLBACK(close_add_device_view), view);

    /* Show the box */
    gtk_widget_show_all(box_devices);
    gtk_stack_add_named(GTK_STACK(priv->stack_account_devices), box_devices, "add-device");
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_devices), box_devices);
}

static void
build_tab_view(AccountDevicesTab *view)
{
    g_return_if_fail(IS_ACCOUNT_DEVICES_TAB(view));
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);

    /* Create the devices list box */
    GtkWidget *box_devices = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    /* Add device button */
    GtkWidget *button_add_device = gtk_button_new_with_label("Add device");
    g_signal_connect(button_add_device, "clicked", G_CALLBACK(add_device_clicked), view);
    gtk_container_add(GTK_CONTAINER(box_devices), button_add_device);

    /* Show the box */
    gtk_stack_add_named(GTK_STACK(priv->stack_account_devices), box_devices, "devices");
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_devices), box_devices);
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
