/*
 *  Copyright (C) 2015-2017 Savoir-faire Linux Inc.
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

#include "historyview.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "models/gtkqtreemodel.h"
#include <categorizedhistorymodel.h>
#include <QtCore/QSortFilterProxyModel>
#include <personmodel.h>
#include "utils/calling.h"
#include <memory>
#include <globalinstances.h>
#include "native/pixbufmanipulator.h"
#include "defines.h"
#include "utils/models.h"
#include <contactmethod.h>
#include <QtCore/QDateTime> // for date time formatting
#include <QtCore/QItemSelectionModel>
#include "utils/menus.h"
#include "contactpopupmenu.h"
#include "models/namenumberfilterproxymodel.h"

struct _HistoryView
{
    GtkTreeView parent;
};

struct _HistoryViewClass
{
    GtkTreeViewClass parent_class;
};

typedef struct _HistoryViewPrivate HistoryViewPrivate;

struct _HistoryViewPrivate
{
    GtkWidget *popup_menu;

    NameNumberFilterProxy *filterproxy;
    QMetaObject::Connection category_changed;
};

G_DEFINE_TYPE_WITH_PRIVATE(HistoryView, history_view, GTK_TYPE_TREE_VIEW);

#define HISTORY_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HISTORY_VIEW_TYPE, HistoryViewPrivate))

static void
render_call_direction(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                      GtkCellRenderer *cell,
                      GtkTreeModel *tree_model,
                      GtkTreeIter *iter,
                      G_GNUC_UNUSED gpointer data)
{
    /* check if this is a top level item (the fuzzy date item),
     * in this case we don't want to show a call direction */
    gchar *render_direction = NULL;
    GtkTreeIter parent;
    if (gtk_tree_model_iter_parent(tree_model, &parent, iter)) {
        /* get direction and missed values */
        GValue value = G_VALUE_INIT;
        gtk_tree_model_get_value(tree_model, iter, 3, &value);
        Call::Direction direction = (Call::Direction)g_value_get_int(&value);
        g_value_unset(&value);

        gtk_tree_model_get_value(tree_model, iter, 4, &value);
        gboolean missed = g_value_get_boolean(&value);
        g_value_unset(&value);

        switch (direction) {
            case Call::Direction::INCOMING:
                if (missed)
                    render_direction = g_strdup_printf("<span fgcolor=\"red\" font=\"monospace\">&#8601;</span>");
                else
                    render_direction = g_strdup_printf("<span fgcolor=\"green\" font=\"monospace\">&#8601;</span>");
            break;
            case Call::Direction::OUTGOING:
                if (missed)
                    render_direction = g_strdup_printf("<span fgcolor=\"red\" font=\"monospace\">&#8599;</span>");
                else
                    render_direction = g_strdup_printf("<span fgcolor=\"green\" font=\"monospace\">&#8599;</span>");
            break;
        }
    }
    g_object_set(G_OBJECT(cell), "markup", render_direction, NULL);
    g_free(render_direction);
}

static void
activate_history_item(GtkTreeView *tree_view,
                      GtkTreePath *path,
                      G_GNUC_UNUSED GtkTreeViewColumn *column,
                      G_GNUC_UNUSED gpointer user_data)
{
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);

    /* expand / collapse row */
    if (gtk_tree_view_row_expanded(tree_view, path))
        gtk_tree_view_collapse_row(tree_view, path);
    else
        gtk_tree_view_expand_row(tree_view, path, FALSE);

    /* get iter */
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);

        QVariant contact_method = idx.data(static_cast<int>(Call::Role::ContactMethod));
        /* create new call */
        if (contact_method.value<ContactMethod*>()) {
            place_new_call(contact_method.value<ContactMethod*>());
        }
    }
}

static void
render_call_photo(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *tree_model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    /* check if this is a top level item (category),
     * in this case we don't want to show a photo */
    GtkTreePath *path = gtk_tree_model_get_path(tree_model, iter);
    int depth = gtk_tree_path_get_depth(path);
    gtk_tree_path_free(path);
    if (depth == 2) {
        /* get person */
        QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(tree_model), iter);
        if (idx.isValid()) {
            QVariant var_c = idx.data(static_cast<int>(Call::Role::Object));
            Call *c = var_c.value<Call *>();
            /* get photo */
            QVariant var_p = GlobalInstances::pixmapManipulator().callPhoto(c, QSize(50, 50), false);
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
     * If call, show the name and the contact method (number) underneath;
     * otherwise just display the category.
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
            /* call item */
            QVariant var_name = idx.data(static_cast<int>(Call::Role::Name));
            QVariant var_number = idx.data(static_cast<int>(Call::Role::Number));

            /* we want the color of the status text to be the default color if this iter is
             * selected so that the treeview is able to invert it against the selection color */
            if (is_selected) {
                text = g_markup_printf_escaped("%s\n %s",
                                               var_name.value<QString>().toUtf8().constData(),
                                               var_number.value<QString>().toUtf8().constData());
            } else {
                text = g_markup_printf_escaped("%s\n <span fgcolor=\"gray\">%s</span>",
                                                var_name.value<QString>().toUtf8().constData(),
                                                var_number.value<QString>().toUtf8().constData());
            }
        }
    }

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(text);
}

static void
render_date_time(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
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

    gchar *text = NULL;

    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(tree_model), iter);
    QVariant var_d = idx.data(static_cast<int>(Call::Role::DateTime));
    if (idx.isValid() && var_d.isValid()) {
        QDateTime date_time = var_d.value<QDateTime>();

        /* we want the color of the text to be the default color if this iter is
         * selected so that the treeview is able to invert it against the selection color */
        if (is_selected) {
            text = g_markup_printf_escaped("%s\n%s",
                                           date_time.time().toString().toUtf8().constData(),
                                           date_time.date().toString().toUtf8().constData()
            );
        } else {
            text = g_markup_printf_escaped("%s\n<span fgcolor=\"gray\">%s</span>",
                                           date_time.time().toString().toUtf8().constData(),
                                           date_time.date().toString().toUtf8().constData()
            );
        }
    }

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(text);
}

static void
history_view_init(HistoryView *self)
{
    HistoryViewPrivate *priv = HISTORY_VIEW_GET_PRIVATE(self);

    /* make headers visible to allow column resizing */
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(self), TRUE);
    /* disable default search, we will handle it ourselves;
     * otherwise the search steals input focus on key presses */
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(self), FALSE);

    /* instantiate history proxy model */
    auto q_sorted_proxy = &CategorizedHistoryModel::SortedProxy::instance();

    /* select default category (the first one, which is by date) */
    q_sorted_proxy->categorySelectionModel()->setCurrentIndex(
        q_sorted_proxy->categoryModel()->index(0, 0),
        QItemSelectionModel::ClearAndSelect);
    /* make sure it is sorted so that newest calls are at the top */
    q_sorted_proxy->model()->sort(0, Qt::AscendingOrder);

    /* filter for name and number */
    priv->filterproxy = new NameNumberFilterProxy(q_sorted_proxy->model());

    GtkQTreeModel *history_model = gtk_q_tree_model_new(
        priv->filterproxy,
        5,
        0, Qt::DisplayRole, G_TYPE_STRING,
        0, Call::Role::Number, G_TYPE_STRING,
        0, Call::Role::FormattedDate, G_TYPE_STRING,
        0, Call::Role::Direction, G_TYPE_INT,
        0, Call::Role::Missed, G_TYPE_BOOLEAN);
    gtk_tree_view_set_model( GTK_TREE_VIEW(self), GTK_TREE_MODEL(history_model) );

    /* call direction, photo, name/number column */
    GtkCellArea *area = gtk_cell_area_box_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_area(area);
    gtk_tree_view_column_set_title(column, C_("Call history", "Call"));

    /* call direction */
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    /* display the call direction with arrows */
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_call_direction,
        NULL,
        NULL);

    /* photo renderer */
    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    /* get the photo */
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_call_photo,
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
    gtk_tree_view_column_set_expand(column, TRUE);

    /* date time column */
    area = gtk_cell_area_box_new();
    column = gtk_tree_view_column_new_with_area(area);
    gtk_tree_view_column_set_title(column, C_("Call history", "Date"));

    /* date time renderer */
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);
    /* format the time*/
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_date_time,
        self,
        NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(self), column);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, FALSE);

    g_signal_connect(self, "row-activated", G_CALLBACK(activate_history_item), NULL);

    /* init popup menu */
    priv->popup_menu = contact_popup_menu_new(GTK_TREE_VIEW(self));
    g_signal_connect_swapped(self, "button-press-event", G_CALLBACK(contact_popup_menu_show), priv->popup_menu);

    gtk_widget_show_all(GTK_WIDGET(self));
}

static void
history_view_dispose(GObject *object)
{
    HistoryViewPrivate *priv = HISTORY_VIEW_GET_PRIVATE(object);

    QObject::disconnect(priv->category_changed);
    gtk_widget_destroy(priv->popup_menu);

    G_OBJECT_CLASS(history_view_parent_class)->dispose(object);
}

static void
history_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(history_view_parent_class)->finalize(object);
}

static void
history_view_class_init(HistoryViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = history_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = history_view_dispose;
}

GtkWidget *
history_view_new()
{
    gpointer self = g_object_new(HISTORY_VIEW_TYPE, NULL);

    return (GtkWidget *)self;
}

void
history_view_set_filter_string(HistoryView *self, const char *text)
{
    auto priv = HISTORY_VIEW_GET_PRIVATE(self);

    priv->filterproxy->setFilterRegExp(QRegExp(text, Qt::CaseInsensitive, QRegExp::FixedString));

    // if filtering, expand all categories so we can see the calls
    // otherwise collapse all
    if (text && strlen(text) > 0)
        gtk_tree_view_expand_all(GTK_TREE_VIEW(self));
    else
        gtk_tree_view_collapse_all(GTK_TREE_VIEW(self));
}
