/*
 *  Copyright (C) 2017-2018 Savoir-faire Linux Inc.
 *  Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>
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

#include "bannedcontactsview.h"

// GTK
#include <gtk/gtk.h>

// Qt
#include <QMetaObject>

// Lrc
#include <api/contactmodel.h>

struct _BannedContactsView
{
    GtkTreeView parent;
};

struct _BannedContactsViewClass
{
    GtkTreeViewClass parent_class;
};

typedef struct _BannedContactsViewPrivate BannedContactsViewPrivate;

struct _BannedContactsViewPrivate
{
    AccountInfoPointer const *accountInfo_ = nullptr;

    bool built = false;

    QMetaObject::Connection contactAddedConnection_;
    QMetaObject::Connection contactRemovedConnection_;
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
    gchar *uri;

    gtk_tree_model_get (model, iter,
                        0 /* col# */, &uri /* data */,
                        -1);

    g_object_set(G_OBJECT(cell), "markup", uri, NULL);
    g_free(uri);
}

void
remove_from_list(BannedContactsView *self, const std::string& contactUri) {
    auto priv = BANNED_CONTACTS_VIEW_GET_PRIVATE(self);
    auto model = gtk_tree_view_get_model (GTK_TREE_VIEW(self));

    if (!priv->built) return;

    auto idx = 0;
    auto iterIsCorrect = true;
    GtkTreeIter iter;

    while (iterIsCorrect) {
        iterIsCorrect = gtk_tree_model_iter_nth_child (model, &iter, nullptr, idx);
        if (!iterIsCorrect)
            break;
        gchar *uri;
        gtk_tree_model_get (model, &iter,
                            0 /* col# */, &uri /* data */,
                            -1);
        if (std::string(uri) == contactUri) {
            g_debug("bannedcontactsview::remove_from_list Removing unbanned contact from list");
            gtk_list_store_remove (GTK_LIST_STORE(model), &iter);
            g_free(uri);
            return;
        }
        g_free(uri);
        idx++;
    }
}

void
add_to_list(BannedContactsView *self, const std::string& contactUri) {
    auto priv = BANNED_CONTACTS_VIEW_GET_PRIVATE(self);
    auto model = gtk_tree_view_get_model (GTK_TREE_VIEW(self));

    if (!priv->built) return;

    GtkTreeIter iter;

    gtk_list_store_append (GTK_LIST_STORE(model), &iter);
    gtk_list_store_set (GTK_LIST_STORE(model), &iter,
                        0 /* col # */ , contactUri.c_str() /* celldata */,
                        -1 /* end */);
}

static GtkTreeModel*
create_and_fill_model(BannedContactsView *self)
{
    auto priv = BANNED_CONTACTS_VIEW_GET_PRIVATE(self);
    auto store = gtk_list_store_new (1 /* # of cols */ ,
                                     G_TYPE_STRING);
    if (!priv || !priv->accountInfo_) return GTK_TREE_MODEL (store);

    GtkTreeIter iter;

    for (auto contact : (*priv->accountInfo_)->contactModel->getBannedContacts()) {
            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter,
                                0 /* col # */ , contact.c_str() /* celldata */,
                                -1 /* end */);
    }

    return GTK_TREE_MODEL (store);
}

static void
select_contact(GtkTreeSelection *selection, BannedContactsView *self)
{
    // XXX Need to store selected contact ?
}

static void
banned_contacts_view_init(BannedContactsView *self)
{
    // Nothing to do
}

static void
build_banned_contacts_view(BannedContactsView *self)
{
    auto priv = BANNED_CONTACTS_VIEW_GET_PRIVATE(self);

    auto model = create_and_fill_model(self);
    gtk_tree_view_set_model(GTK_TREE_VIEW(self),
                            GTK_TREE_MODEL(model));

    auto area = gtk_cell_area_box_new();
    auto column = gtk_tree_view_column_new_with_area(area);

    auto renderer = gtk_cell_renderer_text_new();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_peer_id,
        self,
        NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(self), column);

    priv->contactAddedConnection_ = QObject::connect(
    &*(*priv->accountInfo_)->contactModel,
    &lrc::api::ContactModel::contactAdded,
    [self] (const std::string& contactUri) {
        add_to_list(self, contactUri);
    });

    priv->contactRemovedConnection_ = QObject::connect(
    &*(*priv->accountInfo_)->contactModel,
    &lrc::api::ContactModel::contactRemoved,
    [self] (const std::string& contactUri) {
        remove_from_list(self, contactUri);
    });

    gtk_widget_show_all(GTK_WIDGET(self));

    auto selectionNew = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));
    g_signal_connect(selectionNew, "changed", G_CALLBACK(select_contact), self);
}

static void
banned_contacts_view_dispose(GObject *object)
{
    auto self = BANNED_CONTACTS_VIEW(object);
    auto priv = BANNED_CONTACTS_VIEW_GET_PRIVATE(self);

    QObject::disconnect(priv->contactRemovedConnection_);
    QObject::disconnect(priv->contactAddedConnection_);

    G_OBJECT_CLASS(banned_contacts_view_parent_class)->dispose(object);
}

static void
banned_contacts_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(banned_contacts_view_parent_class)->finalize(object);
}

static void
banned_contacts_view_class_init(BannedContactsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = banned_contacts_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = banned_contacts_view_dispose;
}

void
set_accountInfo_pointer(BannedContactsView *self, AccountInfoPointer const & accountInfo) {
    auto priv = BANNED_CONTACTS_VIEW_GET_PRIVATE(self);
    priv->accountInfo_ = &accountInfo;

    if (*priv->accountInfo_)
        build_banned_contacts_view(self);
        priv->built = true;
}

GtkWidget *
banned_contacts_view_new()
{
    auto self = BANNED_CONTACTS_VIEW(g_object_new(BANNED_CONTACTS_VIEW_TYPE, NULL));
    auto priv = BANNED_CONTACTS_VIEW_GET_PRIVATE(self);

    return (GtkWidget *)self;
}
