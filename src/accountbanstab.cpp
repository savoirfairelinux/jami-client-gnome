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

// GTK+ related
#include <gtk/gtk.h>
#include <glib/gi18n.h>

// LRC
#include <account.h>
//~ #include <codecmodel.h>
//~ #include "ringdevicemodel.h"
//~ #include "ringdevice.h"

// Ring Client
#include "accountbanstab.h"
#include "models/gtkqtreemodel.h"
#include "utils/models.h"

struct _AccountBansTab
{
    GtkBox parent;
};

struct _AccountBansTabClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountBansTabPrivate AccountBansTabPrivate;

struct _AccountBansTabPrivate
{
    Account   *account;
    GtkWidget *treeview_bans;

};

G_DEFINE_TYPE_WITH_PRIVATE(AccountBansTab, account_bans_tab, GTK_TYPE_BOX);

#define ACCOUNT_BANS_TAB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_BANS_TAB_TYPE, AccountBansTabPrivate))

static void
account_bans_tab_dispose(GObject *object)
{
    G_OBJECT_CLASS(account_bans_tab_parent_class)->dispose(object);
}

static void
account_bans_tab_init(AccountBansTab *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
account_bans_tab_class_init(AccountBansTabClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_bans_tab_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountbanstab.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountBansTab, treeview_bans);

}



static void
build_tab_view(AccountBansTab *view)
{
    g_return_if_fail(IS_ACCOUNT_BANS_TAB(view));
    AccountBansTabPrivate *priv = ACCOUNT_BANS_TAB_GET_PRIVATE(view);

    //~ g_signal_connect_swapped(priv->button_add_device, "clicked", G_CALLBACK(show_add_device_view), view);
    //~ g_signal_connect_swapped(priv->button_add_device_cancel, "clicked", G_CALLBACK(show_manage_devices_view), view);
    //~ g_signal_connect(priv->button_export_on_the_ring, "clicked", G_CALLBACK(export_on_the_ring_clicked), view);
    //~ g_signal_connect_swapped(priv->button_generated_pin_ok, "clicked", G_CALLBACK(show_manage_devices_view), view);
    //~ g_signal_connect_swapped(priv->button_export_on_ring_error_ok, "clicked", G_CALLBACK(show_add_device_view), view);

    //~ gtk_label_set_text(GTK_LABEL(priv->label_device_id), priv->account->deviceId().toUtf8().constData());

    /* treeview_devices */
    //~ auto *devices_model = gtk_q_tree_model_new(
        //~ (QAbstractItemModel*) priv->account->ringDeviceModel(),
        //~ 2,
        //~ RingDevice::Column::Name, Qt::DisplayRole, G_TYPE_STRING,
        //~ RingDevice::Column::Id, Qt::DisplayRole, G_TYPE_STRING);

    //~ gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_devices), GTK_TREE_MODEL(devices_model));

    //~ GtkCellRenderer* renderer;
    //~ GtkTreeViewColumn* column;

    //~ renderer = gtk_cell_renderer_text_new();
    //~ column = gtk_tree_view_column_new_with_attributes(C_("Device Name Column", "Name"), renderer, "text", 0, NULL);
    //~ gtk_tree_view_column_set_expand(column, TRUE);
    //~ gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_devices), column);

    //~ renderer = gtk_cell_renderer_text_new();
    //~ column = gtk_tree_view_column_new_with_attributes(C_("Device ID Column", "ID"), renderer, "text", 1, NULL);
    //~ gtk_tree_view_column_set_expand(column, TRUE);
    //~ gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_devices), column);

    /* Show manage-devices view */
    //~ gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_devices), priv->manage_devices);
}

GtkWidget *
account_bans_tab_new(Account *account)
{
    g_return_val_if_fail(account != NULL, NULL);

    gpointer view = g_object_new(ACCOUNT_BANS_TAB_TYPE, NULL);

    AccountBansTabPrivate *priv = ACCOUNT_BANS_TAB_GET_PRIVATE(view);
    priv->account = account;

    build_tab_view(ACCOUNT_BANS_TAB(view));

    return (GtkWidget *)view;
}
