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

#include "choosecontactview.h"

#include <contactmethod.h>
#include <personmodel.h>
#include <QtCore/QSortFilterProxyModel>
#include <memory>
#include "models/gtkqtreemodel.h"
#include <globalinstances.h>
#include "native/pixbufmanipulator.h"
#include "utils/models.h"

enum
{
    PERSON_SELECTED,
    NEW_PERSON_CLICKED,

    LAST_SIGNAL
};

struct _ChooseContactView
{
    GtkBox parent;
};

struct _ChooseContactViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _ChooseContactViewPrivate ChooseContactViewPrivate;

struct _ChooseContactViewPrivate
{
    GtkWidget *treeview_choose_contact;
    GtkWidget *button_create_contact;

    ContactMethod *cm;

    QSortFilterProxyModel *sorted_contacts;
};

static guint choose_contact_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE(ChooseContactView, choose_contact_view, GTK_TYPE_BOX);

#define CHOOSE_CONTACT_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CHOOSE_CONTACT_VIEW_TYPE, ChooseContactViewPrivate))

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
        QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(tree_model), iter);
        if (idx.isValid()) {
            QVariant var_c = idx.data(static_cast<int>(Person::Role::Object));
            Person *c = var_c.value<Person *>();
            /* get photo */
            QVariant var_p = GlobalInstances::pixmapManipulator().contactPhoto(c, QSize(50, 50), false);
            std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
            g_object_set(G_OBJECT(cell), "pixbuf", photo.get(), NULL);
            return;
        }
    }

    /* otherwise, make sure its an empty pixbuf */
    g_object_set(G_OBJECT(cell), "pixbuf", NULL, NULL);
}

static void
select_cb(ChooseContactView *self)
{
    g_return_if_fail(IS_CHOOSE_CONTACT_VIEW(self));
    ChooseContactViewPrivate *priv = CHOOSE_CONTACT_VIEW_GET_PRIVATE(self);

    /* get the selected collection */
    auto selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_choose_contact));
    auto idx = get_index_from_selection(selection);
    if (idx.isValid()) {
        auto p = idx.data(static_cast<int>(Person::Role::Object)).value<Person *>();

        g_signal_emit(self, choose_contact_signals[PERSON_SELECTED], 0, p);
    } else {
        g_warning("invalid Person selected");
    }
}

static void
create_contact_cb(G_GNUC_UNUSED GtkButton *button, ChooseContactView *self)
{
    g_return_if_fail(IS_CHOOSE_CONTACT_VIEW(self));

    g_signal_emit(self, choose_contact_signals[NEW_PERSON_CLICKED], 0);
}

static void
choose_contact_view_init(ChooseContactView *self)
{
    gtk_widget_init_template(GTK_WIDGET(self));

    ChooseContactViewPrivate *priv = CHOOSE_CONTACT_VIEW_GET_PRIVATE(self);

    priv->sorted_contacts = new QSortFilterProxyModel(&PersonModel::instance());
    priv->sorted_contacts->setSourceModel(&PersonModel::instance());
    priv->sorted_contacts->setSortCaseSensitivity(Qt::CaseInsensitive);
    priv->sorted_contacts->sort(0);

    auto contacts_model = gtk_q_tree_model_new(
        priv->sorted_contacts,
        1,
        0, Qt::DisplayRole, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_choose_contact), GTK_TREE_MODEL(contacts_model));
    g_object_unref(contacts_model); /* the model should be freed when the view is destroyed */

    /* photo and name/contact method colparentumn */
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
choose_contact_view_dispose(GObject *object)
{
    G_OBJECT_CLASS(choose_contact_view_parent_class)->dispose(object);
}

static void
choose_contact_view_finalize(GObject *object)
{
    ChooseContactView *self = CHOOSE_CONTACT_VIEW(object);
    ChooseContactViewPrivate *priv = CHOOSE_CONTACT_VIEW_GET_PRIVATE(self);

    if (priv->sorted_contacts)
        delete priv->sorted_contacts;

    G_OBJECT_CLASS(choose_contact_view_parent_class)->finalize(object);
}

static void
choose_contact_view_class_init(ChooseContactViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = choose_contact_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = choose_contact_view_dispose;

    choose_contact_signals[NEW_PERSON_CLICKED] =
        g_signal_new("new-person-clicked",
            G_OBJECT_CLASS_TYPE(G_OBJECT_CLASS(klass)),
            (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
            0, /* class offset */
            NULL, /* accumulater */
            NULL, /* accu data */
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    choose_contact_signals[PERSON_SELECTED] =
        g_signal_new ("person-selected",
            G_OBJECT_CLASS_TYPE(G_OBJECT_CLASS(klass)),
            (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
            0, /* class offset */
            NULL, /* accumulater */
            NULL, /* accu data */
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/cx/ring/RingGnome/choosecontactview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), ChooseContactView, treeview_choose_contact);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), ChooseContactView, button_create_contact);
}

GtkWidget *
choose_contact_view_new(ContactMethod *cm)
{
    g_return_val_if_fail(cm, NULL);

    gpointer self = g_object_new(CHOOSE_CONTACT_VIEW_TYPE, NULL);

    ChooseContactViewPrivate *priv = CHOOSE_CONTACT_VIEW_GET_PRIVATE(self);
    priv->cm = cm;

    return (GtkWidget *)self;
}
