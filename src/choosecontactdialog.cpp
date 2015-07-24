/*
 *  Copyright (C) 2015 Savoir-faire Linux Inc.
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
 *  terms of the OpenSSL or SSLeay licenses, Savoir-faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "choosecontactdialog.h"

#include <contactmethod.h>
#include <personmodel.h>
#include <QtCore/QSortFilterProxyModel>
#include <memory>
#include "models/gtkqsortfiltertreemodel.h"
#include "delegates/pixbufdelegate.h"
#include "editcontactdialog.h"
#include "utils/models.h"

struct _ChooseContactDialog
{
    GtkDialog parent;
};

struct _ChooseContactDialogClass
{
    GtkDialogClass parent_class;
};

typedef struct _ChooseContactDialogPrivate ChooseContactDialogPrivate;

struct _ChooseContactDialogPrivate
{
    GtkWidget *parent;
    GtkWidget *treeview_choose_contact;
    GtkWidget *button_create_contact;

    ContactMethod *cm;

    QSortFilterProxyModel *sorted_contacts;
};

G_DEFINE_TYPE_WITH_PRIVATE(ChooseContactDialog, choose_contact_dialog, GTK_TYPE_DIALOG);

#define CHOOSE_CONTACT_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CHOOSE_CONTACT_DIALOG_TYPE, ChooseContactDialogPrivate))

static void
render_contact_photo(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *tree_model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    /* show a photo for the top level (Person) */
    GtkTreePath *path = gtk_tree_model_get_path(tree_model, iter);
    int depth = gtk_tree_path_get_depth(path);
    gtk_tree_path_free(path);
    if (depth == 1) {
        /* get person */
        QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(tree_model), iter);
        if (idx.isValid()) {
            QVariant var_c = idx.data(static_cast<int>(Person::Role::Object));
            Person *c = var_c.value<Person *>();
            /* get photo */
            QVariant var_p = PixbufDelegate::instance()->contactPhoto(c, QSize(50, 50), false);
            std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
            g_object_set(G_OBJECT(cell), "pixbuf", photo.get(), NULL);
            return;
        }
    }

    /* otherwise, make sure its an empty pixbuf */
    g_object_set(G_OBJECT(cell), "pixbuf", NULL, NULL);
}

static void
select_cb(ChooseContactDialog *self)
{
    g_return_if_fail(IS_CHOOSE_CONTACT_DIALOG(self));
    ChooseContactDialogPrivate *priv = CHOOSE_CONTACT_DIALOG_GET_PRIVATE(self);

    /* get the selected collection */
    auto selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_choose_contact));
    auto idx = get_index_from_selection(selection);
    if (idx.isValid()) {
        auto p = idx.data(static_cast<int>(Person::Role::Object)).value<Person *>();

        auto dialog = edit_contact_dialog_new(priv->cm, p, priv->parent);
        auto response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        /* if the user did not choose to save the contact, then we want to return
         * to this dialog */
        if (response == GTK_RESPONSE_OK)
            gtk_dialog_response(GTK_DIALOG(self), response);
    }
}

static void
create_contact_cb(G_GNUC_UNUSED GtkButton *button, ChooseContactDialog *self)
{
    g_return_if_fail(IS_CHOOSE_CONTACT_DIALOG(self));
    ChooseContactDialogPrivate *priv = CHOOSE_CONTACT_DIALOG_GET_PRIVATE(self);

    auto dialog = create_contact_dialog_new(priv->cm, priv->parent);
    auto response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    /* if the user did not choose to save the contact, then we want to return
     * to this dialog */
    if (response == GTK_RESPONSE_OK)
        gtk_dialog_response(GTK_DIALOG(self), response);
}

static void
choose_contact_dialog_init(ChooseContactDialog *self)
{
    gtk_widget_init_template(GTK_WIDGET(self));

    ChooseContactDialogPrivate *priv = CHOOSE_CONTACT_DIALOG_GET_PRIVATE(self);

    priv->sorted_contacts = new QSortFilterProxyModel(PersonModel::instance());
    priv->sorted_contacts->setSourceModel(PersonModel::instance());
    priv->sorted_contacts->setSortCaseSensitivity(Qt::CaseInsensitive);
    priv->sorted_contacts->sort(0);

    auto contacts_model = gtk_q_sort_filter_tree_model_new(
        priv->sorted_contacts,
        1,
        Qt::DisplayRole, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_choose_contact), GTK_TREE_MODEL(contacts_model));
    g_object_unref(contacts_model); /* the model should be freed when the view is destroyed */

    /* photo and name/contact method colparentumn */
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
        (GtkTreeCellDataFunc)render_contact_photo,
        NULL,
        NULL);

    /* name and contact method renderer */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", 0);

    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_choose_contact), column);
    gtk_tree_view_column_set_resizable(column, TRUE);

    /* connect to the button signals */
    g_signal_connect_swapped(priv->treeview_choose_contact, "row-activated", G_CALLBACK(select_cb), self);
    g_signal_connect(priv->button_create_contact, "clicked", G_CALLBACK(create_contact_cb), self);
}

static void
choose_contact_dialog_dispose(GObject *object)
{
    G_OBJECT_CLASS(choose_contact_dialog_parent_class)->dispose(object);
}

static void
choose_contact_dialog_finalize(GObject *object)
{
    ChooseContactDialog *self = CHOOSE_CONTACT_DIALOG(object);
    ChooseContactDialogPrivate *priv = CHOOSE_CONTACT_DIALOG_GET_PRIVATE(self);

    delete priv->sorted_contacts;

    G_OBJECT_CLASS(choose_contact_dialog_parent_class)->finalize(object);
}

static void
choose_contact_dialog_class_init(ChooseContactDialogClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = choose_contact_dialog_finalize;
    G_OBJECT_CLASS(klass)->dispose = choose_contact_dialog_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/cx/ring/RingGnome/choosecontactdialog.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), ChooseContactDialog, treeview_choose_contact);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), ChooseContactDialog, button_create_contact);
}

GtkWidget *
choose_contact_dialog_new(ContactMethod *cm, GtkWidget *parent)
{
    g_return_val_if_fail(cm, NULL);

    if (parent && GTK_IS_WIDGET(parent)) {
        /* get parent window so we can center on it */
        parent = gtk_widget_get_toplevel(GTK_WIDGET(parent));
        if (!gtk_widget_is_toplevel(parent)) {
            parent = NULL;
            g_debug("could not get top level parent");
        }
    }

    gpointer self = g_object_new(CHOOSE_CONTACT_DIALOG_TYPE, "transient-for", parent, NULL);

    ChooseContactDialogPrivate *priv = CHOOSE_CONTACT_DIALOG_GET_PRIVATE(self);
    priv->parent = parent;
    priv->cm = cm;

    return (GtkWidget *)self;
}
