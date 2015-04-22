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

#include "contactsview.h"

#include <gtk/gtk.h>
#include "models/gtkqsortfiltertreemodel.h"
#include "models/activeitemproxymodel.h"
#include <categorizedcontactmodel.h>
#include <personmodel.h>
#include "utils/calling.h"
#include <memory>
#include "delegates/pixbufdelegate.h"
#include <contactmethod.h>

struct _ContactsView
{
    GtkScrolledWindow parent;
};

struct _ContactsViewClass
{
    GtkScrolledWindowClass parent_class;
};

typedef struct _ContactsViewPrivate ContactsViewPrivate;

struct _ContactsViewPrivate
{
    ActiveItemProxyModel *q_contact_model;
};

G_DEFINE_TYPE_WITH_PRIVATE(ContactsView, contacts_view, GTK_TYPE_SCROLLED_WINDOW);

#define CONTACTS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CONTACTS_VIEW_TYPE, ContactsViewPrivate))

static void
render_contact_photo(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *tree_model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    /* check if this is a top level item (category),
     * or a bottom level item (contact method)gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
     * in this case we don't want to show a photo */
    GtkTreePath *path = gtk_tree_model_get_path(tree_model, iter);
    int depth = gtk_tree_path_get_depth(path);
    gtk_tree_path_free(path);
    if (depth == 2) {
        /* get person */
        QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(tree_model), iter);
        if (idx.isValid()) {
            QVariant var_c = idx.data(static_cast<int>(Person::Role::Object));
            Person *c = var_c.value<Person *>();
            /* get photo */
            QVariant var_p = PixbufDelegate::instance()->contactPhoto(c, QSize(50, 50), false);
            std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
            g_object_set(G_OBJECT(cell), "pixbuf", photo.get(), NULL);
            return;
        }
    }

    /* otherwise, make sure its an empty pixbuf */
    g_object_set(G_OBJECT(cell), "pixbuf", NULL, NULL);
}

static void
render_name_and_contact_method(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                               GtkCellRenderer *cell,
                               GtkTreeModel *tree_model,
                               GtkTreeIter *iter,
                               G_GNUC_UNUSED gpointer data)
{
    /**
     * If contact (person), show the name and the contact method (number)
     * underneath; if multiple contact methods, then indicate as such
     *
     * Otherwise just display the category or contact method
     */
    GtkTreePath *path = gtk_tree_model_get_path(tree_model, iter);
    int depth = gtk_tree_path_get_depth(path);
    gtk_tree_path_free(path);

    gchar *text = NULL;

    QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(tree_model), iter);
    if (idx.isValid()) {
        QVariant var = idx.data(Qt::DisplayRole);
        if (depth == 1) {
            /* category */
            text = g_strdup_printf("<b>%s</b>", var.value<QString>().toUtf8().constData());
        } else if (depth == 2) {
            /* contact, check for contact methods */
            QVariant var_c = idx.data(static_cast<int>(Person::Role::Object));
            if (var_c.isValid()) {
                Person *c = var_c.value<Person *>();
                switch (c->phoneNumbers().size()) {
                    case 0:
                        text = g_strdup_printf("%s\n", c->formattedName().toUtf8().constData());
                        break;
                    case 1:
                    {
                        QString number;
                        QVariant var_n = c->phoneNumbers().first()->roleData(Qt::DisplayRole);
                        if (var_n.isValid())
                            number = var_n.value<QString>();

                        text = g_strdup_printf("%s\n <span fgcolor=\"gray\">%s</span>",
                                               c->formattedName().toUtf8().constData(),
                                               number.toUtf8().constData());
                        break;
                    }
                    default:
                        /* more than one, for now don't show any of the contact methods */
                        text = g_strdup_printf("%s\n", c->formattedName().toUtf8().constData());
                        break;
                }
            } else {
                /* should never happen since depth 2 should always be a contact (person) */
                text = g_strdup_printf("%s", var.value<QString>().toUtf8().constData());
            }
        } else {
            /* contact method (or deeper??) */
            text = g_strdup_printf("%s", var.value<QString>().toUtf8().constData());
        }
    }

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(text);

    /* set the colour */
    if ( depth == 1) {
        /* nice blue taken from the ring logo */
        GdkRGBA rgba = {0.2, 0.75294117647, 0.82745098039, 0.1};
        g_object_set(G_OBJECT(cell), "cell-background-rgba", &rgba, NULL);
    } else {
        g_object_set(G_OBJECT(cell), "cell-background", NULL, NULL);
    }
}

static void
expand_if_child(G_GNUC_UNUSED GtkTreeModel *tree_model,
                GtkTreePath  *path,
                G_GNUC_UNUSED GtkTreeIter  *iter,
                GtkTreeView  *treeview)
{
    if (gtk_tree_path_get_depth(path) == 2)
        gtk_tree_view_expand_to_path(treeview, path);
}

static void
activate_contact_item(GtkTreeView *tree_view,
                      GtkTreePath *path,
                      G_GNUC_UNUSED GtkTreeViewColumn *column,
                      G_GNUC_UNUSED gpointer user_data)
{
    /* expand / contract row */
    if (gtk_tree_view_row_expanded(tree_view, path))
        gtk_tree_view_collapse_row(tree_view, path);
    else
        gtk_tree_view_expand_row(tree_view, path, FALSE);

    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);

    /* get iter */
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), &iter);
        if (idx.isValid()) {
            int depth = gtk_tree_path_get_depth(path);
            switch (depth) {
                case 0:
                case 1:
                    /* category, nothing to do */
                    break;
                case 2:
                {
                    /* contact (person), use contact method if there is only one */
                    QVariant var_c = idx.data(static_cast<int>(Person::Role::Object));
                    if (var_c.isValid()) {
                        Person *c = var_c.value<Person *>();
                        if (c->phoneNumbers().size() == 1) {
                            /* call with contact method */
                            place_new_call(c->phoneNumbers().first());
                        }
                    }
                    break;
                }
                default:
                {
                    /* contact method (or deeper) */
                    QVariant var_n = idx.data(static_cast<int>(ContactMethod::Role::Object));
                    if (var_n.isValid()) {
                        /* call with contat method */
                        place_new_call(var_n.value<ContactMethod *>());
                    }
                    break;
                }
            }
        }
    }
}

static void
contacts_view_init(ContactsView *self)
{
    ContactsViewPrivate *priv = CONTACTS_VIEW_GET_PRIVATE(self);

    /* contacts view/model */
    GtkWidget *treeview_contacts = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_contacts), FALSE);
    gtk_container_add(GTK_CONTAINER(self), treeview_contacts);

    CategorizedContactModel::instance()->setUnreachableHidden(true);
    priv->q_contact_model = new ActiveItemProxyModel(CategorizedContactModel::instance());
    priv->q_contact_model->setSortRole(Qt::DisplayRole);
    priv->q_contact_model->setSortLocaleAware(true);
    priv->q_contact_model->setSortCaseSensitivity(Qt::CaseInsensitive);
    priv->q_contact_model->sort(0);

    GtkQSortFilterTreeModel *contact_model = gtk_q_sort_filter_tree_model_new(
        (QSortFilterProxyModel *)priv->q_contact_model,
        1,
        Qt::DisplayRole, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview_contacts), GTK_TREE_MODEL(contact_model));

    /* photo and name/contact method column */
    GtkCellArea *area = gtk_cell_area_box_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_area(area);
    gtk_tree_view_column_set_title(column, "Name");

    /* photo renderer */
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    /* get the photo */
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_contact_photo,
        NULL,
        NULL);

    /* name and contact method renderer */
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_name_and_contact_method,
        NULL,
        NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_contacts), column);
    gtk_tree_view_column_set_resizable(column, TRUE);

    gtk_tree_view_expand_all(GTK_TREE_VIEW(treeview_contacts));
    g_signal_connect(contact_model, "row-inserted", G_CALLBACK(expand_if_child), treeview_contacts);
    g_signal_connect(treeview_contacts, "row-activated", G_CALLBACK(activate_contact_item), NULL);

    gtk_widget_show_all(GTK_WIDGET(self));
}

static void
contacts_view_dispose(GObject *object)
{
    G_OBJECT_CLASS(contacts_view_parent_class)->dispose(object);
}

static void
contacts_view_finalize(GObject *object)
{
    ContactsView *self = CONTACTS_VIEW(object);
    ContactsViewPrivate *priv = CONTACTS_VIEW_GET_PRIVATE(self);

    delete priv->q_contact_model;

    G_OBJECT_CLASS(contacts_view_parent_class)->finalize(object);
}

static void
contacts_view_class_init(ContactsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = contacts_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = contacts_view_dispose;
}

GtkWidget *
contacts_view_new()
{
    gpointer self = g_object_new(CONTACTS_VIEW_TYPE, NULL);

    return (GtkWidget *)self;
}