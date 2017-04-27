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

#ifndef GTK_Q_TREE_MODEL_H_
#define GTK_Q_TREE_MODEL_H_

#include <gtk/gtk.h>
#include <QtCore/QAbstractItemModel>
#include "gtkaccessproxymodel.h"

G_BEGIN_DECLS

#define GTK_TYPE_Q_TREE_MODEL           (gtk_q_tree_model_get_type ())
#define GTK_Q_TREE_MODEL(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_Q_TREE_MODEL, GtkQTreeModel))
#define GTK_Q_TREE_MODEL_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass),  GTK_TYPE_Q_TREE_MODEL, GtkQTreeModelClass))
#define GTK_IS_Q_TREE_MODEL(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_Q_TREE_MODEL))
#define GTK_IS_Q_TREE_MODEL_CLASS(klass)(G_TYPE_CHECK_CLASS_TYPE ((klass),  GTK_TYPE_Q_TREE_MODEL))
#define GTK_Q_TREE_MODEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),  GTK_TYPE_Q_TREE_MODEL, GtkQTreeModelClass))

typedef struct _GtkQTreeModel        GtkQTreeModel;
typedef struct _GtkQTreeModelClass   GtkQTreeModelClass;

GType               gtk_q_tree_model_get_type            (void) G_GNUC_CONST;
GtkQTreeModel      *gtk_q_tree_model_new                 (QAbstractItemModel *, size_t, ...);
QAbstractItemModel *gtk_q_tree_model_get_qmodel          (GtkQTreeModel *);
QModelIndex         gtk_q_tree_model_get_source_idx      (GtkQTreeModel *, GtkTreeIter *);
gboolean            gtk_q_tree_model_source_index_to_iter(GtkQTreeModel *, const QModelIndex &, GtkTreeIter *);
gboolean            gtk_q_tree_model_is_layout_changing  (GtkQTreeModel *);

/**
 * Helper function which takes a GtkTreeSelection which comes from a GtkTreeView which is using a
 * GtkQTreeModel and returns TRUE if the layout is changing and so the selection change should be
 * ignored. Returns FALSE otherwise
 */
gboolean gtk_q_tree_model_ignore_selection_change (GtkTreeSelection *);

G_END_DECLS

#endif /* GTK_Q_TREE_MODEL_H_ */
