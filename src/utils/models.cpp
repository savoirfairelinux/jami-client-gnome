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
    GtkTreeIter filter_iter;
    GtkTreeIter child_iter;
    GtkTreeModel *filter_model = gtk_combo_box_get_model(box);
    GtkTreeModel *model = filter_model;

    GtkTreeIter *iter = NULL;

    if (GTK_IS_TREE_MODEL_FILTER(filter_model))
        model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(filter_model));

    if (gtk_combo_box_get_active_iter(box, &filter_iter)) {
        if (GTK_IS_TREE_MODEL_FILTER(filter_model)) {
            gtk_tree_model_filter_convert_iter_to_child_iter(
                GTK_TREE_MODEL_FILTER(filter_model),
                &child_iter,
                &filter_iter);
            iter = &child_iter;
        } else {
            iter = &filter_iter;
        }

        if (GTK_IS_Q_TREE_MODEL(model))
            return gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);
        else if (GTK_IS_Q_SORT_FILTER_TREE_MODEL(model))
            return gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), iter);
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

static gboolean
filter_disabled_items(GtkTreeModel *model, GtkTreeIter *iter, G_GNUC_UNUSED gpointer data)
{
    QModelIndex idx;
    if (GTK_IS_Q_TREE_MODEL(model))
        idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);
    else if (GTK_IS_Q_SORT_FILTER_TREE_MODEL(model))
        idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), iter);

    if (idx.isValid()) {
        return idx.flags() & Qt::ItemIsEnabled ? TRUE : FALSE;
    }
    return FALSE;
}

void
gtk_combo_box_set_active_index(GtkComboBox *box, const QModelIndex& idx)
{
    if (idx.isValid()) {
        GtkTreeIter new_iter;
        GtkTreeModel *filter_model = gtk_combo_box_get_model(box);
        g_return_if_fail(filter_model);
        GtkTreeModel *model = filter_model;

        if (GTK_IS_TREE_MODEL_FILTER(filter_model))
            model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(filter_model));

        gboolean valid;
        if (GTK_IS_Q_TREE_MODEL(model)) {
            valid = gtk_q_tree_model_source_index_to_iter(
                GTK_Q_TREE_MODEL(model), idx, &new_iter);
        } else if (GTK_IS_Q_SORT_FILTER_TREE_MODEL(model)) {
            valid = gtk_q_sort_filter_tree_model_source_index_to_iter(
                GTK_Q_SORT_FILTER_TREE_MODEL(model), idx, &new_iter);
        }

        if (valid) {
            if (GTK_IS_TREE_MODEL_FILTER(filter_model)) {
                GtkTreeIter filter_iter;
                if (gtk_tree_model_filter_convert_child_iter_to_iter(
                        GTK_TREE_MODEL_FILTER(filter_model),
                        &filter_iter,
                        &new_iter)
                ) {
                    gtk_combo_box_set_active_iter(box, &filter_iter);
                } else {
                    g_warning("failed to convert iter from source model to filter model iter");
                }
            } else {
                gtk_combo_box_set_active_iter(box, &new_iter);
            }
        } else {
            g_warning("Given QModelIndex doesn't exist in GtkTreeModel");
        }
    }
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

    /* use a filter model to remove disabled items */
    GtkTreeModel *filter_model = gtk_tree_model_filter_new(model, NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filter_model),
                                           (GtkTreeModelFilterVisibleFunc)filter_disabled_items,
                                           NULL, NULL);

    gtk_combo_box_set_model(box, GTK_TREE_MODEL(filter_model));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(box), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(box), renderer,
                                   "text", 0, NULL);

   /* sync the initial selection */
   gtk_combo_box_set_active_index(box, selection_model->currentIndex());

    /* connect signals to and from the selection model */
    connection = QObject::connect(
        selection_model,
        &QItemSelectionModel::currentChanged,
        [=](const QModelIndex current, G_GNUC_UNUSED const QModelIndex & previous) {
            gtk_combo_box_set_active_index(box, current);
        }
    );
    g_signal_connect(box,
                     "changed",
                     G_CALLBACK(update_selection),
                     selection_model);

    return connection;
}
