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

#include "gtkqsortfiltertreemodel.h"
#include <gtk/gtk.h>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QDebug>
#include "gtkaccessproxymodel.h"

typedef union _int_ptr_t
{
    gint value;
    void *ptr;
} int_ptr_t;

typedef struct _QIter {
    gint stamp;
    int_ptr_t row;
    int_ptr_t column;
    void *id;
    gpointer user_data;
} QIter;

typedef struct _GtkQSortFilterTreeModelPrivate GtkQSortFilterTreeModelPrivate;

struct _GtkQSortFilterTreeModel
{
    GObject parent;

    /* private */
    GtkQSortFilterTreeModelPrivate *priv;
};

struct _GtkQSortFilterTreeModelClass
{
    GObjectClass parent_class;
};

struct _GtkQSortFilterTreeModelPrivate
{
    GType *column_headers;
    gint  *column_roles;

    gint stamp;
    gint n_columns;

    QSortFilterProxyModel *given_model;
    QAbstractItemModel    *original_model;
    GtkAccessProxyModel   *access_model;
};

/* static prototypes */

/* GtkTreeModel prototypes */
static void              gtk_q_sort_filter_tree_model_tree_model_init (GtkTreeModelIface * );
static void              gtk_q_sort_filter_tree_model_finalize        (GObject *           );
static GtkTreeModelFlags gtk_q_sort_filter_tree_model_get_flags       (GtkTreeModel *      );
static gint              gtk_q_sort_filter_tree_model_get_n_columns   (GtkTreeModel *      );
static GType             gtk_q_sort_filter_tree_model_get_column_type (GtkTreeModel *,
                                                                       gint                );
static gboolean          gtk_q_sort_filter_tree_model_get_iter        (GtkTreeModel *,
                                                                       GtkTreeIter *,
                                                                       GtkTreePath *       );
static GtkTreePath *     gtk_q_sort_filter_tree_model_get_path        (GtkTreeModel *,
                                                                       GtkTreeIter *       );
static void              gtk_q_sort_filter_tree_model_get_value       (GtkTreeModel *,
                                                                       GtkTreeIter *,
                                                                       gint,
                                                                       GValue *            );
static gboolean          gtk_q_sort_filter_tree_model_iter_next       (GtkTreeModel *,
                                                                       GtkTreeIter *       );
static gboolean          gtk_q_sort_filter_tree_model_iter_previous   (GtkTreeModel *,
                                                                       GtkTreeIter *       );
static gboolean          gtk_q_sort_filter_tree_model_iter_children   (GtkTreeModel *,
                                                                       GtkTreeIter *,
                                                                       GtkTreeIter *       );
static gboolean          gtk_q_sort_filter_tree_model_iter_has_child  (GtkTreeModel *,
                                                                       GtkTreeIter *       );
static gint              gtk_q_sort_filter_tree_model_iter_n_children (GtkTreeModel *,
                                                                       GtkTreeIter *       );
static gboolean          gtk_q_sort_filter_tree_model_iter_nth_child  (GtkTreeModel *,
                                                                       GtkTreeIter *,
                                                                       GtkTreeIter *,
                                                                       gint                );
static gboolean          gtk_q_sort_filter_tree_model_iter_parent     (GtkTreeModel *,
                                                                       GtkTreeIter *,
                                                                       GtkTreeIter *       );

/* implementation prototypes */
static void qmodelindex_to_iter              (const QModelIndex &,
                                              GtkTreeIter *        );
// static void gtk_q_sort_filter_tree_model_increment_stamp (GtkQSortFilterTreeModel * );

static gint gtk_q_sort_filter_tree_model_length          (GtkQSortFilterTreeModel *  );

static void gtk_q_sort_filter_tree_model_set_n_columns   (GtkQSortFilterTreeModel *,
                                                          gint                       );
static void gtk_q_sort_filter_tree_model_set_column_type (GtkQSortFilterTreeModel *,
                                                          gint,
                                                          gint,
                                                          GType                      );

/* End private prototypes */

/* define type, inherit from GObject, define implemented interface(s) */
G_DEFINE_TYPE_WITH_CODE (GtkQSortFilterTreeModel, gtk_q_sort_filter_tree_model, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkQSortFilterTreeModel)
       G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
            gtk_q_sort_filter_tree_model_tree_model_init))

#define GTK_Q_SORT_FILTER_TREE_MODEL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_Q_SORT_FILTER_TREE_MODEL, GtkQSortFilterTreeModelPrivate))

#define Q_ITER(iter) ((QIter *)iter)

static void
gtk_q_sort_filter_tree_model_class_init(GtkQSortFilterTreeModelClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = gtk_q_sort_filter_tree_model_finalize;
}

static void
gtk_q_sort_filter_tree_model_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_flags = gtk_q_sort_filter_tree_model_get_flags;
    iface->get_n_columns = gtk_q_sort_filter_tree_model_get_n_columns;
    iface->get_column_type = gtk_q_sort_filter_tree_model_get_column_type;
    iface->get_iter = gtk_q_sort_filter_tree_model_get_iter;
    iface->get_path = gtk_q_sort_filter_tree_model_get_path;
    iface->get_value = gtk_q_sort_filter_tree_model_get_value;
    iface->iter_next = gtk_q_sort_filter_tree_model_iter_next;
    iface->iter_previous = gtk_q_sort_filter_tree_model_iter_previous;
    iface->iter_children = gtk_q_sort_filter_tree_model_iter_children;
    iface->iter_has_child = gtk_q_sort_filter_tree_model_iter_has_child;
    iface->iter_n_children = gtk_q_sort_filter_tree_model_iter_n_children;
    iface->iter_nth_child = gtk_q_sort_filter_tree_model_iter_nth_child;
    iface->iter_parent = gtk_q_sort_filter_tree_model_iter_parent;
}

static void
gtk_q_sort_filter_tree_model_init(GtkQSortFilterTreeModel *q_tree_model)
{
    GtkQSortFilterTreeModelPrivate *priv = GTK_Q_SORT_FILTER_TREE_MODEL_GET_PRIVATE(q_tree_model);
    q_tree_model->priv = priv;

    priv->stamp = g_random_int();
    priv->access_model = NULL;
    priv->original_model = NULL;
    priv->given_model = NULL;
}

/**
 * gtk_q_sort_filter_tree_model_get_qmodel
 * returns the original model from which this GtkQSortFilterTreeModel is created
 */
QSortFilterProxyModel *
gtk_q_sort_filter_tree_model_get_qmodel(GtkQSortFilterTreeModel *q_tree_model)
{
    GtkQSortFilterTreeModelPrivate *priv = GTK_Q_SORT_FILTER_TREE_MODEL_GET_PRIVATE(q_tree_model);
    return priv->given_model;
}

/**
 * gtk_q_sort_filter_tree_model_get_source_idx
 * Returns the index of the original model used to create this GtkQSortFilterTreeModel from
 * the given iter, if there is one.
 */
QModelIndex
gtk_q_sort_filter_tree_model_get_source_idx(GtkQSortFilterTreeModel *q_tree_model, GtkTreeIter *iter)
{
    GtkQSortFilterTreeModelPrivate *priv = GTK_Q_SORT_FILTER_TREE_MODEL_GET_PRIVATE(q_tree_model);
    /* get the call */
    QIter *qiter = Q_ITER(iter);
    QModelIndex retval = QModelIndex();
    QModelIndex access_idx = priv->access_model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
    if (access_idx.isValid()) {
        QModelIndex original_idx = priv->access_model->mapToSource(access_idx);
        if (original_idx.isValid())
            retval = priv->given_model->mapFromSource(original_idx);
    }
    if (!retval.isValid())
        g_warning("could not get valid QModelIndex from given GtkTreeIter");

    return retval;
}

/**
 * Takes a QModelIndex from the original QAbstractItemModel and returns a valid GtkTreeIter in the corresponding
 * GtkQSortFilterTreeModel
 */
gboolean
gtk_q_sort_filter_tree_model_source_index_to_iter(GtkQSortFilterTreeModel *q_tree_model, const QModelIndex &idx, GtkTreeIter *iter)
{
    GtkQSortFilterTreeModelPrivate *priv = GTK_Q_SORT_FILTER_TREE_MODEL_GET_PRIVATE(q_tree_model);

    /* the the proxy_idx from the source idx */
    QModelIndex original_idx = priv->given_model->mapToSource(idx);
    QModelIndex access_idx = priv->access_model->mapFromSource(original_idx);
    if (!access_idx.isValid())
        return FALSE;

    /* make sure iter is valid */
    iter->stamp = priv->stamp;

    /* map the proxy idx to iter */
    Q_ITER(iter)->row.value = access_idx.row();
    Q_ITER(iter)->column.value = access_idx.column();
    Q_ITER(iter)->id = access_idx.internalPointer();
    return TRUE;
}

/**
 * gtk_q_sort_filter_tree_model_new:
 * @model: QAbstractItemModel to which this model will bind.
 * @n_columns: number of columns in the list store
 * @...: all #GType follwed by the #Role pair for each column.
 *
 * Return value: a new #GtkQSortFilterTreeModel
 */
GtkQSortFilterTreeModel *
gtk_q_sort_filter_tree_model_new(QSortFilterProxyModel *model, size_t n_columns, ...)
{
    GtkQSortFilterTreeModel *retval;
    va_list args;
    gint i;

    g_return_val_if_fail(n_columns > 0, NULL);

    retval = (GtkQSortFilterTreeModel *)g_object_new(GTK_TYPE_Q_SORT_FILTER_TREE_MODEL, NULL);
    gtk_q_sort_filter_tree_model_set_n_columns(retval, n_columns);

    /* get original model */
    retval->priv->given_model = model;
    retval->priv->original_model = model->sourceModel();

    /* TODO: for now, assume one level of proxy model, later make it work for any
     * number of proxy models */
    // QAbstractItemModel *original = model;
    // QAbstractProxyModel *proxy = qobject_cast<QAbstractProxyModel*>(model);
    // while (proxy) {
    //     g_debug("got a proxy!");
    //     original = proxy->sourceModel();
    //     proxy = qobject_cast<QAbstractProxyModel*>(original);
    // }
    // retval->priv->original_model = original;
    // retval->priv->given_model = model;

    /* get access model from original model */
    retval->priv->access_model = new GtkAccessProxyModel();
    retval->priv->access_model->setSourceModel(retval->priv->original_model);
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

        gtk_q_sort_filter_tree_model_set_column_type (retval, i, role, type);
    }

    va_end (args);

    gtk_q_sort_filter_tree_model_length(retval);

    /* connect signals */
    QObject::connect(
        retval->priv->given_model,
        &QAbstractItemModel::rowsInserted,
        [=](const QModelIndex & parent, int first, int last) {
            for( int row = first; row <= last; ++row) {
                // g_debug("inserted row %d", row);
                GtkTreeIter *iter = g_new0(GtkTreeIter, 1);
                QModelIndex idx_given = retval->priv->given_model->index(row, 0, parent);
                QModelIndex idx_original = retval->priv->given_model->mapToSource(idx_given);
                QModelIndex idx_access = retval->priv->access_model->mapFromSource(idx_original);
                iter->stamp = stamp;
                qmodelindex_to_iter(idx_access, iter);
                GtkTreePath *path = gtk_q_sort_filter_tree_model_get_path(GTK_TREE_MODEL(retval), iter);
                gtk_tree_model_row_inserted(GTK_TREE_MODEL(retval), path, iter);
            }
        }
    );

    QObject::connect(
        retval->priv->given_model,
        &QAbstractItemModel::rowsAboutToBeMoved,
        [=](const QModelIndex & sourceParent, int sourceStart, int sourceEnd,
            G_GNUC_UNUSED const QModelIndex & destinationParent, G_GNUC_UNUSED int destinationRow) {
            /* first remove the row from old location
             * then insert them at the new location on the "rowsMoved signal */
            for( int row = sourceEnd; row >= sourceStart; --row) {
                // g_debug("row about to be moved: %d", row);
                QModelIndex idx_given = retval->priv->given_model->index(row, 0, sourceParent);
                QModelIndex idx_original = retval->priv->given_model->mapToSource(idx_given);
                QModelIndex idx_access = retval->priv->access_model->mapFromSource(idx_original);
                GtkTreeIter iter_old;
                qmodelindex_to_iter(idx_access, &iter_old);
                GtkTreePath *path_old = gtk_q_sort_filter_tree_model_get_path(GTK_TREE_MODEL(retval), &iter_old);
                gtk_tree_model_row_deleted(GTK_TREE_MODEL(retval), path_old);
            }
        }
    );

    QObject::connect(
        retval->priv->given_model,
        &QAbstractItemModel::rowsMoved,
        [=](G_GNUC_UNUSED const QModelIndex & sourceParent, int sourceStart, int sourceEnd,
            const QModelIndex & destinationParent, int destinationRow) {
            /* these rows should have been removed in the "rowsAboutToBeMoved" handler
             * now insert them in the new location */
            for( int row = sourceStart; row <= sourceEnd; ++row) {
                // g_debug("row moved %d", row);
                GtkTreeIter *iter_new = g_new0(GtkTreeIter, 1);
                QModelIndex idx_given = retval->priv->given_model->index(destinationRow, 0, destinationParent);
                QModelIndex idx_original = retval->priv->given_model->mapToSource(idx_given);
                QModelIndex idx_access = retval->priv->access_model->mapFromSource(idx_original);
                iter_new->stamp = stamp;
                qmodelindex_to_iter(idx_access, iter_new);
                GtkTreePath *path_new = gtk_q_sort_filter_tree_model_get_path(GTK_TREE_MODEL(retval), iter_new);
                gtk_tree_model_row_inserted(GTK_TREE_MODEL(retval), path_new, iter_new);
                destinationRow++;
            }
        }
    );

    QObject::connect(
        retval->priv->given_model,
        &QAbstractItemModel::rowsRemoved,
        [=](const QModelIndex & parent, int first, int last) {

            GtkTreePath *parent_path = NULL;
            if (parent.isValid()) {
                QModelIndex parent_original = retval->priv->given_model->mapToSource(parent);
                QModelIndex parent_access = retval->priv->access_model->mapFromSource(parent_original);
                GtkTreeIter iter_parent;
                iter_parent.stamp = stamp;
                qmodelindex_to_iter(parent_access, &iter_parent);
                parent_path = gtk_q_sort_filter_tree_model_get_path(GTK_TREE_MODEL(retval), &iter_parent);
            } else {
                parent_path = gtk_tree_path_new();
            }

            /* go last to first, since the rows are being deleted */
            for( int row = last; row >= first; --row) {
                // g_debug("deleting row: %d", row);
                GtkTreePath *path = gtk_tree_path_copy(parent_path);
                gtk_tree_path_append_index(path, row);
                gtk_tree_model_row_deleted(GTK_TREE_MODEL(retval), path);
            }

            gtk_tree_path_free(parent_path);
        }
    );

    QObject::connect(
        retval->priv->given_model,
        &QAbstractItemModel::dataChanged,
        [=](const QModelIndex & topLeft, const QModelIndex & bottomRight,
            G_GNUC_UNUSED const QVector<int> & roles = QVector<int> ()) {
            /* we have to assume only one column */
            int first = topLeft.row();
            // g_debug("data changed on row: %d", first);
            int last = bottomRight.row();
            if (topLeft.column() != bottomRight.column() ) {
                // g_warning("more than one column is not supported!");
            }
            /* the first idx IS topLeft, the reset are his siblings */
            GtkTreeIter *iter = g_new0(GtkTreeIter, 1);
            QModelIndex idx_given = topLeft;
            QModelIndex idx_original = retval->priv->given_model->mapToSource(idx_given);
            QModelIndex idx_access = retval->priv->access_model->mapFromSource(idx_original);
            iter->stamp = stamp;
            qmodelindex_to_iter(idx_access, iter);
            GtkTreePath *path = gtk_q_sort_filter_tree_model_get_path(GTK_TREE_MODEL(retval), iter);
            gtk_tree_model_row_changed(GTK_TREE_MODEL(retval), path, iter);
            for( int row = first + 1; row <= last; row++) {
                // g_debug("data changed on row: %d", row);
                iter = g_new0(GtkTreeIter, 1);
                idx_given = topLeft.sibling(row, 0);
                idx_original = retval->priv->given_model->mapToSource(idx_given);
                idx_access = retval->priv->access_model->mapFromSource(idx_original);
                iter->stamp = stamp;
                qmodelindex_to_iter(idx_access, iter);
                path = gtk_q_sort_filter_tree_model_get_path(GTK_TREE_MODEL(retval), iter);
                gtk_tree_model_row_changed(GTK_TREE_MODEL(retval), path, iter);
            }
        }
    );

    QObject::connect(
        retval->priv->given_model,
        &QAbstractItemModel::modelAboutToBeReset,
        [=] () {
            // g_debug("model about to be reset");
            /* nothing equvivalent eixists in GtkTreeModel, so simply delete all
             * rows, and add all rows when the model is reset;
             * we must delete the rows in ascending order */
            int row_count = retval->priv->given_model->rowCount();
            for (int row = row_count; row > 0; --row) {
                // g_debug("deleting row %d", row -1);
                QModelIndex idx_given = retval->priv->given_model->index(row - 1, 0);
                QModelIndex idx_original = retval->priv->given_model->mapToSource(idx_given);
                QModelIndex idx_access = retval->priv->access_model->mapFromSource(idx_original);
                GtkTreeIter iter;
                iter.stamp = stamp;
                qmodelindex_to_iter(idx_access, &iter);
                GtkTreePath *path = gtk_q_sort_filter_tree_model_get_path(GTK_TREE_MODEL(retval), &iter);
                gtk_tree_model_row_deleted(GTK_TREE_MODEL(retval), path);
            }
        }
    );

    QObject::connect(
        retval->priv->given_model,
        &QAbstractItemModel::modelReset,
        [=] () {
            // g_debug("model reset");
            /* now add all the (new) rows */
            int row_count = retval->priv->given_model->rowCount();
            for (int row = 0; row < row_count; ++row) {
                // g_debug("adding row %d", row);
                GtkTreeIter *iter_new = g_new0(GtkTreeIter, 1);
                QModelIndex idx_given = retval->priv->given_model->index(row, 0);
                QModelIndex idx_original = retval->priv->given_model->mapToSource(idx_given);
                QModelIndex idx_access = retval->priv->access_model->mapFromSource(idx_original);
                iter_new->stamp = stamp;
                qmodelindex_to_iter(idx_access, iter_new);
                GtkTreePath *path_new = gtk_q_sort_filter_tree_model_get_path(GTK_TREE_MODEL(retval), iter_new);
                gtk_tree_model_row_inserted(GTK_TREE_MODEL(retval), path_new, iter_new);
            }
        }
    );

    return retval;
}

static gint
gtk_q_sort_filter_tree_model_length(GtkQSortFilterTreeModel *q_tree_model)
{
    GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;
    // g_debug("row cout: %d", priv->access_model->rowCount());
    return priv->access_model->rowCount();
}

static void
gtk_q_sort_filter_tree_model_set_n_columns(GtkQSortFilterTreeModel *q_tree_model,
                               gint          n_columns)
{
    GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;
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
gtk_q_sort_filter_tree_model_set_column_type(GtkQSortFilterTreeModel *q_tree_model,
                                 gint          column,
                                 gint          role,
                                 GType         type)
{
  GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;

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
gtk_q_sort_filter_tree_model_finalize(GObject *object)
{
    GtkQSortFilterTreeModel *q_tree_model = GTK_Q_SORT_FILTER_TREE_MODEL (object);
    GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;

    g_free(priv->column_headers);
    g_free(priv->column_roles);

    /* delete the created proxy model */
    delete priv->access_model;

    G_OBJECT_CLASS(gtk_q_sort_filter_tree_model_parent_class)->finalize (object);
}

static gboolean
iter_is_valid(GtkTreeIter *iter,
              GtkQSortFilterTreeModel   *q_tree_model)
{
    // g_debug("\titer is valid");
    g_return_val_if_fail(iter != NULL, FALSE);
    QIter *qiter = Q_ITER(iter);
    // g_debug("\t\tr: %d, p: %p", qiter->row.value, qiter->id);
    QModelIndex idx_access = q_tree_model->priv->access_model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
    // if (!idx_access.isValid())
    //     g_debug("access idx not valid");
    QModelIndex idx_original = q_tree_model->priv->access_model->mapToSource(idx_access);
    // if (!idx_original.isValid())
    //     g_debug("original idx not valid");
    QModelIndex idx_given = q_tree_model->priv->given_model->mapFromSource(idx_original);
    // if (!idx_given.isValid())
    //     g_debug("given idx not valid");
    return idx_given.isValid();
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
gtk_q_sort_filter_tree_model_get_flags(G_GNUC_UNUSED GtkTreeModel *tree_model)
{
    // TODO: possibly return based on the model?
    return (GtkTreeModelFlags)(GTK_TREE_MODEL_ITERS_PERSIST);
}

/* number of columns supported by this tree model */
static gint
gtk_q_sort_filter_tree_model_get_n_columns(GtkTreeModel *tree_model)
{
    GtkQSortFilterTreeModel *q_tree_model = GTK_Q_SORT_FILTER_TREE_MODEL(tree_model);
    GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;

    return priv->given_model->columnCount();
}

/* get given column type */
static GType
gtk_q_sort_filter_tree_model_get_column_type(GtkTreeModel *tree_model,
                                 gint          index)
{
    GtkQSortFilterTreeModel *q_tree_model = GTK_Q_SORT_FILTER_TREE_MODEL (tree_model);
    GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;

    g_return_val_if_fail (index < gtk_q_sort_filter_tree_model_get_n_columns(tree_model), G_TYPE_INVALID);

    return priv->column_headers[index];
}

/* Sets @iter to a valid iterator pointing to @path.  If @path does
 * not exist, @iter is set to an invalid iterator and %FALSE is returned.
 */
static gboolean
gtk_q_sort_filter_tree_model_get_iter(GtkTreeModel *tree_model,
                          GtkTreeIter  *iter,
                          GtkTreePath  *path)
{
    // g_debug("get iter from path");
    GtkQSortFilterTreeModel *q_tree_model = GTK_Q_SORT_FILTER_TREE_MODEL (tree_model);
    GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;

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
    QModelIndex idx_given = priv->given_model->index(indices[0], 0);
    for(int layer = 1; layer < depth; layer++ ) {
        /* check if previous iter is valid */
        if (!idx_given.isValid()) {
            break;
        } else {
            idx_given = idx_given.child(indices[layer], 0);
        }
    }

    if (!idx_given.isValid()) {
        iter->stamp = 0;
        return FALSE;
    } else {
        /* we have a valid QModelIndex; turn it into an iter */
        iter->stamp = priv->stamp;
        QModelIndex idx_original = priv->given_model->mapToSource(idx_given);
        // if (!idx_original.isValid())
        //     g_debug("\toriginal idx not valid");
        QModelIndex idx_access = priv->access_model->mapFromSource(idx_original);
        // if (!idx_access.isValid())
        //     g_debug("\taccess idx not valid");
        qmodelindex_to_iter(idx_access, iter);
    }

    return TRUE;
}

/* Returns a newly-created #GtkTreePath referenced by @iter.
 *
 * This path should be freed with gtk_tree_path_free().
 */
static GtkTreePath *
gtk_q_sort_filter_tree_model_get_path(GtkTreeModel *tree_model,
                          GtkTreeIter  *iter)
{
    // g_debug("get path");
    GtkQSortFilterTreeModel *q_tree_model = GTK_Q_SORT_FILTER_TREE_MODEL (tree_model);
    GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;
    GtkTreePath *path;

    g_return_val_if_fail (iter->stamp == priv->stamp, NULL);
    g_return_val_if_fail (iter_is_valid(iter, q_tree_model), NULL);

    /* To get the path, we have to traverse from the child all the way up */
    path = gtk_tree_path_new();
    QIter *qiter = Q_ITER(iter);
    QModelIndex idx_access = priv->access_model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
    QModelIndex idx_original = priv->access_model->mapToSource(idx_access);
    QModelIndex idx_given = priv->given_model->mapFromSource(idx_original);
    while( idx_given.isValid() ){
        gtk_tree_path_prepend_index(path, idx_given.row());
        idx_given = idx_given.parent();
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
gtk_q_sort_filter_tree_model_get_value(GtkTreeModel *tree_model,
                           GtkTreeIter  *iter,
                           gint          column,
                           GValue       *value)
{
    GtkQSortFilterTreeModel *q_tree_model = GTK_Q_SORT_FILTER_TREE_MODEL (tree_model);
    GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;

    g_return_if_fail (column < priv->n_columns);
    g_return_if_fail (iter_is_valid(iter, q_tree_model));

    /* get the data */
    QIter *qiter = Q_ITER(iter);
    QModelIndex idx_access = priv->access_model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
    QModelIndex idx_original = priv->access_model->mapToSource(idx_access);
    QModelIndex idx_given = priv->given_model->mapFromSource(idx_original);
    int role = priv->column_roles[column];
    QVariant var = idx_given.data(role);
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
            QByteArray ba = var.toString().toLocal8Bit();
            g_value_set_string(value, (gchar *)ba.constData());
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
gtk_q_sort_filter_tree_model_iter_next(GtkTreeModel  *tree_model,
                           GtkTreeIter   *iter)
{
    // g_debug("iter next");
    GtkQSortFilterTreeModel *q_tree_model = GTK_Q_SORT_FILTER_TREE_MODEL (tree_model);
    GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;

    g_return_val_if_fail (iter_is_valid(iter, q_tree_model), FALSE);

    QIter *qiter = Q_ITER(iter);
    QModelIndex idx_access = priv->access_model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
    // if (!idx_access.isValid())
    //     g_debug("\taccess not valid");
    QModelIndex idx_original = priv->access_model->mapToSource(idx_access);
    // if (!idx_original.isValid())
    //     g_debug("\toriginal not valid");
    QModelIndex idx_given = priv->given_model->mapFromSource(idx_original);
    // if (!idx_given.isValid())
    //     g_debug("\tgiven not valid");
    idx_given = idx_given.sibling(idx_given.row()+1, idx_given.column());
    // if (!idx_given.isValid())
    //     g_debug("\tgiven sibling not valid");

    /* validate */
    idx_original = priv->given_model->mapToSource(idx_given);
    // if (!idx_original.isValid())
    //     g_debug("\toriginal sibling not valid");
    idx_access = priv->access_model->mapFromSource(idx_original);
    // if (!idx_access.isValid())
    //     g_debug("\tacess sibling not valid");
    if (validate_index(priv->stamp, idx_access, iter) ) {
        // QIter *qiter = Q_ITER(iter);
        // g_debug("\tr: %d, p: %p", qiter->row.value, qiter->id);
        // g_debug("\ttrue");
        return TRUE;
    } else {
        // g_debug("\tfalse");
        return FALSE;
    }
}

/* Sets @iter to point to the previous node at the current level.
 *
 * If there is no previous @iter, %FALSE is returned and @iter is
 * set to be invalid.
 */
static gboolean
gtk_q_sort_filter_tree_model_iter_previous(GtkTreeModel *tree_model,
                               GtkTreeIter  *iter)
{
    // g_debug("iter prev");
    GtkQSortFilterTreeModel *q_tree_model = GTK_Q_SORT_FILTER_TREE_MODEL (tree_model);
    GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;

    g_return_val_if_fail (iter_is_valid(iter, q_tree_model), FALSE);

    QIter *qiter = Q_ITER(iter);
    QModelIndex idx_access = priv->access_model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
    QModelIndex idx_original = priv->access_model->mapToSource(idx_access);
    QModelIndex idx_given = priv->given_model->mapFromSource(idx_original);
    idx_given = idx_given.sibling(idx_given.row()-1, idx_given.column());

    /* validate */
    // return validate_index(priv->stamp, idx, iter);
    idx_original = priv->given_model->mapToSource(idx_given);
    idx_access = priv->access_model->mapFromSource(idx_original);
    if (validate_index(priv->stamp, idx_access, iter)) {
        // g_debug("\ttrue");
        return TRUE;
    } else {
        // g_debug("\tfalse");
        return FALSE;
    }
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
gtk_q_sort_filter_tree_model_iter_children(GtkTreeModel *tree_model,
                               GtkTreeIter  *iter,
                               GtkTreeIter  *parent)
{
    // g_debug("iter return first child");
    GtkQSortFilterTreeModel *q_tree_model = GTK_Q_SORT_FILTER_TREE_MODEL (tree_model);
    GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;
    QModelIndex idx_access;
    QModelIndex idx_original;
    QModelIndex idx_given;

    /* make sure parent if valid node if its not NULL */
    if (parent)
        g_return_val_if_fail(iter_is_valid(parent, q_tree_model), FALSE);

    if (parent) {
        /* get first child */
        QIter *qparent = Q_ITER(parent);
        idx_access = priv->access_model->indexFromId(qparent->row.value, qparent->column.value, qparent->id);
        idx_original = priv->access_model->mapToSource(idx_access);
        idx_given = priv->given_model->mapFromSource(idx_original);
        idx_given = idx_given.child(0, 0);
    } else {
        /* parent is NULL, get the first node */
        idx_given = priv->given_model->index(0, 0);
        // g_debug("\treturning the first node");
    }

    /* validate child */
    // return validate_index(priv->stamp, idx, iter);
    idx_original = priv->given_model->mapToSource(idx_given);
    idx_access = priv->access_model->mapFromSource(idx_original);
    if (validate_index(priv->stamp, idx_access, iter)) {
        // g_debug("\ttrue");
        return TRUE;
    } else {
        // g_debug("\tfalse");
        return FALSE;
    }
}

/* Returns %TRUE if @iter has children, %FALSE otherwise. */
static gboolean
gtk_q_sort_filter_tree_model_iter_has_child (GtkTreeModel *tree_model,
                   GtkTreeIter  *iter)
{
    // g_debug("iter has child");
    GtkQSortFilterTreeModel *q_tree_model = GTK_Q_SORT_FILTER_TREE_MODEL (tree_model);
    GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;
    g_return_val_if_fail(iter_is_valid(iter, q_tree_model), FALSE);

    QIter *qiter = Q_ITER(iter);
    QModelIndex idx_access = priv->access_model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
    QModelIndex idx_original = priv->access_model->mapToSource(idx_access);
    QModelIndex idx_given = priv->given_model->mapFromSource(idx_original);
    // return priv->access_model->hasChildren(idx);
    if (priv->given_model->hasChildren(idx_given)) {
        // g_debug("\ttrue");
        return TRUE;
    } else {
        // g_debug("\tfalse");
        return FALSE;
    }
}

/* Returns the number of children that @iter has.
 *
 * As a special case, if @iter is %NULL, then the number
 * of toplevel nodes is returned.
 */
static gint
gtk_q_sort_filter_tree_model_iter_n_children (GtkTreeModel *tree_model,
                GtkTreeIter  *iter)
{
    // g_debug("get num children");
    GtkQSortFilterTreeModel *q_tree_model = GTK_Q_SORT_FILTER_TREE_MODEL (tree_model);
    GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;

    if (iter == NULL) {
        // g_debug("\ttop level");
        return gtk_q_sort_filter_tree_model_length(q_tree_model);
    }

    g_return_val_if_fail(iter_is_valid(iter, q_tree_model), -1);
    QIter *qiter = Q_ITER(iter);
    QModelIndex idx_access = priv->access_model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
    QModelIndex idx_original = priv->access_model->mapToSource(idx_access);
    QModelIndex idx_given = priv->given_model->mapFromSource(idx_original);
    // g_debug("\t%d", priv->given_model->rowCount(idx_given));
    return priv->given_model->rowCount(idx_given);
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
gtk_q_sort_filter_tree_model_iter_nth_child(GtkTreeModel *tree_model,
                                GtkTreeIter  *iter,
                                GtkTreeIter  *parent,
                                gint          n)
{
    // g_debug("get nth child");
    GtkQSortFilterTreeModel *q_tree_model = GTK_Q_SORT_FILTER_TREE_MODEL (tree_model);
    GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;
    QModelIndex idx_access;
    QModelIndex idx_original;
    QModelIndex idx_given;

    if (parent) {
        g_return_val_if_fail(iter_is_valid(parent, q_tree_model), FALSE);

        /* get the nth child */
        QIter *qparent = Q_ITER(parent);
        idx_access = priv->access_model->indexFromId(qparent->row.value, qparent->column.value, qparent->id);
        idx_original = priv->access_model->mapToSource(idx_access);
        idx_given = priv->given_model->mapFromSource(idx_original);
        idx_given = idx_given.child(n, 0);
    } else {
        idx_given = priv->given_model->index(n, 0);
        // g_debug("\tof root node");
    }

    /* validate */
    // return validate_index(priv->stamp, idx, iter);
    idx_original = priv->given_model->mapToSource(idx_given);
    idx_access = priv->access_model->mapFromSource(idx_original);
    if (validate_index(priv->stamp, idx_access, iter)) {
        // g_debug("\ttrue");
        return TRUE;
    } else {
        // g_debug("\tfalse");
        return FALSE;
    }
}

/* Sets @iter to be the parent of @child.
 *
 * If @child is at the toplevel, and doesn't have a parent, then
 * @iter is set to an invalid iterator and %FALSE is returned.
 * @child will remain a valid node after this function has been
 * called.
 */
static gboolean
gtk_q_sort_filter_tree_model_iter_parent(GtkTreeModel *tree_model,
                             GtkTreeIter  *iter,
                             GtkTreeIter  *child)
{
    // g_debug("get parent");
    GtkQSortFilterTreeModel *q_tree_model = GTK_Q_SORT_FILTER_TREE_MODEL (tree_model);
    GtkQSortFilterTreeModelPrivate *priv = q_tree_model->priv;
    QModelIndex idx_access;
    QModelIndex idx_original;
    QModelIndex idx_given;

    g_return_val_if_fail(iter_is_valid(child, q_tree_model), FALSE);

    QIter *qchild = Q_ITER(child);
    idx_access = priv->access_model->indexFromId(qchild->row.value, qchild->column.value, qchild->id);
    idx_original = priv->access_model->mapToSource(idx_access);
    idx_given = priv->given_model->mapFromSource(idx_original);
    idx_given = idx_given.parent();

    /* validate */
    // return validate_index(priv->stamp, idx, iter);
    idx_original = priv->given_model->mapToSource(idx_given);
    idx_access = priv->access_model->mapFromSource(idx_original);
    if (validate_index(priv->stamp, idx_access, iter)) {
        // g_debug("\ttrue");
        return TRUE;
    } else {
        // g_debug("\tfalse");
        return FALSE;
    }
}

/* End Fulfill the GtkTreeModel requirements */
