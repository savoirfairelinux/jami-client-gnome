/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
 *  Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>
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
#include "pendingcontactrequests.h"
#include "models/gtkqtreemodel.h"
#include "native/pixbufmanipulator.h"
#include "utils/accounts.h"

// LRC
#include <recentmodel.h>
#include <accountmodel.h>
#include <pendingcontactrequestmodel.h>
#include <account.h>
#include <availableaccountmodel.h>

// System
#include <gtk/gtk.h>
#include <glib/gi18n.h>

/**
 * gtk structure
 */
struct _PendingContactRequestsView
{
    GtkTreeView parent;
};

/**
 * gtk class structure
 */
struct _PendingContactRequestsViewClass
{
    GtkTreeViewClass parent_class;
};

typedef struct _PendingContactRequestsViewPrivate PendingContactRequestsViewPrivate;

/**
 * gtk private structure
 */
struct _PendingContactRequestsViewPrivate
{
    GtkWidget *treeview_pending_contact_request_list;
    QSortFilterProxyModel* myProxy;
};

G_DEFINE_TYPE_WITH_PRIVATE(PendingContactRequestsView, pending_contact_requests_view, GTK_TYPE_TREE_VIEW);

#define PENDING_CONTACT_REQUESTS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), PENDING_CONTACT_REQUESTS_VIEW_TYPE, PendingContactRequestsViewPrivate))

/**
 * bind Account::pendingContactRequestModel() to pending_contact_requests_model
 */
static void
bind_models(PendingContactRequestsView *self, Account* account)
{
    if (not account) {
        g_warning("invalid account, cannot bind models.");
        return;
    }

    auto pending_contact_requests_model = gtk_q_tree_model_new(account->pendingContactRequestModel(),
                                                          1/*nmbr. of cols.*/,
                                                          0,
                                                          Qt::DisplayRole,
                                                          G_TYPE_STRING);

    gtk_tree_view_set_model(GTK_TREE_VIEW(self), GTK_TREE_MODEL(pending_contact_requests_model));
}

/**
 * gtk init function
 */
static void
pending_contact_requests_view_init(PendingContactRequestsView *self)
{
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(self), FALSE);
    gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(self), FALSE);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(self), FALSE);

    // the next signal is used to set the model in function of the selection of the account
    QObject::connect(AvailableAccountModel::instance().selectionModel(), &QItemSelectionModel::currentChanged, [self](const QModelIndex& idx){
        bind_models(self, idx.data(static_cast<int>(Account::Role::Object)).value<Account*>());
    });

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(C_("Account alias (name) column", "Alias"), renderer, "text", 0, NULL);

    /* layout */
    gtk_tree_view_append_column(GTK_TREE_VIEW(self), column);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_expand_all(GTK_TREE_VIEW(self));

    /* init the model */
    bind_models(self, get_active_ring_account());

    gtk_widget_show_all(GTK_WIDGET(self));
}

/**
 * gtk dispose function
 */
static void
pending_contact_requests_view_dispose(GObject *object)
{
    G_OBJECT_CLASS(pending_contact_requests_view_parent_class)->dispose(object);
}

/**
 * gtk finalize function
 */
static void
pending_contact_requests_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(pending_contact_requests_view_parent_class)->finalize(object);
}

/**
 * gtk class init function
 */
static void
pending_contact_requests_view_class_init(PendingContactRequestsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = pending_contact_requests_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = pending_contact_requests_view_dispose;
}

/**
 * gtk new function
 */
GtkWidget *
pending_contact_requests_view_new()
{
    gpointer self = g_object_new(PENDING_CONTACT_REQUESTS_VIEW_TYPE, NULL);

    return (GtkWidget *)self;
}
