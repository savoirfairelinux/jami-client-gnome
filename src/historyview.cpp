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

#include "historyview.h"

#include <gtk/gtk.h>
#include "models/gtkqsortfiltertreemodel.h"
#include <categorizedhistorymodel.h>
#include <QtCore/QSortFilterProxyModel>
#include <personmodel.h>
#include "utils/calling.h"
#include <memory>
#include "delegates/pixbufdelegate.h"
#include "defines.h"
#include "utils/models.h"
#include <contactmethod.h>

struct _HistoryView
{
    GtkScrolledWindow parent;
};

struct _HistoryViewClass
{
    GtkScrolledWindowClass parent_class;
};

typedef struct _HistoryViewPrivate HistoryViewPrivate;

struct _HistoryViewPrivate
{
    QSortFilterProxyModel *q_history_model;
};

G_DEFINE_TYPE_WITH_PRIVATE(HistoryView, history_view, GTK_TYPE_SCROLLED_WINDOW);

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
        QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), &iter);

        QVariant contact_method = idx.data(static_cast<int>(Call::Role::ContactMethod));
        /* create new call */
        if (contact_method.value<ContactMethod*>()) {
            place_new_call(contact_method.value<ContactMethod*>());
        }
    }
}

static void
copy_history_item(G_GNUC_UNUSED GtkWidget *item, GtkTreeView *treeview)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    QModelIndex idx = get_index_from_selection(selection);

    if (idx.isValid()) {
        GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

        const gchar* number = idx.data(static_cast<int>(Call::Role::Number)).toString().toUtf8().constData();
        gtk_clipboard_set_text(clip, number, -1);
    }
}

/* TODO: can't seem to delete just one item for now, add when supported in backend
 * static void
 * delete_history_item(G_GNUC_UNUSED GtkWidget *item, GtkTreeView *treeview)
 * {
 *     GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
 *     QModelIndex idx = get_index_from_selection(selection);
 *
 *     if (idx.isValid()) {
 *         g_debug("deleting history item");
 *         CategorizedHistoryModel::instance()->removeRow(idx.row(), idx.parent());
 *     }
 * }
 */

static gboolean
history_popup_menu(G_GNUC_UNUSED GtkWidget *widget, GdkEventButton *event, GtkTreeView *treeview)
{
    /* build popup menu when right clicking on history item
     * user should be able to copy the "number",
     * delete history item or all of the history,
     * and eventualy add the number to a contact
     */

    /* check for right click */
    if (event->button != BUTTON_RIGHT_CLICK || event->type != GDK_BUTTON_PRESS)
        return FALSE;

    GtkWidget *menu = gtk_menu_new();

    /* copy */
    GtkWidget *item = gtk_menu_item_new_with_mnemonic("_Copy");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(copy_history_item), treeview);

    /* TODO: delete history entry
     * gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
     * item = gtk_menu_item_new_with_mnemonic("_Delete entry");
     * gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
     * g_signal_connect(item, "activate", G_CALLBACK(delete_history_item), treeview);
     */

    /* show menu */
    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);

    return FALSE; /* continue to default handler */
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
history_view_init(HistoryView *self)
{
    HistoryViewPrivate *priv = HISTORY_VIEW_GET_PRIVATE(self);

    /* history view/model */
    GtkWidget *treeview_history = gtk_tree_view_new();
    /* make headers visible to allow column resizing */
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_history), TRUE);
    gtk_container_add(GTK_CONTAINER(self), treeview_history);

    /* disable default search, we will handle it ourselves via LRC;
     * otherwise the saerch steals input focus on key presses */
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(treeview_history), FALSE);

    /* sort the history in descending order by date */
    priv->q_history_model = new QSortFilterProxyModel();
    priv->q_history_model->setSourceModel(CategorizedHistoryModel::instance());
    priv->q_history_model->setSortRole(static_cast<int>(Call::Role::Date));
    priv->q_history_model->sort(0,Qt::DescendingOrder);

    GtkQSortFilterTreeModel *history_model = gtk_q_sort_filter_tree_model_new(
        priv->q_history_model,
        5,
        Qt::DisplayRole, G_TYPE_STRING,
        Call::Role::Number, G_TYPE_STRING,
        Call::Role::FormattedDate, G_TYPE_STRING,
        Call::Role::Direction, G_TYPE_INT,
        Call::Role::Missed, G_TYPE_BOOLEAN);
    gtk_tree_view_set_model( GTK_TREE_VIEW(treeview_history), GTK_TREE_MODEL(history_model) );

    /* name column, also used for call direction and fuzzy date for top level items */
    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "Name");

    /* call direction */
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);

    /* display the call direction with arrows */
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_call_direction,
        NULL,
        NULL);

    /* name or time category column */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_attributes(column, renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_history), column);
    gtk_tree_view_column_set_resizable(column, TRUE);

    /* "number" column */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    column = gtk_tree_view_column_new_with_attributes("Number", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_history), column);
    gtk_tree_view_column_set_resizable(column, TRUE);

    /* date column */
    renderer = gtk_cell_renderer_text_new ();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    column = gtk_tree_view_column_new_with_attributes ("Date", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_history), column);
    gtk_tree_view_column_set_resizable(column, TRUE);

    /* expand the first row, which should be the most recent calls */
    gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview_history),
                             gtk_tree_path_new_from_string("0"),
                             FALSE);

    g_signal_connect(treeview_history, "row-activated", G_CALLBACK(activate_history_item), NULL);
    g_signal_connect(treeview_history, "button-press-event", G_CALLBACK(history_popup_menu), treeview_history);
    g_signal_connect(history_model, "row-inserted", G_CALLBACK(expand_if_child), treeview_history);

    gtk_widget_show_all(GTK_WIDGET(self));
}

static void
history_view_dispose(GObject *object)
{
    G_OBJECT_CLASS(history_view_parent_class)->dispose(object);
}

static void
history_view_finalize(GObject *object)
{
    HistoryView *self = HISTORY_VIEW(object);
    HistoryViewPrivate *priv = HISTORY_VIEW_GET_PRIVATE(self);

    delete priv->q_history_model;

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