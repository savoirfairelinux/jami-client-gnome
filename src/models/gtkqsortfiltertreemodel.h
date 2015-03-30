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

#ifndef GTK_Q_SORT_FILTER_TREE_MODEL_H_
#define GTK_Q_SORT_FILTER_TREE_MODEL_H_

#include <gtk/gtk.h>
#include <QtCore/QSortFilterProxyModel>
#include "gtkaccessproxymodel.h"

G_BEGIN_DECLS

#define GTK_TYPE_Q_SORT_FILTER_TREE_MODEL           (gtk_q_sort_filter_tree_model_get_type ())
#define GTK_Q_SORT_FILTER_TREE_MODEL(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_Q_SORT_FILTER_TREE_MODEL, GtkQSortFilterTreeModel))
#define GTK_Q_SORT_FILTER_TREE_MODEL_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass),  GTK_TYPE_Q_SORT_FILTER_TREE_MODEL, GtkQSortFilterTreeModelClass))
#define GTK_IS_Q_SORT_FILTER_TREE_MODEL(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_Q_SORT_FILTER_TREE_MODEL))
#define GTK_IS_Q_SORT_FILTER_TREE_MODEL_CLASS(klass)(G_TYPE_CHECK_CLASS_TYPE ((klass),  GTK_TYPE_Q_SORT_FILTER_TREE_MODEL))
#define GTK_Q_SORT_FILTER_TREE_MODEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),  GTK_TYPE_Q_SORT_FILTER_TREE_MODEL, GtkQSortFilterTreeModelClass))

typedef struct _GtkQSortFilterTreeModel        GtkQSortFilterTreeModel;
typedef struct _GtkQSortFilterTreeModelClass   GtkQSortFilterTreeModelClass;

GType                    gtk_q_sort_filter_tree_model_get_type            (void) G_GNUC_CONST;
GtkQSortFilterTreeModel *gtk_q_sort_filter_tree_model_new                 (QSortFilterProxyModel *, size_t, ...);
QSortFilterProxyModel   *gtk_q_sort_filter_tree_model_get_qmodel          (GtkQSortFilterTreeModel *);
QModelIndex              gtk_q_sort_filter_tree_model_get_source_idx      (GtkQSortFilterTreeModel *, GtkTreeIter *);
gboolean                 gtk_q_sort_filter_tree_model_source_index_to_iter(GtkQSortFilterTreeModel *, const QModelIndex &, GtkTreeIter *);

G_END_DECLS

#endif /* GTK_Q_SORT_FILTER_TREE_MODEL_H_ */
