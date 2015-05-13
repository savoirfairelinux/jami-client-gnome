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

#include "frequentcontactsview.h"

#include <gtk/gtk.h>
#include "models/gtkqtreemodel.h"
#include "utils/calling.h"
#include <memory>
#include "delegates/pixbufdelegate.h"
#include <contactmethod.h>
#include "defines.h"
#include "utils/models.h"
#include <categorizedbookmarkmodel.h>
#include <call.h>

#define COPY_DATA_KEY "copy_data"

struct _FrequentContactsView
{
    GtkBox parent;
};

struct _FrequentContactsViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _FrequentContactsViewPrivate FrequentContactsViewPrivate;

struct _FrequentContactsViewPrivate
{
};

G_DEFINE_TYPE_WITH_PRIVATE(FrequentContactsView, frequent_contacts_view, GTK_TYPE_BOX);

#define FREQUENT_CONTACTS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), FREQUENT_CONTACTS_VIEW_TYPE, FrequentContactsViewPrivate))


static void
copy_contact_info(GtkWidget *item, G_GNUC_UNUSED gpointer user_data)
{
    gpointer data = g_object_get_data(G_OBJECT(item), COPY_DATA_KEY);
    g_return_if_fail(data);
    gchar* text = (gchar *)data;
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clip, text, -1);
}

static void
render_contact_photo(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *filter_model,
                     GtkTreeIter *filter_iter,
                     G_GNUC_UNUSED gpointer data)
{
    /* convert to original model */
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(filter_model));
    gtk_tree_model_filter_convert_iter_to_child_iter(
        GTK_TREE_MODEL_FILTER(filter_model),
        &iter,
        filter_iter);

    /* get contact method */
    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
    if (idx.isValid()) {
        auto n = CategorizedBookmarkModel::instance()->getNumber(idx);
        /* get photo */
        QVariant var_p = PixbufDelegate::instance()->callPhoto(n, QSize(50, 50), false);
        std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
        g_object_set(G_OBJECT(cell), "pixbuf", photo.get(), NULL);
        return;
    }

    /* otherwise, make sure its an empty pixbuf */
    g_object_set(G_OBJECT(cell), "pixbuf", NULL, NULL);
}

static void
render_name_and_contact_method(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                               GtkCellRenderer *cell,
                               GtkTreeModel *filter_model,
                               GtkTreeIter *filter_iter,
                               G_GNUC_UNUSED gpointer data)
{
    gchar *text = NULL;

    /* convert to original model */
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(filter_model));
    gtk_tree_model_filter_convert_iter_to_child_iter(
        GTK_TREE_MODEL_FILTER(filter_model),
        &iter,
        filter_iter);

    /* get contact method */
    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
    if (idx.isValid()) {
        /* get name and number */
        QVariant c = idx.data(static_cast<int>(Call::Role::Name));
        QVariant n = idx.data(static_cast<int>(Call::Role::Number));

        text = g_strdup_printf("%s\n <span fgcolor=\"gray\">%s</span>",
                                c.value<QString>().toUtf8().constData(),
                                n.value<QString>().toUtf8().constData());
    }


    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(text);
}

static void
activate_item(GtkTreeView *tree_view,
              GtkTreePath *path,
              G_GNUC_UNUSED GtkTreeViewColumn *column,
              G_GNUC_UNUSED gpointer user_data)
{
    GtkTreeModel *filter_model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter filter_iter;
    if (gtk_tree_model_get_iter(filter_model, &filter_iter, path)) {
        /* convert to original model */
        GtkTreeIter iter;
        GtkTreeModel *model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(filter_model));
        gtk_tree_model_filter_convert_iter_to_child_iter(
            GTK_TREE_MODEL_FILTER(filter_model),
            &iter,
            &filter_iter);
        QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
        if (idx.isValid()) {
            QVariant var_n = idx.data(static_cast<int>(Call::Role::ContactMethod));
            if (var_n.isValid())
                place_new_call(var_n.value<ContactMethod *>());
        }
    }
}

static gboolean
popup_menu(GtkTreeView *treeview, GdkEventButton *event, G_GNUC_UNUSED gpointer user_data)
{
    /* build popup menu when right clicking on contact item
     * user should be able to copy the contact's name or "number".
     * other functionality may be added later.
     */

    /* check for right click */
    if (event->button != BUTTON_RIGHT_CLICK || event->type != GDK_BUTTON_PRESS)
        return FALSE;

    /* we don't want a popup menu for categories for now, so everything deeper
     * than one */
    GtkTreeIter filter_iter;
    GtkTreeModel *filter_model;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    if (!gtk_tree_selection_get_selected(selection, &filter_model, &filter_iter))
        return FALSE;

    /* convert to original model */
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(filter_model));
    gtk_tree_model_filter_convert_iter_to_child_iter(
        GTK_TREE_MODEL_FILTER(filter_model),
        &iter,
        &filter_iter);

    /* deeper than a category, so create a menu */
    GtkWidget *menu = gtk_menu_new();
    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);

    /* get name and number */
    QVariant c = idx.data(static_cast<int>(Call::Role::Name));
    QVariant n = idx.data(static_cast<int>(Call::Role::Number));

    /* copy name */
    gchar *name = g_strdup_printf("%s", c.value<QString>().toUtf8().constData());
    GtkWidget *item = gtk_menu_item_new_with_mnemonic("_Copy name");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_object_set_data_full(G_OBJECT(item), COPY_DATA_KEY, name, (GDestroyNotify)g_free);
    g_signal_connect(item,
                     "activate",
                     G_CALLBACK(copy_contact_info),
                     NULL);

    gchar *number = g_strdup_printf("%s", n.value<QString>().toUtf8().constData());
    item = gtk_menu_item_new_with_mnemonic("_Copy number");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_object_set_data_full(G_OBJECT(item), COPY_DATA_KEY, number, (GDestroyNotify)g_free);
    g_signal_connect(item,
                     "activate",
                     G_CALLBACK(copy_contact_info),
                     NULL);

    /* show menu */
    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);

    return TRUE; /* we handled the event */
}

static void
frequent_contacts_view_init(FrequentContactsView *self)
{
    FrequentContactsViewPrivate *priv = FREQUENT_CONTACTS_VIEW_GET_PRIVATE(self);

    gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_VERTICAL);

    /* frequent contacts/numbers */
    GtkWidget *label_frequent = gtk_label_new("Frequent Contacts");
    gtk_box_pack_start(GTK_BOX(self), label_frequent, FALSE, TRUE, 10);

    GtkWidget *treeview_frequent = gtk_tree_view_new();
    /* set can-focus to false so that the scrollwindow doesn't jump to try to
     * contain the top of the treeview */
    gtk_widget_set_can_focus(treeview_frequent, FALSE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_frequent), FALSE);
    gtk_box_pack_start(GTK_BOX(self), treeview_frequent, FALSE, TRUE, 0);
    /* no need to show the expander since it will always be expanded */
    gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(treeview_frequent), FALSE);
    /* disable default search, we will handle it ourselves via LRC;
     * otherwise the search steals input focus on key presses */
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(treeview_frequent), FALSE);

    GtkQTreeModel *bookmark_model = gtk_q_tree_model_new(
        (QAbstractItemModel *)CategorizedBookmarkModel::instance(),
        1,
        Qt::DisplayRole, G_TYPE_STRING);

    /* create filter to only show the children of the popular contacts item */
    GtkTreePath *popular_contact_parent_path = gtk_tree_path_new_from_indices(0, -1);
    GtkTreeModel *frequent_model = gtk_tree_model_filter_new(
                                    GTK_TREE_MODEL(bookmark_model),
                                    popular_contact_parent_path);
    gtk_tree_path_free(popular_contact_parent_path);
    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview_frequent),
                            GTK_TREE_MODEL(frequent_model));

    /* photo and name/contact method column */
    GtkCellArea *area = gtk_cell_area_box_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_area(area);
    gtk_tree_view_column_set_title(column, "Name");

    /* photo renderer */
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    /* get the photo */
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc) render_contact_photo,
        NULL,
        NULL);

    /* name and contact method renderer */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_name_and_contact_method,
        NULL,
        NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_frequent), column);
    gtk_tree_view_column_set_resizable(column, TRUE);

    gtk_tree_view_expand_all(GTK_TREE_VIEW(treeview_frequent));

    g_signal_connect(treeview_frequent, "button-press-event", G_CALLBACK(popup_menu), NULL);
    g_signal_connect(treeview_frequent, "row-activated", G_CALLBACK(activate_item), NULL);

    gtk_widget_show_all(GTK_WIDGET(self));
}

static void
frequent_contacts_view_dispose(GObject *object)
{
    G_OBJECT_CLASS(frequent_contacts_view_parent_class)->dispose(object);
}

static void
frequent_contacts_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(frequent_contacts_view_parent_class)->finalize(object);
}

static void
frequent_contacts_view_class_init(FrequentContactsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = frequent_contacts_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = frequent_contacts_view_dispose;
}

GtkWidget *
frequent_contacts_view_new()
{
    gpointer self = g_object_new(FREQUENT_CONTACTS_VIEW_TYPE, NULL);

    return (GtkWidget *)self;
}