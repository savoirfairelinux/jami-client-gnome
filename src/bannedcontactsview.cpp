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

#include "bannedcontactsview.h"

// GTK
#include <gtk/gtk.h>

// Ring Client
#include "models/gtkqtreemodel.h"
#include "native/pixbufmanipulator.h"

// Std
#include <memory>

// LRC
#include <accountmodel.h>
#include <bannedcontactmodel.h>
#include <globalinstances.h>
#include <personmodel.h>

// Qt
#include <QItemSelectionModel>
#include <QSize>

/**
 * gtk structure
 */
struct _BannedContactsView
{
    GtkTreeView parent;
};

/**
 * gtk class structure
 */
struct _BannedContactsViewClass
{
    GtkTreeViewClass parent_class;
};

typedef struct _BannedContactsViewPrivate BannedContactsViewPrivate;

/**
 * gtk private structure
 */
struct _BannedContactsViewPrivate
{
};

G_DEFINE_TYPE_WITH_PRIVATE(BannedContactsView, banned_contacts_view, GTK_TYPE_TREE_VIEW);

#define BANNED_CONTACTS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), BANNED_CONTACTS_VIEW_TYPE, BannedContactsViewPrivate))

/**
 * callback function to render the peer id
 */
static void
render_peer_id(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
               GtkCellRenderer *cell,
               GtkTreeModel *model,
               GtkTreeIter *iter,
               G_GNUC_UNUSED GtkTreeView *treeview)
{
    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);

    auto uri_qt = idx.data(Qt::DisplayRole).toString();
    auto uri_std = uri_qt.toStdString();

    g_object_set(G_OBJECT(cell), "markup", uri_std.c_str(), NULL);
}

/**
 * gtk init function
 */
static void
banned_contacts_view_init(BannedContactsView *self)
{
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(self), FALSE);
    /* no need to show the expander since it will always be expanded */
    gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(self), FALSE);
    /* disable default search otherwise the search steals input focus on key presses */
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(self), FALSE);

    const auto idx_account = AccountModel::instance().selectionModel()->currentIndex();
    auto account = idx_account.data(static_cast<int>(Account::Role::Object)).value<Account*>();

    if(not account)
        return;

    GtkQTreeModel *recent_model = gtk_q_tree_model_new(
        account->bannedContactModel(),
        1,
        BannedContactModel::Columns::PEER_ID,
        Qt::DisplayRole, G_TYPE_STRING);

    gtk_tree_view_set_model(GTK_TREE_VIEW(self),
                            GTK_TREE_MODEL(recent_model));

    /* peer id renderer */
    GtkCellArea *area = gtk_cell_area_box_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_area(area);
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_peer_id,
        self,
        NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(self), column);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_expand_all(GTK_TREE_VIEW(self));

    gtk_widget_show_all(GTK_WIDGET(self));
}

/**
 * gtk dispose function
 */
static void
banned_contacts_view_dispose(GObject *object)
{
    G_OBJECT_CLASS(banned_contacts_view_parent_class)->dispose(object);
}

/**
 * gtk finalize function
 */
static void
banned_contacts_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(banned_contacts_view_parent_class)->finalize(object);
}

/**
 * gtk class init function
 */
static void
banned_contacts_view_class_init(BannedContactsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = banned_contacts_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = banned_contacts_view_dispose;
}

/**
 * gtk new function
 */
GtkWidget *
banned_contacts_view_new()
{
    gpointer self = g_object_new(BANNED_CONTACTS_VIEW_TYPE, NULL);

    return (GtkWidget *)self;
}
