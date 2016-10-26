/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
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
 */

#include "contactsview.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "models/gtkqtreemodel.h"
#include <categorizedcontactmodel.h>
#include <personmodel.h>
#include "utils/calling.h"
#include <memory>
#include <globalinstances.h>
#include "native/pixbufmanipulator.h"
#include <contactmethod.h>
#include "defines.h"
#include "utils/models.h"
#include <QtCore/QItemSelectionModel>
#include "numbercategory.h"
#include "contactpopupmenu.h"

class NameNumberFilterProxy : public QSortFilterProxyModel
{
public:
    NameNumberFilterProxy(QAbstractItemModel* source_model);

    virtual bool filterAcceptsRow ( int source_row, const QModelIndex & source_parent ) const override;

};

NameNumberFilterProxy::NameNumberFilterProxy(QAbstractItemModel* sourceModel)
{
    setParent(sourceModel);
    setSourceModel(sourceModel);
}

bool
NameNumberFilterProxy::filterAcceptsRow(int source_row, const QModelIndex & source_parent) const
{
    //we don't filter on top nodes, since those are categories
    if (!source_parent.isValid() || filterRegExp().isEmpty())
        return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
    else {
        auto idx = sourceModel()->index(source_row, 0);

        //we want to filter on name and number; note that Person object may have many numbers
        if (idx.data(static_cast<int>(Ring::Role::Name)).toString().contains(filterRegExp())) {
            return true;
        } else {
            auto type = idx.data(static_cast<int>(Ring::Role::ObjectType)).value<Ring::ObjectType>();
            auto object = idx.data(static_cast<int>(Ring::Role::Object));

            switch (type) {
                case Ring::ObjectType::Person:
                {
                    auto p = object.value<Person *>();
                    for (auto cm : p->phoneNumbers()) {
                        if (cm->uri().full().contains(filterRegExp()))
                            return true;
                    }
                    return false;
                }
                break;
                case Ring::ObjectType::ContactMethod:
                {
                    auto cm = object.value<ContactMethod *>();
                    return cm->uri().full().contains(filterRegExp());
                }
                break;
                // top nodes are only of type Person or ContactMethod
                case Ring::ObjectType::Call:
                case Ring::ObjectType::Media:
                case Ring::ObjectType::Certificate:
                case Ring::ObjectType::COUNT__:
                break;
            }

        }
        return false; // no matches
    }
}

struct _ContactsView
{
    GtkTreeView parent;
};

struct _ContactsViewClass
{
    GtkTreeViewClass parent_class;
};

typedef struct _ContactsViewPrivate ContactsViewPrivate;

struct _ContactsViewPrivate
{
    GtkWidget *popup_menu;

    NameNumberFilterProxy *filterproxy;
};

G_DEFINE_TYPE_WITH_PRIVATE(ContactsView, contacts_view, GTK_TYPE_TREE_VIEW);

#define CONTACTS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CONTACTS_VIEW_TYPE, ContactsViewPrivate))

static void
render_contact_photo(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *tree_model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    /* check if this is a top level item (category),
     * or a bottom level item (contact method)
     * in this case we don't want to show a photo */
    GtkTreePath *path = gtk_tree_model_get_path(tree_model, iter);
    int depth = gtk_tree_path_get_depth(path);
    gtk_tree_path_free(path);
    if (depth == 2) {
        /* get person */
        QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(tree_model), iter);
        if (idx.isValid()) {
            QVariant var_c = idx.data(static_cast<int>(Person::Role::Object));
            Person *c = var_c.value<Person *>();
            /* get photo */
            QVariant var_p = GlobalInstances::pixmapManipulator().contactPhoto(c, QSize(50, 50), false);
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
                               GtkTreeView *treeview)
{
    // check if this iter is selected
    gboolean is_selected = FALSE;
    if (GTK_IS_TREE_VIEW(treeview)) {
        auto selection = gtk_tree_view_get_selection(treeview);
        is_selected = gtk_tree_selection_iter_is_selected(selection, iter);
    }

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

    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(tree_model), iter);
    if (idx.isValid()) {
        QVariant var = idx.data(Qt::DisplayRole);
        if (depth == 1) {
            /* category */
            text = g_markup_printf_escaped("<b>%s</b>", var.value<QString>().toUtf8().constData());
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

                        /* we want the color of the status text to be the default color if this iter is
                         * selected so that the treeview is able to invert it against the selection color */
                        if (is_selected) {
                            text = g_markup_printf_escaped("%s\n %s",
                                                           c->formattedName().toUtf8().constData(),
                                                           number.toUtf8().constData());
                        } else {
                            text = g_markup_printf_escaped("%s\n <span fgcolor=\"gray\">%s</span>",
                                                            c->formattedName().toUtf8().constData(),
                                                            number.toUtf8().constData());
                        }
                        break;
                    }
                    default:
                        /* more than one, for now don't show any of the contact methods */
                        text = g_markup_printf_escaped("%s\n", c->formattedName().toUtf8().constData());
                        break;
                }
            } else {
                /* should never happen since depth 2 should always be a contact (person) */
                text = g_markup_printf_escaped("%s", var.value<QString>().toUtf8().constData());
            }
        } else {
            auto var_object = idx.data(static_cast<int>(Ring::Role::Object));
            auto cm = var_object.value<ContactMethod *>();
            if (cm && cm->category()) {
                // try to get the number category, eg: "home"
                text = g_markup_printf_escaped("(%s) %s", cm->category()->name().toUtf8().constData(),
                                                          cm->uri().toUtf8().constData());
            } else if (cm) {
                text = g_markup_printf_escaped("%s", cm->uri().toUtf8().constData());
            } else {
                /* should only ever be a CM, so this should never execute */
                text = g_markup_printf_escaped("%s", var.value<QString>().toUtf8().constData());
            }
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
        QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
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

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(self), FALSE);

    /* disable default search, we will handle it ourselves;
     * otherwise the search steals input focus on key presses */
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(self), FALSE);

    /* initial set up to be categorized by name and sorted alphabetically */
    auto q_sorted_proxy = &CategorizedContactModel::SortedProxy::instance();
    CategorizedContactModel::instance().setUnreachableHidden(true);

    /* for now we always want to sort by ascending order */
    q_sorted_proxy->model()->sort(0);

    /* select default category (the first one, which is by name) */
    q_sorted_proxy->categorySelectionModel()->setCurrentIndex(
        q_sorted_proxy->categoryModel()->index(0, 0),
        QItemSelectionModel::ClearAndSelect);

    // filtering
    priv->filterproxy = new NameNumberFilterProxy(q_sorted_proxy->model());

    GtkQTreeModel *contact_model = gtk_q_tree_model_new(
        priv->filterproxy,
        1,
        0, Qt::DisplayRole, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(self), GTK_TREE_MODEL(contact_model));

    /* photo and name/contact method column */
    GtkCellArea *area = gtk_cell_area_box_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_area(area);

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
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_name_and_contact_method,
        self,
        NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(self), column);
    gtk_tree_view_column_set_resizable(column, TRUE);

    gtk_tree_view_expand_all(GTK_TREE_VIEW(self));
    g_signal_connect(contact_model, "row-inserted", G_CALLBACK(expand_if_child), self);
    g_signal_connect(self, "row-activated", G_CALLBACK(activate_contact_item), NULL);

    /* init popup menu */
    priv->popup_menu = contact_popup_menu_new(GTK_TREE_VIEW(self));
    g_signal_connect_swapped(self, "button-press-event", G_CALLBACK(contact_popup_menu_show), priv->popup_menu);

    gtk_widget_show_all(GTK_WIDGET(self));
}

static void
contacts_view_dispose(GObject *object)
{
    ContactsViewPrivate *priv = CONTACTS_VIEW_GET_PRIVATE(object);

    gtk_widget_destroy(priv->popup_menu);

    G_OBJECT_CLASS(contacts_view_parent_class)->dispose(object);
}

static void
contacts_view_finalize(GObject *object)
{
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

void
contacts_view_set_filter_string(ContactsView *self, const char* text)
{
    ContactsViewPrivate *priv = CONTACTS_VIEW_GET_PRIVATE(self);
    priv->filterproxy->setFilterRegExp(QRegExp(text, Qt::CaseInsensitive, QRegExp::FixedString));
}
