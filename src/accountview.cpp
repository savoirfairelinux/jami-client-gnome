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
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountView, account_view, GTK_TYPE_BOX);

#define ACCOUNT_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_VIEW_TYPE, AccountViewPrivate))

static void
account_view_dispose(GObject *object)
{
    // AccountView *view;
    // AccountViewPrivate *priv;

    // view = CURRENT_CALL_VIEW(object);
    // priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    G_OBJECT_CLASS(account_view_parent_class)->dispose(object);
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
        Qt::DisplayRole, G_TYPE_STRING,
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
}

static void
account_view_class_init(AccountViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountView, treeview_account_list);
}

GtkWidget *
account_view_new(void)
{
    return (GtkWidget *)g_object_new(ACCOUNT_VIEW_TYPE, NULL);
}
