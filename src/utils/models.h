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

#ifndef _MODELS_H
#define _MODELS_H

#include <gtk/gtk.h>
#include <QtCore/QMetaObject>

class QModelIndex;
class QAbstractItemModel;
class QItemSelectionModel;

QModelIndex
get_index_from_selection(GtkTreeSelection *selection);

QModelIndex
gtk_combo_box_get_index(GtkComboBox *box);

void
gtk_combo_box_set_active_index(GtkComboBox *box, const QModelIndex& idx);

/**
 * Helper method to automatically place the qmodel into the combobox and sync it with the
 * selection model (if given). Returns the connection to the QItemSelectionModel::currentChanged
 * signal of the given selection model.
 * This function doesn't set any GtkCellRenderer for the combo box, so nothing will be displayed
 * by default. If you simply want to display the Qt::DisplayRole of the model as text, you should
 * use gtk_combo_box_set_qmodel_text()
 */
QMetaObject::Connection
gtk_combo_box_set_qmodel(GtkComboBox *box, QAbstractItemModel *qmodel, QItemSelectionModel *selection_model);

/**
 * Like gtk_combo_box_set_qmodel, but sets a GtkCellRendererText for the combo box which will
 * display the Qt::DisplayRole as simple text. This function is a usefull default for most
 * combo boxes. If you want to customize the GtkCellRenderer of your combo box, you should usefull
 * gtk_combo_box_set_qmodel()
 */
QMetaObject::Connection
gtk_combo_box_set_qmodel_text(GtkComboBox *box, QAbstractItemModel *qmodel, QItemSelectionModel *selection_model);

#endif /* _MODELS_H */
