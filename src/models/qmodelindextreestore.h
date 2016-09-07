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
 */

#ifndef QMODELINDEXTREESTORE_H_
#define QMODELINDEXTREESTORE_H_

#include <gtk/gtk.h>

class QAbstractItemModel;

G_BEGIN_DECLS

#define QMODELINDEX_TREE_STORE_TYPE           (qmodelindex_tree_store_get_type ())
#define QMODELINDEX_TREE_STORE(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), QMODELINDEX_TREE_STORE_TYPE, QModelIndexTreeStore))
#define QMODELINDEX_TREE_STORE_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass),  QMODELINDEX_TREE_STORE_TYPE, QModelIndexTreeStoreClass))
#define IS_QMODELINDEX_TREE_STORE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), QMODELINDEX_TREE_STORE_TYPE))
#define IS_QMODELINDEX_TREE_STORE_CLASS(klass)(G_TYPE_CHECK_CLASS_TYPE ((klass),  QMODELINDEX_TREE_STORE_TYPE))
#define QMODELINDEX_TREE_STORE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),  QMODELINDEX_TREE_STORE_TYPE, QModelIndexTreeStoreClass))

typedef struct _QModelIndexTreeStore        QModelIndexTreeStore;
typedef struct _QModelIndexTreeStoreClass   QModelIndexTreeStoreClass;

GType                 qmodelindex_tree_store_get_type            (void) G_GNUC_CONST;
QModelIndexTreeStore *qmodelindex_tree_store_new                 (QAbstractItemModel *, size_t, ...);
QAbstractItemModel   *qmodelindex_tree_store_get_qmodel          (QModelIndexTreeStore *);
QModelIndex           qmodelindex_tree_store_get_source_idx      (QModelIndexTreeStore *, GtkTreeIter *);
gboolean              qmodelindex_tree_store_source_index_to_iter(QModelIndexTreeStore *, const QModelIndex &, GtkTreeIter *);

G_END_DECLS

#endif /* QMODELINDEXTREESTORE_H_ */
