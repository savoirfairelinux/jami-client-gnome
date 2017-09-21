/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
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

// GTK+ related
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <QFile>

// LRC
#include <account.h>
#include <codecmodel.h>
#include "ringdevicemodel.h"
#include "ringdevice.h"

// Ring Client
#include "accountdevicestab.h"
#include "models/gtkqtreemodel.h"
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
    QMetaObject::Connection export_on_ring_ended;

    /* generated_pin view */
    GtkWidget *generated_pin;
    GtkWidget *label_generated_pin;
    GtkWidget *button_generated_pin_ok;

    /* manage_devices view */
    GtkWidget *manage_devices;
    GtkWidget *label_device_id;
    GtkWidget *treeview_devices;
    GtkWidget *button_add_device;

    /* add_device view */
    GtkWidget *add_device;
    GtkWidget *entry_export_account_archive;
    GtkWidget *button_export_on_the_ring;
    GtkWidget *button_validate_export;
    GtkWidget *button_add_device_cancel;
    GtkWidget *entry_password;

    /* generating account spinner */
    GtkWidget *vbox_generating_pin_spinner;

    /* export on ring error */
    GtkWidget *export_on_ring_error;
    GtkWidget *label_export_on_ring_error;
    GtkWidget* button_export_on_ring_error_ok;

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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, label_device_id);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, treeview_devices);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, button_add_device);

    /* add_device view */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, add_device);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, entry_export_account_archive);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, button_validate_export);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, button_export_on_the_ring);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, button_add_device_cancel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, entry_password);

    /* generating pin spinner */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, vbox_generating_pin_spinner);

    /* export on ring error */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, export_on_ring_error);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, label_export_on_ring_error);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountDevicesTab, button_export_on_ring_error_ok);
}

static void
show_manage_devices_view(AccountDevicesTab *view)
{
    g_return_if_fail(IS_ACCOUNT_DEVICES_TAB(view));
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_devices), priv->manage_devices);
}

static void
show_add_device_view(AccountDevicesTab *view)
{
    g_return_if_fail(IS_ACCOUNT_DEVICES_TAB(view));
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_devices), priv->add_device);
}

static void
show_generated_pin_view(AccountDevicesTab *view)
{
    g_return_if_fail(IS_ACCOUNT_DEVICES_TAB(view));
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);
    gtk_widget_show(priv->generated_pin);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_devices), priv->generated_pin);
}

static void
show_generating_pin_spinner(AccountDevicesTab *view)
{
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_devices), priv->vbox_generating_pin_spinner);
}

static void
show_export_on_ring_error(AccountDevicesTab *view)
{
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_devices), priv->export_on_ring_error);
}

static void
export_on_the_ring_clicked(G_GNUC_UNUSED GtkButton *button, AccountDevicesTab *view)
{
    g_return_if_fail(IS_ACCOUNT_DEVICES_TAB(view));
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);

    auto password = QString(gtk_entry_get_text(GTK_ENTRY(priv->entry_password)));
    gtk_entry_set_text(GTK_ENTRY(priv->entry_password), "");

    priv->export_on_ring_ended = QObject::connect(
        priv->account,
        &Account::exportOnRingEnded,
        [=] (Account::ExportOnRingStatus status, QString pin) {
            QObject::disconnect(priv->export_on_ring_ended);
            switch (status)
            {
                case Account::ExportOnRingStatus::SUCCESS:
                {
                    gtk_label_set_text(GTK_LABEL(priv->label_generated_pin), pin.toStdString().c_str());
                    show_generated_pin_view(view);
                    break;
                }
                case Account::ExportOnRingStatus::WRONG_PASSWORD:
                {
                    gtk_label_set_text(GTK_LABEL(priv->label_export_on_ring_error), _("Bad password"));
                    show_export_on_ring_error(view);
                    break;
                }
                case Account::ExportOnRingStatus::NETWORK_ERROR:
                {
                    gtk_label_set_text(GTK_LABEL(priv->label_export_on_ring_error), _("Network error, try again"));
                    show_export_on_ring_error(view);
                    break;
                }
            }
        }
    );

    show_generating_pin_spinner(view);
    if (!priv->account->exportOnRing(password))
    {
        QObject::disconnect(priv->export_on_ring_ended);
        gtk_label_set_text(GTK_LABEL(priv->label_export_on_ring_error), _("Could not initiate export to the Ring, try again"));
        g_debug("Could not initiate exportOnRing operation");
        show_export_on_ring_error(view);
    }
}

#include <iostream>

static void
copy_account_archive(GtkButton *button, AccountDevicesTab *view)
{
    g_return_if_fail(IS_ACCOUNT_DEVICES_TAB(view));
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);
    auto directory = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(priv->entry_export_account_archive));
    if (directory) {
        auto from = priv->account->archivePath().toStdString();
        auto destination = std::string(directory);
        destination += "/" + priv->account->id().toStdString() + ".gz";
        std::cout << from << ";" << destination << std::endl;
        // TODO use std::copy
        if (QFile::exists(from.c_str())) {
            QFile::copy(from.c_str(), destination.c_str());
            gtk_button_set_label(button, _("Copied!"));
        }
    }
}

static void
build_tab_view(AccountDevicesTab *view)
{
    g_return_if_fail(IS_ACCOUNT_DEVICES_TAB(view));
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);

    auto *chooser = GTK_FILE_CHOOSER(priv->entry_export_account_archive);
    gtk_file_chooser_set_action(chooser, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);

    g_signal_connect_swapped(priv->button_add_device, "clicked", G_CALLBACK(show_add_device_view), view);
    g_signal_connect_swapped(priv->button_add_device_cancel, "clicked", G_CALLBACK(show_manage_devices_view), view);
    g_signal_connect(priv->button_validate_export, "clicked", G_CALLBACK(copy_account_archive), view);
    g_signal_connect(priv->button_export_on_the_ring, "clicked", G_CALLBACK(export_on_the_ring_clicked), view);
    g_signal_connect_swapped(priv->button_generated_pin_ok, "clicked", G_CALLBACK(show_manage_devices_view), view);
    g_signal_connect_swapped(priv->button_export_on_ring_error_ok, "clicked", G_CALLBACK(show_add_device_view), view);

    gtk_label_set_text(GTK_LABEL(priv->label_device_id), priv->account->deviceId().toUtf8().constData());

    /* treeview_devices */
    auto *devices_model = gtk_q_tree_model_new(
        (QAbstractItemModel*) priv->account->ringDeviceModel(),
        2,
        RingDevice::Column::Name, Qt::DisplayRole, G_TYPE_STRING,
        RingDevice::Column::Id, Qt::DisplayRole, G_TYPE_STRING);

    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_devices), GTK_TREE_MODEL(devices_model));

    GtkCellRenderer* renderer;
    GtkTreeViewColumn* column;

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(C_("Device Name Column", "Name"), renderer, "text", 0, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_devices), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(C_("Device ID Column", "ID"), renderer, "text", 1, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_devices), column);

    /* Show manage-devices view */
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
