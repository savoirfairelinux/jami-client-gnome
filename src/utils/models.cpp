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

#include "models.h"

#include "../models/gtkqtreemodel.h"
#include <QtCore/QItemSelectionModel>

QModelIndex
get_index_from_selection(GtkTreeSelection *selection)
{
    GtkTreeIter iter;
    GtkTreeModel *model = NULL;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        if (GTK_IS_Q_TREE_MODEL(model))
            return gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
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
    selection_model->clearSelection();
}

void
gtk_combo_box_set_active_index(GtkComboBox *box, const QModelIndex& idx)
{
    if (idx.isValid()) {
        GtkTreeIter new_iter;
        GtkTreeModel *model = gtk_combo_box_get_model(box);

        g_return_if_fail(GTK_IS_Q_TREE_MODEL(model));

        if (gtk_q_tree_model_source_index_to_iter(GTK_Q_TREE_MODEL(model), idx, &new_iter)) {
            gtk_combo_box_set_active_iter(box, &new_iter);
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

    model = (GtkTreeModel *)gtk_q_tree_model_new(
        qmodel,
        1,
        0, Qt::DisplayRole, G_TYPE_STRING);

    gtk_combo_box_set_model(box, GTK_TREE_MODEL(model));

    if (!selection_model) return connection;

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

QMetaObject::Connection
gtk_combo_box_set_qmodel_text(GtkComboBox *box, QAbstractItemModel *qmodel, QItemSelectionModel *selection_model)
{
    auto connection = gtk_combo_box_set_qmodel(box, qmodel, selection_model);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(box), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(box), renderer,
                                   "text", 0, NULL);

    return connection;
}
