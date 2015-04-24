/*
 *  Copyright (C) 2017 Savoir-Faire Linux Inc.
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

#include "phonedirectoryview.h"

#include <gtk/gtk.h>
#include "../models/gtkqtreemodel.h"
#include <phonedirectorymodel.h>

struct _PhonedirectoryView
{
    GtkScrolledWindow parent;
};

struct _PhonedirectoryViewClass
{
    GtkScrolledWindowClass parent_class;
};

typedef struct _PhonedirectoryViewPrivate PhonedirectoryViewPrivate;

struct _PhonedirectoryViewPrivate
{
};

G_DEFINE_TYPE_WITH_PRIVATE(PhonedirectoryView, phonedirectory_view, GTK_TYPE_SCROLLED_WINDOW);

#define PHONEDIRECTORY_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), PHONEDIRECTORY_VIEW_TYPE, PhonedirectoryViewPrivate))

static void
render_column_value(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                    GtkCellRenderer *cell,
                    GtkTreeModel *tree_model,
                    GtkTreeIter *iter,
                    gpointer data)
{
    int column_idx = *(int*)(data);

    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(tree_model), iter);
    gchar *text = NULL;

    if (idx.isValid()) {
        QVariant var = idx.sibling(idx.row(), column_idx).data(Qt::DisplayRole);
        text = g_strdup_printf("%s", var.value<QString>().toUtf8().constData());
    }

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(text);
}

static void
add_text_column(const gchar* name, int column_idx, const GtkTreeView *treeview)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
//     g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(name, renderer, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    gtk_tree_view_column_set_resizable(column, TRUE);
    int *column_ptr = (int *)g_malloc(sizeof(int));
    *column_ptr = column_idx;
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_column_value,
        column_ptr,
        (GDestroyNotify)g_free);
}

static void
phonedirectory_view_init(PhonedirectoryView *self)
{
    gtk_widget_set_size_request(GTK_WIDGET(self), 1024, 512);

    /* contacts view/model */
    GtkWidget *treeview = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);
    gtk_container_add(GTK_CONTAINER(self), treeview);

    GtkQTreeModel *model = gtk_q_tree_model_new(
        (QAbstractItemModel *)&PhoneDirectoryModel::instance(),
        1,
        0, Qt::DisplayRole, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(model));

    add_text_column("URI", 0, GTK_TREE_VIEW(treeview));
    add_text_column("TYPE", 1, GTK_TREE_VIEW(treeview));
    add_text_column("CONTACT", 2, GTK_TREE_VIEW(treeview));
    add_text_column("ACCOUNT", 3, GTK_TREE_VIEW(treeview));
    add_text_column("STATE", 4, GTK_TREE_VIEW(treeview));
    add_text_column("CALL_COUNT", 5, GTK_TREE_VIEW(treeview));
    add_text_column("WEEK_COUNT", 6, GTK_TREE_VIEW(treeview));
    add_text_column("TRIM_COUNT", 7, GTK_TREE_VIEW(treeview));
    add_text_column("HAVE_CALLED", 8, GTK_TREE_VIEW(treeview));
    add_text_column("LAST_USED", 9, GTK_TREE_VIEW(treeview));
    add_text_column("NAME_COUNT", 10, GTK_TREE_VIEW(treeview));
    add_text_column("TOTAL_SECONDS", 11, GTK_TREE_VIEW(treeview));
    add_text_column("POPULARITY_INDEX", 12, GTK_TREE_VIEW(treeview));
    add_text_column("BOOKMARED", 13, GTK_TREE_VIEW(treeview));
    add_text_column("TRACKED", 14, GTK_TREE_VIEW(treeview));
    add_text_column("PRESENT", 15, GTK_TREE_VIEW(treeview));
    add_text_column("PRESENCE_MESSAGE", 16, GTK_TREE_VIEW(treeview));
    add_text_column("UID", 17, GTK_TREE_VIEW(treeview));

    gtk_widget_show_all(GTK_WIDGET(self));
}

static void
phonedirectory_view_dispose(GObject *object)
{
    G_OBJECT_CLASS(phonedirectory_view_parent_class)->dispose(object);
}

static void
phonedirectory_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(phonedirectory_view_parent_class)->finalize(object);
}

static void
phonedirectory_view_class_init(PhonedirectoryViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = phonedirectory_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = phonedirectory_view_dispose;
}

GtkWidget *
phonedirectory_view_new()
{
    gpointer self = g_object_new(PHONEDIRECTORY_VIEW_TYPE, NULL);

    return (GtkWidget *)self;
}
