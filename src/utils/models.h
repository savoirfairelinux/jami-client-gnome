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

QMetaObject::Connection
gtk_combo_box_set_qmodel(GtkComboBox *box, QAbstractItemModel *qmodel, QItemSelectionModel *selection_model);

#endif /* _MODELS_H */
