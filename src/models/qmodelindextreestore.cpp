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

#include "qmodelindextreestore.h"

#include <QtCore/QAbstractItemModel>

typedef struct _QModelIndexTreeStorePrivate QModelIndexTreeStorePrivate;

struct _QModelIndexTreeStore
{
    GObject parent;
};

struct _QModelIndexTreeStoreClass
{
    GObjectClass parent_class;
};

struct _QModelIndexTreeStorePrivate
{
    GtkTreeModel       *gtk_model;
    QAbstractItemModel *qt_model;

    /* connections */
};

/* define type, inherit from GObject, define implemented interface(s) */
G_DEFINE_TYPE_WITH_CODE (QModelIndexTreeStore, qmodelindex_tree_store, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (QModelIndexTreeStore)
       G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
            qmodelindex_tree_store_tree_model_init))

#define QMODELINDEX_TREE_STORE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), QMODELINDEX_TREE_STORE_TYPE, QModelIndexTreeStorePrivate))

static void
qmodelindex_tree_store_class_init(QModelIndexTreeStoreClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = qmodelindex_tree_store_finalize;
}

static void
qmodelindex_tree_store_init(QModelIndexTreeStore *q_tree_model)
{
    QModelIndexTreeStorePrivate *priv = QMODELINDEX_TREE_STORE_GET_PRIVATE(q_tree_model);
    q_tree_model->priv = priv;

    priv->stamp = g_random_int();
    priv->model = NULL;
}

/**
 * qmodelindex_tree_store_get_qmodel
 * returns the original model from which this QModelIndexTreeStore is created
 */
QAbstractItemModel *
qmodelindex_tree_store_get_qmodel(QModelIndexTreeStore *q_tree_model)
{
    QModelIndexTreeStorePrivate *priv = QMODELINDEX_TREE_STORE_GET_PRIVATE(q_tree_model);
    return priv->qt_model;
}

/**
 * qmodelindex_tree_store_get_source_idx
 * Returns the index of the original model used to create this QModelIndexTreeStore from
 * the given iter, if there is one.
 */
QModelIndex
qmodelindex_tree_store_get_source_idx(QModelIndexTreeStore *q_tree_model, GtkTreeIter *iter)
{
    QModelIndexTreeStorePrivate *priv = QMODELINDEX_TREE_STORE_GET_PRIVATE(q_tree_model);
    /* get the call */
    QIter *qiter = Q_ITER(iter);
    GtkAccessProxyModel *proxy_model = priv->model;
    QModelIndex proxy_idx = proxy_model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
    if (proxy_idx.isValid()) {
        /* we have the proxy model idx, now get the actual idx so we can get the call object */
        return proxy_model->mapToSource(proxy_idx);
    } else {
        g_warning("could not get valid QModelIndex from given GtkTreeIter");
        return QModelIndex();
    }
}

/**
 * Takes a QModelIndex from the original QAbstractItemModel and returns a valid GtkTreeIter in the corresponding
 * QModelIndexTreeStore
 */
gboolean
qmodelindex_tree_store_source_index_to_iter(QModelIndexTreeStore *q_tree_model, const QModelIndex &idx, GtkTreeIter *iter)
{
    QModelIndexTreeStorePrivate *priv = QMODELINDEX_TREE_STORE_GET_PRIVATE(q_tree_model);

    /* the the proxy_idx from the source idx */
    QModelIndex proxy_idx = priv->model->mapFromSource(idx);
    if (!proxy_idx.isValid())
        return FALSE;

    /* make sure iter is valid */
    iter->stamp = priv->stamp;

    /* map the proxy idx to iter */
    Q_ITER(iter)->row.value = proxy_idx.row();
    Q_ITER(iter)->column.value = proxy_idx.column();
    Q_ITER(iter)->id = proxy_idx.internalPointer();
    return TRUE;
}

/**
 * qmodelindex_tree_store_new:
 * @model: QAbstractItemModel to which this model will bind.
 * @n_columns: number of columns in the list store
 * @...: all #GType follwed by the #Role pair for each column.
 *
 * Return value: a new #QModelIndexTreeStore
 */
QModelIndexTreeStore *
qmodelindex_tree_store_new(QAbstractItemModel *model)
{
    g_return_val_if_fail(model, NULL);

    retval = (QModelIndexTreeStore *)g_object_new(QMODELINDEX_TREE_STORE_TYPE, NULL);
    qmodelindex_tree_store_set_n_columns(retval, n_columns);

    /* get proxy model from given model */
    GtkAccessProxyModel* proxy_model = new GtkAccessProxyModel();
    proxy_model->setSourceModel(model);
    retval->priv->model = proxy_model;
    gint stamp = retval->priv->stamp;

    n_columns = 2*n_columns;
    va_start (args, n_columns);

    for (i = 0; i < (gint)(n_columns/2); i++) {
        /* first get the role of the QModel */
        gint role = va_arg(args, gint);

        /* then get the type the role will be interpreted as */
        GType type = va_arg(args, GType);

        /* TODO: check if its a type supported by our model
         * if (! _gtk_tree_data_list_check_type (type))
         *   {
         *     g_warning ("%s: Invalid type %s\n", G_STRLOC, g_type_name (type));
         *     g_object_unref (retval);
         *     va_end (args);
         *
         *     return NULL;
         *   }
         */

        qmodelindex_tree_store_set_column_type (retval, i, role, type);
    }

    va_end (args);

    qmodelindex_tree_store_length(retval);

    /* connect signals */
    QObject::connect(
        proxy_model,
        &QAbstractItemModel::rowsInserted,
        [=](const QModelIndex & parent, int first, int last) {
            for( int row = first; row <= last; row++) {
                GtkTreeIter iter;
                QModelIndex idx = proxy_model->index(row, 0, parent);
                iter.stamp = stamp;
                qmodelindex_to_iter(idx, &iter);
                GtkTreePath *path = qmodelindex_tree_store_get_path(GTK_TREE_MODEL(retval), &iter);
                gtk_tree_model_row_inserted(GTK_TREE_MODEL(retval), path, &iter);
                gtk_tree_path_free(path);
            }
        }
    );

    QObject::connect(
        proxy_model,
        &QAbstractItemModel::rowsAboutToBeMoved,
        [=](const QModelIndex & sourceParent, int sourceStart, int sourceEnd,
            const QModelIndex & destinationParent, G_GNUC_UNUSED int destinationRow) {
            /* if the sourceParent and the destinationParent are the same, then we can use the
             * GtkTreeModel API to move the rows, otherwise we must first delete them and then
             * re-insert them under the new parent */
            if (sourceParent == destinationParent)
                return;

            /* first remove the row from old location
             * then insert them at the new location on the "rowsMoved signal */
            for( int row = sourceStart; row <= sourceEnd; row++) {
                QModelIndex idx = proxy_model->index(row, 0, sourceParent);
                GtkTreeIter iter_old;
                iter_old.stamp = stamp;
                qmodelindex_to_iter(idx, &iter_old);
                GtkTreePath *path_old = qmodelindex_tree_store_get_path(GTK_TREE_MODEL(retval), &iter_old);
                gtk_tree_model_row_deleted(GTK_TREE_MODEL(retval), path_old);
                gtk_tree_path_free(path_old);
            }
        }
    );

    QObject::connect(
        proxy_model,
        &QAbstractItemModel::rowsMoved,
        [=](const QModelIndex & sourceParent, int sourceStart, int sourceEnd,
            const QModelIndex & destinationParent, int destinationRow) {
            /* if the sourceParent and the destinationParent are the same, then we can use the
             * GtkTreeModel API to move the rows, otherwise we must first delete them and then
             * re-insert them under the new parent */
            if (sourceParent == destinationParent) {
                GtkTreeIter *iter_parent = nullptr;
                GtkTreePath *path_parent = nullptr;
                if (sourceParent.isValid()) {
                    iter_parent = g_new0(GtkTreeIter, 1);
                    iter_parent->stamp = stamp;
                    qmodelindex_to_iter(sourceParent, iter_parent);
                    path_parent = qmodelindex_tree_store_get_path(GTK_TREE_MODEL(retval), iter_parent);
                } else {
                    path_parent = gtk_tree_path_new();
                }

                /* gtk_tree_model_rows_reordered takes an array of integers mapping the current
                 * position of each child to its old position before the re-ordering,
                 * i.e. new_order [newpos] = oldpos
                 */
                const auto rows = proxy_model->rowCount(sourceParent);
                gint new_order[rows];
                const auto destinationRowLast = destinationRow + (sourceEnd - sourceStart);
                const auto totalMoved = sourceEnd - sourceStart + 1;
                for (int row = 0; row < rows; ++row ) {
                    if ( (row < sourceStart && row < destinationRow)
                         || (row > sourceEnd && row > destinationRowLast) ) {
                        // if the row is outside of the bounds of change, then it stayed in the same place
                        new_order[row] = row;
                    } else {
                        if (row < destinationRow) {
                            // in front of the destination, so it used to be behind the rows that moved
                            new_order[row] = row + totalMoved;
                        } else if (row >= destinationRow && row <= destinationRowLast) {
                            // within the destination, so it was within the rows that moved
                            new_order[row] = sourceStart + (row - destinationRow);
                        } else {
                            // behind the destination, so it used to be in front of the rows that moved
                            new_order[row] = row - totalMoved;
                        }
                    }
                }
                gtk_tree_model_rows_reordered(GTK_TREE_MODEL(retval), path_parent, iter_parent, new_order);

                g_free(iter_parent);
                gtk_tree_path_free(path_parent);
            } else {
                /* these rows should have been removed in the "rowsAboutToBeMoved" handler
                 * now insert them in the new location */
                for( int row = sourceStart; row <= sourceEnd; row++) {
                    GtkTreeIter iter_new;
                    QModelIndex idx = proxy_model->index(destinationRow, 0, destinationParent);
                    iter_new.stamp = stamp;
                    qmodelindex_to_iter(idx, &iter_new);
                    GtkTreePath *path_new = qmodelindex_tree_store_get_path(GTK_TREE_MODEL(retval), &iter_new);
                    gtk_tree_model_row_inserted(GTK_TREE_MODEL(retval), path_new, &iter_new);
                    gtk_tree_path_free(path_new);
                    insert_children(idx, retval);
                    destinationRow++;
                }
            }
        }
    );

    QObject::connect(
        proxy_model,
        &QAbstractItemModel::rowsRemoved,
        [=](const QModelIndex & parent, int first, int last) {
            GtkTreePath *parent_path = NULL;
            if (parent.isValid()) {
                GtkTreeIter iter_parent;
                iter_parent.stamp = stamp;
                qmodelindex_to_iter(parent, &iter_parent);
                parent_path = qmodelindex_tree_store_get_path(GTK_TREE_MODEL(retval), &iter_parent);
            } else {
                parent_path = gtk_tree_path_new();
            }

            /* go last to first, since the rows are being deleted */
            for( int row = last; row >= first; --row) {
                // g_debug("deleting row: %d", row);
                GtkTreePath *path = gtk_tree_path_copy(parent_path);
                gtk_tree_path_append_index(path, row);
                gtk_tree_model_row_deleted(GTK_TREE_MODEL(retval), path);
                gtk_tree_path_free(path);
            }

            gtk_tree_path_free(parent_path);
        }
    );

    QObject::connect(
        proxy_model,
        &QAbstractItemModel::dataChanged,
        [=](const QModelIndex & topLeft, const QModelIndex & bottomRight,
            G_GNUC_UNUSED const QVector<int> & roles = QVector<int> ()) {
            /* we have to assume only one column */
            int first = topLeft.row();
            int last = bottomRight.row();
            if (topLeft.column() != bottomRight.column() ) {
                g_warning("more than one column is not supported!");
            }
            /* the first idx IS topLeft, the reset are his siblings */
            GtkTreeIter iter;
            QModelIndex idx = topLeft;
            iter.stamp = stamp;
            qmodelindex_to_iter(idx, &iter);
            GtkTreePath *path = qmodelindex_tree_store_get_path(GTK_TREE_MODEL(retval), &iter);
            gtk_tree_model_row_changed(GTK_TREE_MODEL(retval), path, &iter);
            gtk_tree_path_free(path);
            for( int row = first + 1; row <= last; row++) {
                idx = topLeft.sibling(row, 0);
                qmodelindex_to_iter(idx, &iter);
                path = qmodelindex_tree_store_get_path(GTK_TREE_MODEL(retval), &iter);
                gtk_tree_model_row_changed(GTK_TREE_MODEL(retval), path, &iter);
                gtk_tree_path_free(path);
            }
        }
    );

    QObject::connect(
        proxy_model,
        &QAbstractItemModel::modelAboutToBeReset,
        [=] () {
            // g_debug("model about to be reset");
            /* nothing equvivalent eixists in GtkTreeModel, so simply delete all
             * rows, and add all rows when the model is reset;
             * we must delete the rows in ascending order */
            int row_count = proxy_model->rowCount();
            for (int row = row_count; row > 0; --row) {
                // g_debug("deleting row %d", row -1);
                QModelIndex idx = proxy_model->index(row - 1, 0);
                GtkTreeIter iter;
                iter.stamp = stamp;
                qmodelindex_to_iter(idx, &iter);
                GtkTreePath *path = qmodelindex_tree_store_get_path(GTK_TREE_MODEL(retval), &iter);
                gtk_tree_model_row_deleted(GTK_TREE_MODEL(retval), path);
                gtk_tree_path_free(path);
            }
        }
    );

    QObject::connect(
        proxy_model,
        &QAbstractItemModel::modelReset,
        [=] () {
            // g_debug("model reset");
            /* now recursively add all the (new) rows */
            insert_children(QModelIndex(), retval);
        }
    );

//     QObject::connect(
//         proxy_model,
//         &QAbstractItemModel::layoutAboutToBeChanged,
//         [=] () {
//             // g_debug("layout about to be changed");
//             /* nothing equvivalent eixists in GtkTreeModel, so simply delete all
//              * rows, and add all rows when the model is reset;
//              * we must delete the rows in ascending order */
//             int row_count = proxy_model->rowCount();
//             for (int row = row_count; row > 0; --row) {
//                 // g_debug("deleting row %d", row -1);
//                 QModelIndex idx = proxy_model->index(row - 1, 0);
//                 GtkTreeIter iter;
//                 iter.stamp = stamp;
//                 qmodelindex_to_iter(idx, &iter);
//                 GtkTreePath *path = qmodelindex_tree_store_get_path(GTK_TREE_MODEL(retval), &iter);
//                 gtk_tree_model_row_deleted(GTK_TREE_MODEL(retval), path);
//             }
//         }
//     );
//
//     QObject::connect(
//         proxy_model,
//         &QAbstractItemModel::layoutChanged,
//         [=] () {
//             // g_debug("layout changed");
//             /* now add all the (new) rows */
//             int row_count = proxy_model->rowCount();
//             for (int row = 0; row < row_count; row++) {
//                 // g_debug("adding row %d", row);
//                 GtkTreeIter *iter_new = g_new0(GtkTreeIter, 1);
//                 QModelIndex idx = proxy_model->index(row, 0);
//                 iter_new->stamp = stamp;
//                 qmodelindex_to_iter(idx, iter_new);
//                 GtkTreePath *path_new = qmodelindex_tree_store_get_path(GTK_TREE_MODEL(retval), iter_new);
//                 gtk_tree_model_row_inserted(GTK_TREE_MODEL(retval), path_new, iter_new);
//             }
//         }
//     );

    return retval;
}

static gint
qmodelindex_tree_store_length(QModelIndexTreeStore *q_tree_model)
{
    QModelIndexTreeStorePrivate *priv = q_tree_model->priv;
    return priv->model->rowCount();
}

static void
qmodelindex_tree_store_set_n_columns(QModelIndexTreeStore *q_tree_model,
                               gint          n_columns)
{
    QModelIndexTreeStorePrivate *priv = q_tree_model->priv;
    int i;

    if (priv->n_columns == n_columns)
        return;

    priv->column_headers = g_renew (GType, priv->column_headers, n_columns);
    for (i = priv->n_columns; i < n_columns; i++)
        priv->column_headers[i] = G_TYPE_INVALID;
    priv->n_columns = n_columns;

    priv->column_roles = g_renew (gint, priv->column_roles, n_columns);
    for (i = priv->n_columns; i < n_columns; i++)
        priv->column_roles[i] = -1;
}

static void
qmodelindex_tree_store_set_column_type(QModelIndexTreeStore *q_tree_model,
                                 gint          column,
                                 gint          role,
                                 GType         type)
{
  QModelIndexTreeStorePrivate *priv = q_tree_model->priv;

  /* TODO: check if its a type supported by our model
  * if (!_gtk_tree_data_list_check_type (type))
  *   {
  *     g_warning ("%s: Invalid type %s\n", G_STRLOC, g_type_name (type));
  *     return;
  *   }
  */

  priv->column_headers[column] = type;
  priv->column_roles[column] = role;
}


static void
qmodelindex_tree_store_finalize(GObject *object)
{
    QModelIndexTreeStore *q_tree_model = QMODELINDEX_TREE_STORE (object);
    QModelIndexTreeStorePrivate *priv = q_tree_model->priv;

    g_free(priv->column_headers);
    g_free(priv->column_roles);

    /* delete the created proxy model */
    delete priv->model;

    G_OBJECT_CLASS(qmodelindex_tree_store_parent_class)->finalize (object);
}

static gboolean
iter_is_valid(GtkTreeIter *iter,
              QModelIndexTreeStore   *q_tree_model)
{
    gboolean retval;
    g_return_val_if_fail(iter != NULL, FALSE);
    QIter *qiter = Q_ITER(iter);
    retval = q_tree_model->priv->model->indexFromId(qiter->row.value, qiter->column.value, qiter->id).isValid();
    return retval;
}

static void
qmodelindex_to_iter(const QModelIndex &idx, GtkTreeIter *iter)
{
    Q_ITER(iter)->row.value = idx.row();
    Q_ITER(iter)->column.value = idx.column();
    Q_ITER(iter)->id = idx.internalPointer();
}

static gboolean
validate_index(gint stamp, const QModelIndex &idx, GtkTreeIter *iter)
{
    if (idx.isValid()) {
        iter->stamp = stamp;
        qmodelindex_to_iter(idx, iter);
    } else {
        iter->stamp = 0;
        return FALSE;
    }
    return TRUE;
}

/* Start Fulfill the GtkTreeModel requirements */

/* flags supported by this interface */
static GtkTreeModelFlags
qmodelindex_tree_store_get_flags(G_GNUC_UNUSED GtkTreeModel *tree_model)
{
    // TODO: possibly return based on the model?
    return (GtkTreeModelFlags)(GTK_TREE_MODEL_ITERS_PERSIST);
}

/* number of columns supported by this tree model */
static gint
qmodelindex_tree_store_get_n_columns(GtkTreeModel *tree_model)
{
    QModelIndexTreeStore *q_tree_model = QMODELINDEX_TREE_STORE(tree_model);
    QModelIndexTreeStorePrivate *priv = q_tree_model->priv;

    return priv->model->columnCount();
}

/* get given column type */
static GType
qmodelindex_tree_store_get_column_type(GtkTreeModel *tree_model,
                                 gint          index)
{
    QModelIndexTreeStore *q_tree_model = QMODELINDEX_TREE_STORE (tree_model);
    QModelIndexTreeStorePrivate *priv = q_tree_model->priv;

    g_return_val_if_fail (index < qmodelindex_tree_store_get_n_columns(tree_model), G_TYPE_INVALID);

    return priv->column_headers[index];
}

/* Sets @iter to a valid iterator pointing to @path.  If @path does
 * not exist, @iter is set to an invalid iterator and %FALSE is returned.
 */
static gboolean
qmodelindex_tree_store_get_iter(GtkTreeModel *tree_model,
                          GtkTreeIter  *iter,
                          GtkTreePath  *path)
{
    QModelIndexTreeStore *q_tree_model = QMODELINDEX_TREE_STORE (tree_model);
    QModelIndexTreeStorePrivate *priv = q_tree_model->priv;

    /* GtkTreePath is a list of indices, each one indicates
     * the child of the previous one.
     * Since GtkTreeModel only has rows (columns are only used to
     * store data in each item), each index is basically the row
     * at the given tree level.
     * To get the iter, we want to get the QModelIndex. To get
     * the QModelIndex we simply start at the first level and
     * traverse the model the number of layers equal to the number
     * of indices in the path.
     */
    gint depth;
    gint* indices = gtk_tree_path_get_indices_with_depth(path, &depth);
    QModelIndex idx = priv->model->index(indices[0], 0);
    for(int layer = 1; layer < depth; layer++ ) {
        /* check if previous iter is valid */
        if (!idx.isValid()) {
            break;
        } else {
            idx = idx.child(indices[layer], 0);
        }
    }

    if (!idx.isValid()) {
        iter->stamp = 0;
        return FALSE;
    } else {
        /* we have a valid QModelIndex; turn it into an iter */
        iter->stamp = priv->stamp;
        qmodelindex_to_iter(idx, iter);
    }

    return TRUE;
}

/* Returns a newly-created #GtkTreePath referenced by @iter.
 *
 * This path should be freed with gtk_tree_path_free().
 */
static GtkTreePath *
qmodelindex_tree_store_get_path(GtkTreeModel *tree_model,
                          GtkTreeIter  *iter)
{
    QModelIndexTreeStore *q_tree_model = QMODELINDEX_TREE_STORE (tree_model);
    QModelIndexTreeStorePrivate *priv = q_tree_model->priv;
    GtkTreePath *path;

    g_return_val_if_fail (iter->stamp == priv->stamp, NULL);
    g_return_val_if_fail (iter_is_valid(iter, q_tree_model), NULL);

    /* To get the path, we have to traverse from the child all the way up */
    path = gtk_tree_path_new();
    QIter *qiter = Q_ITER(iter);
    QModelIndex idx = priv->model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
    while( idx.isValid() ){
        gtk_tree_path_prepend_index(path, idx.row());
        idx = idx.parent();
    }
    return path;
}

static inline GType
get_fundamental_type(GType type)
{
    GType result;

    result = G_TYPE_FUNDAMENTAL (type);

    if (result == G_TYPE_INTERFACE) {
        if (g_type_is_a (type, G_TYPE_OBJECT))
            result = G_TYPE_OBJECT;
    }

    return result;
}

/* Initializes and sets @value to that at @column. */
static void
qmodelindex_tree_store_get_value(GtkTreeModel *tree_model,
                           GtkTreeIter  *iter,
                           gint          column,
                           GValue       *value)
{
    QModelIndexTreeStore *q_tree_model = QMODELINDEX_TREE_STORE (tree_model);
    QModelIndexTreeStorePrivate *priv = q_tree_model->priv;

    g_return_if_fail (column < priv->n_columns);
    g_return_if_fail (iter_is_valid(iter, q_tree_model));

    /* get the data */
    QIter *qiter = Q_ITER(iter);
    QModelIndex idx = priv->model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
    int role = priv->column_roles[column];
    QVariant var = priv->model->data(idx, role);
    GType type = priv->column_headers[column];
    g_value_init (value, type);
    switch (get_fundamental_type (type))
    {
        /* TODO: implement the rest of the data types the we plan to support
         *       otherwise get rid of the code and make sure un-implemented types
         *       are not allowed
         */
        case G_TYPE_BOOLEAN:
            g_value_set_boolean (value, (gboolean) var.toBool());
            break;
        // case G_TYPE_CHAR:
        //   g_value_set_schar (value, (gchar) list->data.v_char);
        //   break;
        // case G_TYPE_UCHAR:
        //   g_value_set_uchar (value, (guchar) list->data.v_uchar);
        //   break;
        case G_TYPE_INT:
            g_value_set_int (value, (gint) var.toInt());
            break;
        case G_TYPE_UINT:
            g_value_set_uint (value, (guint) var.toUInt());
            break;
        // case G_TYPE_LONG:
        //   g_value_set_long (value, list->data.v_long);
        //   break;
        // case G_TYPE_ULONG:
        //   g_value_set_ulong (value, list->data.v_ulong);
        //   break;
        // case G_TYPE_INT64:
        //   g_value_set_int64 (value, list->data.v_int64);
        //   break;
        // case G_TYPE_UINT64:
        //   g_value_set_uint64 (value, list->data.v_uint64);
        //   break;
        // case G_TYPE_ENUM:
        //   g_value_set_enum (value, list->data.v_int);
        //   break;
        // case G_TYPE_FLAGS:
        //   g_value_set_flags (value, list->data.v_uint);
        //   break;
        // case G_TYPE_FLOAT:
        //   g_value_set_float (value, (gfloat) list->data.v_float);
        //   break;
        // case G_TYPE_DOUBLE:
        //   g_value_set_double (value, (gdouble) list->data.v_double);
        //   break;
        case G_TYPE_STRING:
        {
            QByteArray ba = var.toString().toUtf8();
            g_value_set_string(value, (gchar *)var.toByteArray().constData());
        }
        break;
        // case G_TYPE_POINTER:
        //   g_value_set_pointer (value, (gpointer) list->data.v_pointer);
        //   break;
        // case G_TYPE_BOXED:
        //   g_value_set_boxed (value, (gpointer) list->data.v_pointer);
        //   break;
        // case G_TYPE_VARIANT:
        //   g_value_set_variant (value, (gpointer) list->data.v_pointer);
        //   break;
        // case G_TYPE_OBJECT:
        //   g_value_set_object (value, (GObject *) list->data.v_pointer);
        //   break;
        default:
            g_warning ("%s: Unsupported type (%s) retrieved.", G_STRLOC, g_type_name (value->g_type));
            break;
    }

    return;
}

/* Sets @iter to point to the node following it at the current level.
 *
 * If there is no next @iter, %FALSE is returned and @iter is set
 * to be invalid.
 */
static gboolean
qmodelindex_tree_store_iter_next(GtkTreeModel  *tree_model,
                           GtkTreeIter   *iter)
{
    QModelIndexTreeStore *q_tree_model = QMODELINDEX_TREE_STORE (tree_model);
    QModelIndexTreeStorePrivate *priv = q_tree_model->priv;

    g_return_val_if_fail (iter_is_valid(iter, q_tree_model), FALSE);

    QIter *qiter = Q_ITER(iter);
    QModelIndex idx = priv->model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
    idx = idx.sibling(idx.row()+1, idx.column());

    /* validate */
    if (validate_index(priv->stamp, idx, iter) ) {
        GtkTreePath *path = qmodelindex_tree_store_get_path(tree_model, iter);
        gtk_tree_path_free(path);
        return TRUE;
    } else {
        return FALSE;
    }
}

/* Sets @iter to point to the previous node at the current level.
 *
 * If there is no previous @iter, %FALSE is returned and @iter is
 * set to be invalid.
 */
static gboolean
qmodelindex_tree_store_iter_previous(GtkTreeModel *tree_model,
                               GtkTreeIter  *iter)
{
    QModelIndexTreeStore *q_tree_model = QMODELINDEX_TREE_STORE (tree_model);
    QModelIndexTreeStorePrivate *priv = q_tree_model->priv;

    g_return_val_if_fail (iter_is_valid(iter, q_tree_model), FALSE);

    QIter *qiter = Q_ITER(iter);
    QModelIndex idx = priv->model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
    idx = idx.sibling(idx.row()-1, idx.column());

    /* validate */
    return validate_index(priv->stamp, idx, iter);
}

/* Sets @iter to point to the first child of @parent.
 *
 * If @parent has no children, %FALSE is returned and @iter is
 * set to be invalid. @parent will remain a valid node after this
 * function has been called.
 *
 * If @parent is %NULL returns the first node
 */
static gboolean
qmodelindex_tree_store_iter_children(GtkTreeModel *tree_model,
                               GtkTreeIter  *iter,
                               GtkTreeIter  *parent)
{
    QModelIndexTreeStore *q_tree_model = QMODELINDEX_TREE_STORE (tree_model);
    QModelIndexTreeStorePrivate *priv = q_tree_model->priv;
    QModelIndex idx;

    /* make sure parent if valid node if its not NULL */
    if (parent)
        g_return_val_if_fail(iter_is_valid(parent, q_tree_model), FALSE);

    if (parent) {
        /* get first child */
        QIter *qparent = Q_ITER(parent);
        idx = priv->model->indexFromId(qparent->row.value, qparent->column.value, qparent->id);
        idx = idx.child(0, 0);
    } else {
        /* parent is NULL, get the first node */
        idx = priv->model->index(0, 0);
    }

    /* validate child */
    return validate_index(priv->stamp, idx, iter);
}

/* Returns %TRUE if @iter has children, %FALSE otherwise. */
static gboolean
qmodelindex_tree_store_iter_has_child (GtkTreeModel *tree_model,
                   GtkTreeIter  *iter)
{
    QModelIndexTreeStore *q_tree_model = QMODELINDEX_TREE_STORE (tree_model);
    QModelIndexTreeStorePrivate *priv = q_tree_model->priv;
    g_return_val_if_fail(iter_is_valid(iter, q_tree_model), FALSE);

    QIter *qiter = Q_ITER(iter);
    QModelIndex idx = priv->model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
    return priv->model->hasChildren(idx);
}

/* Returns the number of children that @iter has.
 *
 * As a special case, if @iter is %NULL, then the number
 * of toplevel nodes is returned.
 */
static gint
qmodelindex_tree_store_iter_n_children (GtkTreeModel *tree_model,
                GtkTreeIter  *iter)
{
    QModelIndexTreeStore *q_tree_model = QMODELINDEX_TREE_STORE (tree_model);
    QModelIndexTreeStorePrivate *priv = q_tree_model->priv;

    if (iter == NULL)
      return qmodelindex_tree_store_length(q_tree_model);

    g_return_val_if_fail(iter_is_valid(iter, q_tree_model), -1);
    QIter *qiter = Q_ITER(iter);
    QModelIndex idx = priv->model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
    return priv->model->rowCount(idx);
}

/* Sets @iter to be the child of @parent, using the given index.
 *
 * The first index is 0. If @n is too big, or @parent has no children,
 * @iter is set to an invalid iterator and %FALSE is returned. @parent
 * will remain a valid node after this function has been called. As a
 * special case, if @parent is %NULL, then the @n<!-- -->th root node
 * is set.
 */
static gboolean
qmodelindex_tree_store_iter_nth_child(GtkTreeModel *tree_model,
                                GtkTreeIter  *iter,
                                GtkTreeIter  *parent,
                                gint          n)
{
    QModelIndexTreeStore *q_tree_model = QMODELINDEX_TREE_STORE (tree_model);
    QModelIndexTreeStorePrivate *priv = q_tree_model->priv;
    QModelIndex idx;

    if (parent) {
        g_return_val_if_fail(iter_is_valid(parent, q_tree_model), FALSE);

        /* get the nth child */
        QIter *qparent = Q_ITER(parent);
        idx = priv->model->indexFromId(qparent->row.value, qparent->column.value, qparent->id);
        idx = idx.child(n, 0);
    } else {
        idx = priv->model->index(n, 0);
    }

    /* validate */
    return validate_index(priv->stamp, idx, iter);
}

/* Sets @iter to be the parent of @child.
 *
 * If @child is at the toplevel, and doesn't have a parent, then
 * @iter is set to an invalid iterator and %FALSE is returned.
 * @child will remain a valid node after this function has been
 * called.
 */
static gboolean
qmodelindex_tree_store_iter_parent(GtkTreeModel *tree_model,
                             GtkTreeIter  *iter,
                             GtkTreeIter  *child)
{
    QModelIndexTreeStore *q_tree_model = QMODELINDEX_TREE_STORE (tree_model);
    QModelIndexTreeStorePrivate *priv = q_tree_model->priv;
    QModelIndex idx;

    g_return_val_if_fail(iter_is_valid(child, q_tree_model), FALSE);

    QIter *qchild = Q_ITER(child);
    idx = priv->model->indexFromId(qchild->row.value, qchild->column.value, qchild->id);
    idx = idx.parent();

    /* validate */
    return validate_index(priv->stamp, idx, iter);
}

/* End Fulfill the GtkTreeModel requirements */
