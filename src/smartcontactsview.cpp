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

#include "smartcontactsview.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "models/gtkqtreemodel.h"
#include "utils/calling.h"
#include <memory>
#include <globalinstances.h>
#include "native/pixbufmanipulator.h"
#include <contactmethod.h>
#include "defines.h"
#include "utils/models.h"
#include <recentmodel.h>
#include <call.h>
#include "utils/menus.h"
#include <itemdataroles.h>
#include <callmodel.h>
#include <QtCore/QItemSelectionModel>
#include <historytimecategorymodel.h>
#include <QtCore/QDateTime>
#include <QtCore/QMimeData>
#include "utils/drawing.h"
#include <numbercategory.h>
#include "contactpopupmenu.h"

// Lrc WIP model
#include <contactitem.h>
#include <smartlistmodel.h>
#include <smartlistitem.h>
#include <database.h>

static constexpr const char* CALL_TARGET    = "CALL_TARGET";
static constexpr int         CALL_TARGET_ID = 0;

struct _SmartContactsView
{
    GtkTreeView parent;
};

struct _SmartContactsViewClass
{
    GtkTreeViewClass parent_class;
};

typedef struct _SmartContactsViewPrivate SmartContactsViewPrivate;

struct _SmartContactsViewPrivate
{
    GtkWidget *popup_menu;

    QMetaObject::Connection selection_updated;
    QMetaObject::Connection layout_changed;
    QMetaObject::Connection smartListModel_updated;

    QMetaObject::Connection new_message_connection2; // test only


};

G_DEFINE_TYPE_WITH_PRIVATE(SmartContactsView, smart_contacts_view, GTK_TYPE_TREE_VIEW);

#define SMART_CONTACTS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SMART_CONTACTS_VIEW_TYPE, SmartContactsViewPrivate))

static void
render_name_and_number(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                       GtkCellRenderer *cell,
                       GtkTreeModel *model,
                       GtkTreeIter *iter,
                       GtkTreeView *treeview)
{
    gchar *ringId;

    gtk_tree_model_get (model, iter,
                        0 /* col# */, &ringId /* data */,
                        -1);


    g_object_set(G_OBJECT(cell), "markup", ringId, NULL);
    g_free(ringId);

    return;
}


static GtkTreeModel*
create_and_fill_model()
{
    GtkListStore* store = gtk_list_store_new (1 /* # of cols */ , G_TYPE_STRING, G_TYPE_UINT);
    GtkTreeIter iter;

    // append les rows
    for (auto row : SmartListModel::instance().getItems()) {
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
            0 /* col # */ , row->getTitle().c_str() /* celldata */,
            -1 /* end */);
    }

    return GTK_TREE_MODEL (store);
}


static void
activate_smart_list_item(GtkTreeSelection *selection)
{
    GtkTreeIter iter;
    GtkTreeModel *model = nullptr;
    gchar *data = nullptr;

    gtk_tree_selection_get_selected(selection, &model, &iter);

    gtk_tree_model_get(model, &iter,
                       0, &data,
                       -1);


    // get path
    auto path = gtk_tree_model_get_path(model, &iter);

    auto idx = gtk_tree_path_get_indices(path);
    //~ gtk_tree_model_iter_

    auto smart_list_item = SmartListModel::instance().getItem(idx[0]);

    smart_list_item->action();

}

static void
smart_contacts_view_init(SmartContactsView *self)
{
    SmartContactsViewPrivate *privWIP = SMART_CONTACTS_VIEW_GET_PRIVATE(self);

    auto modelWIP = create_and_fill_model();
    gtk_tree_view_set_model(GTK_TREE_VIEW(self),
                            GTK_TREE_MODEL(modelWIP));

    /* ringId method column */
    GtkCellArea *areaWIP = gtk_cell_area_box_new();
    GtkTreeViewColumn *columnWIP = gtk_tree_view_column_new_with_area(areaWIP);

    /* renderer */
    GtkCellRenderer *rendererWIP = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(rendererWIP), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(areaWIP), rendererWIP, FALSE, FALSE, FALSE);

    gtk_tree_view_column_set_cell_data_func(
        columnWIP,
        rendererWIP,
        (GtkTreeCellDataFunc)render_name_and_number,
        self,
        NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(self), columnWIP);

    /* update the model based on SmartListModel */
    privWIP->smartListModel_updated = QObject::connect(&SmartListModel::instance(), &SmartListModel::modelUpdated,
    [self] () {
    // il faut s'assurer que l'on est pas entrain de creer des obj dynmc sans les detruires
    auto modelWIP = create_and_fill_model();

    gtk_tree_view_set_model(GTK_TREE_VIEW(self),
                            GTK_TREE_MODEL(modelWIP));
    });

    gtk_widget_show_all(GTK_WIDGET(self));

    GtkTreeSelection *selectionNew = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));
    g_signal_connect(selectionNew, "changed", G_CALLBACK(activate_smart_list_item), NULL);

    // le signal ne devrait pas etre ici, mais dans chatview test only...
    SmartContactsViewPrivate *priv = SMART_CONTACTS_VIEW_GET_PRIVATE(self);
    priv->new_message_connection2 = QObject::connect(&DataBase::instance(), &DataBase::messageAdded,
    [] (DataBase::Message msg) {
        qDebug() << QString(msg.body.c_str());
    });

    return;
}

static void
smart_contacts_view_dispose(GObject *object)
{
    SmartContactsView *self = SMART_CONTACTS_VIEW(object);
    SmartContactsViewPrivate *priv = SMART_CONTACTS_VIEW_GET_PRIVATE(self);

    QObject::disconnect(priv->selection_updated);
    QObject::disconnect(priv->layout_changed);
    gtk_widget_destroy(priv->popup_menu);

    G_OBJECT_CLASS(smart_contacts_view_parent_class)->dispose(object);
}

static void
smart_contacts_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(smart_contacts_view_parent_class)->finalize(object);
}

static void
smart_contacts_view_class_init(SmartContactsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = smart_contacts_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = smart_contacts_view_dispose;
}

GtkWidget *
smart_contacts_view_new()
{
    gpointer self = g_object_new(SMART_CONTACTS_VIEW_TYPE, NULL);

    return (GtkWidget *)self;
}
