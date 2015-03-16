/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "accountview.h"

#include <gtk/gtk.h>
#include <accountmodel.h>
#include "models/gtkqtreemodel.h"
#include "accountgeneraltab.h"
#include "accountaudiotab.h"
#include "accountvideotab.h"

struct _AccountView
{
    GtkBox parent;
};

struct _AccountViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountViewPrivate AccountViewPrivate;

struct _AccountViewPrivate
{
    GtkWidget *treeview_account_list;
    GtkWidget *notebook_account_settings;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountView, account_view, GTK_TYPE_BOX);

#define ACCOUNT_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_VIEW_TYPE, AccountViewPrivate))

static void
account_view_dispose(GObject *object)
{
    // AccountView *view;
    // AccountViewPrivate *priv;

    // view = ACCOUNT_VIEW(object);
    // priv = ACCOUNT_VIEW_GET_PRIVATE(view);

    G_OBJECT_CLASS(account_view_parent_class)->dispose(object);
}

static QModelIndex
get_index_from_selection(GtkTreeSelection *selection)
{
    GtkTreeIter iter;
    GtkTreeModel *model = NULL;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
    } else {
        return QModelIndex();
    }
}

static void
account_selection_changed(GtkTreeSelection *selection, AccountView *view)
{
    g_return_if_fail(IS_ACCOUNT_VIEW(view));
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(view);
    
    /* first remove all current tabs in the notebook */
    int num_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(priv->notebook_account_settings));
    for (int i = num_pages; i > 0; --i) {
        gtk_notebook_remove_page(GTK_NOTEBOOK(priv->notebook_account_settings), i - 1);
    }

    /* now get the acount selected and create pages based on the account */
    QModelIndex account_idx = get_index_from_selection(selection);
    if (account_idx.isValid()) {
        Account *account = AccountModel::instance()->getAccountByModelIndex(account_idx);

        GtkWidget *general_tab = account_general_tab_new(account);
        gtk_notebook_append_page(GTK_NOTEBOOK(priv->notebook_account_settings),
                                 general_tab,
                                 gtk_label_new("General"));
        GtkWidget *audio_tab = account_audio_tab_new(account);
        gtk_notebook_append_page(GTK_NOTEBOOK(priv->notebook_account_settings),
                                 audio_tab,
                                 gtk_label_new("Audio"));
        GtkWidget *video_tab = account_video_tab_new(account);
        gtk_notebook_append_page(GTK_NOTEBOOK(priv->notebook_account_settings),
                                 video_tab,
                                 gtk_label_new("Video"));
    }
}

static void
account_view_init(AccountView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(view);

    /* account model */
    GtkQTreeModel *account_model;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    /* test */
    g_debug("total accounts %d", AccountModel::instance()->rowCount());

    account_model = gtk_q_tree_model_new(AccountModel::instance(), 4,
        Account::Role::Enabled, G_TYPE_BOOLEAN,
        Account::Role::Alias, G_TYPE_STRING,
        Account::Role::Proto, G_TYPE_INT,
        Account::Role::RegistrationState, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_account_list), GTK_TREE_MODEL(account_model));

    renderer = gtk_cell_renderer_toggle_new();
    column = gtk_tree_view_column_new_with_attributes("Enabled", renderer, "active", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_account_list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Alias", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_account_list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Status", renderer, "text", 3, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_account_list), column);

    /* connect to selection signal */
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_account_list));
    g_signal_connect(selection, "changed", G_CALLBACK(account_selection_changed), view);

    /* select the first item in the treeview */
    GtkTreeIter iter;
    gtk_tree_model_iter_children(GTK_TREE_MODEL(account_model), &iter, NULL);
    gtk_tree_selection_select_iter(selection, &iter);
}

static void
account_view_class_init(AccountViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountView, treeview_account_list);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountView, notebook_account_settings);
}

GtkWidget *
account_view_new(void)
{
    return (GtkWidget *)g_object_new(ACCOUNT_VIEW_TYPE, NULL);
}
