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

#include "models.h"

#include <gtk/gtk.h>
#include "../models/gtkqtreemodel.h"
#include "../models/gtkqsortfiltertreemodel.h"
#include <QtCore/QModelIndex>
#include <QtCore/QItemSelectionModel>

QModelIndex
get_index_from_selection(GtkTreeSelection *selection)
{
    GtkTreeIter iter;
    GtkTreeModel *model = NULL;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        if (GTK_IS_Q_TREE_MODEL(model))
            return gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
        else if (GTK_IS_Q_SORT_FILTER_TREE_MODEL(model))
            return gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), &iter);
    }
    return QModelIndex();
}

QModelIndex
gtk_combo_box_get_index(GtkComboBox *box)
{
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_combo_box_get_model(box);
    if (gtk_combo_box_get_active_iter(box, &iter)) {
        if (GTK_IS_Q_TREE_MODEL(model))
            return gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
        else if (GTK_IS_Q_SORT_FILTER_TREE_MODEL(model))
            return gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), &iter);
    }
    return QModelIndex();
}

static void
update_selection(GtkComboBox *box, QItemSelectionModel *selection_model)
{
    QModelIndex idx = gtk_combo_box_get_index(box);
    if (idx.isValid()) {
        selection_model->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
    }
}

static void
combo_box_show_enabled_data(GtkCellLayout *cell_layout,
                            GtkCellRenderer *cell,
                            GtkTreeModel *model,
                            GtkTreeIter *iter,
                            G_GNUC_UNUSED gpointer data)
{
    QModelIndex idx;
    if (GTK_IS_Q_TREE_MODEL(model))
        idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);
    else if (GTK_IS_Q_SORT_FILTER_TREE_MODEL(model))
        idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), iter);

    gchar *text = NULL;
    gboolean visible = FALSE;

    if (idx.isValid()) {
        text = g_strdup_printf("%s", idx.data(Qt::DisplayRole).value<QString>().toUtf8().constData());
        visible = idx.flags() & Qt::ItemIsEnabled ? TRUE : FALSE;
    }

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_object_set(G_OBJECT(cell), "sensitive", visible, NULL);
    g_free(text);
}

QMetaObject::Connection
gtk_combo_box_set_qmodel(GtkComboBox *box, QAbstractItemModel *qmodel, QItemSelectionModel *selection_model)
{
    QMetaObject::Connection connection;
    GtkTreeModel *model;

    /* check if its a QAbstractItemModel or a QSortFilterProxyModel */
    QSortFilterProxyModel *proxy_qmodel = qobject_cast<QSortFilterProxyModel*>(qmodel);
    if (proxy_qmodel) {
        model = (GtkTreeModel *)gtk_q_sort_filter_tree_model_new(
            proxy_qmodel,
            1,
            Qt::DisplayRole, G_TYPE_STRING);
    } else {
        model = (GtkTreeModel *)gtk_q_tree_model_new(
            qmodel,
            1,
            Qt::DisplayRole, G_TYPE_STRING);
    }

    gtk_combo_box_set_model(box, GTK_TREE_MODEL(model));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(box), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(box), renderer,
                                   "text", 0, NULL);
    /* use a custom data function to hide rows which are disabled */
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(box),
                                       renderer,
                                       (GtkCellLayoutDataFunc)combo_box_show_enabled_data,
                                       NULL, NULL);

    /* connect signals to and from the selection model */
    connection = QObject::connect(
        selection_model,
        &QItemSelectionModel::currentChanged,
        [=](const QModelIndex & current, G_GNUC_UNUSED const QModelIndex & previous) {
            /* select the current */
            if (current.isValid()) {
                GtkTreeIter new_iter;
                GtkTreeModel *model = gtk_combo_box_get_model(box);
                g_return_if_fail(model);

                gboolean valid;
                if (GTK_IS_Q_TREE_MODEL(model)) {
                    valid = gtk_q_tree_model_source_index_to_iter(
                        GTK_Q_TREE_MODEL(model), current, &new_iter);
                } else if (GTK_IS_Q_SORT_FILTER_TREE_MODEL(model)) {
                    valid = gtk_q_sort_filter_tree_model_source_index_to_iter(
                        GTK_Q_SORT_FILTER_TREE_MODEL(model), current, &new_iter);
                }

                if (valid)
                    gtk_combo_box_set_active_iter(box, &new_iter);
                else
                    g_warning("SelectionModel changed to invalid QModelIndex?");
            }
        }
    );
    g_signal_connect(box,
                     "changed",
                     G_CALLBACK(update_selection),
                     selection_model);

    /* sync the initial selection */
    QModelIndex idx = selection_model->currentIndex();
    if (idx.isValid()) {
        GtkTreeIter iter;
        gboolean valid;
        if (GTK_IS_Q_TREE_MODEL(model)) {
            valid = gtk_q_tree_model_source_index_to_iter(
                GTK_Q_TREE_MODEL(model), idx, &iter);
        } else if (GTK_IS_Q_SORT_FILTER_TREE_MODEL(model)) {
            valid = gtk_q_sort_filter_tree_model_source_index_to_iter(
                GTK_Q_SORT_FILTER_TREE_MODEL(model), idx, &iter);
        }

        if (valid)
            gtk_combo_box_set_active_iter(box, &iter);
        else
            g_warning("SelectionModel set to an invlid QModelIndex?");
    }

    return connection;
}