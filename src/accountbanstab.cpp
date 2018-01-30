/*
 *  Copyright (C) 2017-2018 Savoir-faire Linux Inc.
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

// GTK
#include <gtk/gtk.h>
#include <glib/gi18n.h>

// LRC
#include <accountmodel.h>
#include <bannedcontactmodel.h>
#include <contactmethod.h>
#include <personmodel.h>

// Ring Client
#include "accountbanstab.h"
#include "models/gtkqtreemodel.h"
#include "bannedcontactsview.h"
#include "utils/models.h"

// Qt
#include <QItemSelectionModel>
#include <QSortFilterProxyModel>

/**
 * gtk structure
 */
struct _AccountBansTab
{
    GtkBox parent;
};

/**
 * gtk class structure
 */
struct _AccountBansTabClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountBansTabPrivate AccountBansTabPrivate;

/**
 * gtk private structure
 */
struct _AccountBansTabPrivate
{
    Account   *account;
    GtkWidget *scrolled_window_bans_tab;
    GtkWidget *treeview_bans;
    GtkWidget *button_unban;
    QMetaObject::Connection account_state_changed;

};

G_DEFINE_TYPE_WITH_PRIVATE(AccountBansTab, account_bans_tab, GTK_TYPE_BOX);

#define ACCOUNT_BANS_TAB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_BANS_TAB_TYPE, AccountBansTabPrivate))

/**
 * gtk dispose function
 */
static void
account_bans_tab_dispose(GObject *object)
{
    AccountBansTab *self = ACCOUNT_BANS_TAB(object);
    AccountBansTabPrivate *priv = ACCOUNT_BANS_TAB_GET_PRIVATE(self);

    G_OBJECT_CLASS(account_bans_tab_parent_class)->dispose(object);
    QObject::disconnect(priv->account_state_changed);

}

/**
 * gtk callback function called when the selection in the contact request list changed
 */
static void
selection_bans_changed(GtkTreeSelection* selection, AccountBansTab* self)
{
    auto priv = ACCOUNT_BANS_TAB_GET_PRIVATE(self);
    auto has_selection = (gtk_tree_selection_count_selected_rows(selection) > 0);

    gtk_widget_set_sensitive(priv->button_unban, has_selection);
}

/**
 * gtk callback function called when button_unban is clicked.
 */
static void
button_unban_clicked(AccountBansTab* view)
{
    auto priv = ACCOUNT_BANS_TAB_GET_PRIVATE(view);
    auto treeview = GTK_TREE_VIEW(priv->treeview_bans);
    auto tsel = gtk_tree_view_get_selection(treeview);

    GtkTreeModel* tm;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(tsel , &tm , &iter)) {
        /* get Account */
        const auto idx_account = AccountModel::instance().selectionModel()->currentIndex();
        auto account = idx_account.data(static_cast<int>(Account::Role::Object)).value<Account*>();

        /* get ContactMethod */
        const auto idx_cm = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(gtk_tree_view_get_model(treeview)), &iter);
        auto cm = idx_cm.data(static_cast<int>(ContactMethod::Role::Object)).value<ContactMethod*>();

        if (not cm or not account) {
            g_error("cannot unban, invalid pointer(s). cm(%p), account(%p)", cm, account);
            return;
        }

        account->bannedContactModel()->remove(cm);
    }
}

/**
 * gtk finalize function
 */
static void
account_bans_tab_finalize(GObject *object)
{
    G_OBJECT_CLASS(account_bans_tab_parent_class)->finalize(object);
}

/**
 * gtk init function
 */
static void
account_bans_tab_init(AccountBansTab *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
    AccountBansTabPrivate *priv = ACCOUNT_BANS_TAB_GET_PRIVATE(view);

    priv->treeview_bans = banned_contacts_view_new();
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_bans_tab), priv->treeview_bans);

    auto selection_bans = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_bans));

    g_signal_connect_swapped(priv->button_unban, "clicked", G_CALLBACK(button_unban_clicked), view);
    g_signal_connect(selection_bans, "changed", G_CALLBACK(selection_bans_changed), view);
}

/**
 * gtk class init function
 */
static void
account_bans_tab_class_init(AccountBansTabClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = account_bans_tab_finalize;
    G_OBJECT_CLASS(klass)->dispose = account_bans_tab_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass), "/cx/ring/RingGnome/accountbanstab.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountBansTab, scrolled_window_bans_tab);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountBansTab, button_unban);
}

/**
 * gtk new function
 */
GtkWidget *
account_bans_tab_new(Account *account)
{
    g_return_val_if_fail(account != NULL, NULL);

    gpointer view = g_object_new(ACCOUNT_BANS_TAB_TYPE, NULL);

    AccountBansTabPrivate *priv = ACCOUNT_BANS_TAB_GET_PRIVATE(view);
    priv->account = account;

    return (GtkWidget *)view;
}
