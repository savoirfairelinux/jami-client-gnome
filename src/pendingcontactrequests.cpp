/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
 *  Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>
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

// Client
#include "pendingcontactrequests.h"
#include "models/gtkqtreemodel.h"
#include "native/pixbufmanipulator.h"

// LRC
#include <recentmodel.h>
#include <accountmodel.h>
#include <pendingcontactrequestmodel.h>
#include <account.h>
#include <availableaccountmodel.h>
#include <globalinstances.h>
#include <contactrequest.h>

// Gtk
#include <gtk/gtk.h>
#include <glib/gi18n.h>

// Qt
#include <QtCore/QDateTime>
#include <QtCore/QSize>
#include <QtCore/QLocale>

/**
 * gtk structure
 */
struct _PendingContactRequestsView
{
    GtkTreeView parent;
};

/**
 * gtk class structure
 */
struct _PendingContactRequestsViewClass
{
    GtkTreeViewClass parent_class;
};

typedef struct _PendingContactRequestsViewPrivate PendingContactRequestsViewPrivate;

/**
 * gtk private structure
 */
struct _PendingContactRequestsViewPrivate
{
    GtkWidget *treeview_pending_contact_request_list;
    QSortFilterProxyModel* myProxy;
};

G_DEFINE_TYPE_WITH_PRIVATE(PendingContactRequestsView, pending_contact_requests_view, GTK_TYPE_TREE_VIEW);

#define PENDING_CONTACT_REQUESTS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), PENDING_CONTACT_REQUESTS_VIEW_TYPE, PendingContactRequestsViewPrivate))

/**
 * callback function for rendering the contact photo
 */
static void
render_contact_photo(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);

    std::shared_ptr<GdkPixbuf> image;
    QVariant object = idx.data(static_cast<int>(Ring::Role::Object));

    if (idx.isValid() && object.isValid()) {
        QVariant var_photo;
        if (auto cr = object.value<ContactRequest *>()) {
            if (cr->peer())
                var_photo = GlobalInstances::pixmapManipulator().contactPhoto(cr->peer(), QSize(50, 50), false);
        }
        if (var_photo.isValid())
            image = var_photo.value<std::shared_ptr<GdkPixbuf>>();

        g_object_set(G_OBJECT(cell), "height", 50, NULL);
        g_object_set(G_OBJECT(cell), "width", 50, NULL);
        g_object_set(G_OBJECT(cell), "pixbuf", image.get(), NULL);
    }
}

/**
 * callback function for rendering the best id of the peer and the date.
 */
static void
render_name_and_info(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     GtkTreeView *treeview)
{
    auto idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);

    if (not idx.isValid()) {
        g_warning("could not get index for contact request");
        return;
    }

    // check if this iter is selected
    gboolean is_selected = FALSE;
    if (GTK_IS_TREE_VIEW(treeview)) {
        auto selection = gtk_tree_view_get_selection(treeview);
        is_selected = gtk_tree_selection_iter_is_selected(selection, iter);
    }

    /* best id */
    auto uri_qstring = idx.data(static_cast<int>(Qt::DisplayRole)).value<QString>();
    auto uri_std = uri_qstring.toStdString();

    /* profile name */
    auto qt_model = idx.model();
    auto idx_formatted_name = qt_model->index(idx.row(), static_cast<int>(PendingContactRequestModel::Columns::FORMATTED_NAME));
    auto formatted_name_qstring = idx_formatted_name.data(static_cast<int>(Qt::DisplayRole)).value<QString>();
    auto formatted_name_std = formatted_name_qstring.toStdString();

    /* date */
    auto idx_date = qt_model->index(idx.row(), static_cast<int>(PendingContactRequestModel::Columns::TIME));
    auto date_q_date_time = idx_date.data(static_cast<int>(Qt::DisplayRole)).value<QDateTime>();
    auto date_q_string = QLocale::system().toString(date_q_date_time.time(), QLocale::ShortFormat);
    auto date_std = date_q_string.toStdString();

    gchar *text = nullptr;

    if(is_selected) // print in default color
        text = g_markup_printf_escaped("<span font_weight=\"bold\">%s</span>\n%s\n"
                                        "<span size=\"smaller\">%s</span>", formatted_name_std.c_str(),
                                                                            uri_std.c_str(),
                                                                            date_std.c_str());
    else // use our colors
        text = g_markup_printf_escaped("<span font_weight=\"bold\">%s</span>\n<span color=\"gray\">%s\n"
                                        "<span size=\"smaller\">%s</span></span>", formatted_name_std.c_str(),
                                                                                   uri_std.c_str(),
                                                                                   date_std.c_str());

    g_object_set(G_OBJECT(cell), "markup", text, nullptr);
    g_free(text);
}

/**
 * gtk init function
 */
static void
pending_contact_requests_view_init(PendingContactRequestsView *self)
{
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(self), FALSE);
    gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(self), FALSE);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(self), FALSE);

    // the next signal is used to set the model in function of the selection of the account
    QObject::connect(AvailableAccountModel::instance().selectionModel(), &QItemSelectionModel::currentChanged, [self](const QModelIndex& idx){
        if (auto account = idx.data(static_cast<int>(Account::Role::Object)).value<Account*>()) {
            GtkQTreeModel *pending_contact_requests_model;
            pending_contact_requests_model = gtk_q_tree_model_new(
                account->pendingContactRequestModel(),
                1/*nmbr. of cols.*/,
                0, Qt::DisplayRole, G_TYPE_STRING);

            gtk_tree_view_set_model(GTK_TREE_VIEW(self), GTK_TREE_MODEL(pending_contact_requests_model));

        }
    });

    /* photo and name/contact method column */
    GtkCellArea *area = gtk_cell_area_box_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_area(area);

    /* photo renderer */
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    /* get the photo */
    gtk_tree_view_column_set_cell_data_func(
      column,
      renderer,
      (GtkTreeCellDataFunc)render_contact_photo,
      NULL,
      NULL);

    /* name and info renderer */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    gtk_tree_view_column_set_cell_data_func(
      column,
      renderer,
      (GtkTreeCellDataFunc)render_name_and_info,
      self,
      NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(self), column);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_expand_all(GTK_TREE_VIEW(self));

    gtk_widget_show_all(GTK_WIDGET(self));
}

/**
 * gtk dispose function
 */
static void
pending_contact_requests_view_dispose(GObject *object)
{
    G_OBJECT_CLASS(pending_contact_requests_view_parent_class)->dispose(object);
}

/**
 * gtk finalize function
 */
static void
pending_contact_requests_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(pending_contact_requests_view_parent_class)->finalize(object);
}

/**
 * gtk class init function
 */
static void
pending_contact_requests_view_class_init(PendingContactRequestsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = pending_contact_requests_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = pending_contact_requests_view_dispose;
}

/**
 * gtk new function
 */
GtkWidget *
pending_contact_requests_view_new()
{
    gpointer self = g_object_new(PENDING_CONTACT_REQUESTS_VIEW_TYPE, NULL);

    return (GtkWidget *)self;
}
