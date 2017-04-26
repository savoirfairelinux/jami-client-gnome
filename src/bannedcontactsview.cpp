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

#include "bannedcontactsview.h"

// GTK
#include <gtk/gtk.h>

// Ring Client
#include "models/gtkqtreemodel.h"
#include "native/pixbufmanipulator.h"

// Std
#include <memory>

// LRC
#include <globalinstances.h>
#include <personmodel.h>

// Qt
#include <QSize>

/**
 * gtk structure
 */
struct _BannedContactsView
{
    GtkTreeView parent;
};

/**
 * gtk class structure
 */
struct _BannedContactsViewClass
{
    GtkTreeViewClass parent_class;
};

typedef struct _BannedContactsViewPrivate BannedContactsViewPrivate;

/**
 * gtk private structure
 */
struct _BannedContactsViewPrivate
{
};

G_DEFINE_TYPE_WITH_PRIVATE(BannedContactsView, banned_contacts_view, GTK_TYPE_TREE_VIEW);

#define BANNED_CONTACTS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), BANNED_CONTACTS_VIEW_TYPE, BannedContactsViewPrivate))

/**
 * callback function to render the photo of the peer
 */
static void
render_contact_photo(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{

    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);
    QVariant object = idx.data(static_cast<int>(Ring::Role::Object));

    std::shared_ptr<GdkPixbuf> image;

    if (idx.isValid() && object.isValid()) {
        QVariant var_photo;

        /* we only have person */
        if (auto person = object.value<Person *>())
            var_photo = GlobalInstances::pixmapManipulator().contactPhoto(person, QSize(50, 50), false);

        if (var_photo.isValid())
            image = var_photo.value<std::shared_ptr<GdkPixbuf>>();
        else // fallback
            g_object_set(G_OBJECT(cell), "width", 50, NULL);
    }

    g_object_set(G_OBJECT(cell), "pixbuf", image.get(), NULL);
}

/**
 * callback function to render the name and info of the peer
 */
static void
render_name_and_info(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     GtkTreeView *treeview)
{
    gchar *text = NULL;

    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);

    /* check if this iter is selected */
    gboolean is_selected = FALSE;
    if (GTK_IS_TREE_VIEW(treeview)) {
        auto selection = gtk_tree_view_get_selection(treeview);
        is_selected = gtk_tree_selection_iter_is_selected(selection, iter);
    }

    auto type = idx.data(static_cast<int>(Ring::Role::ObjectType));
    if (idx.isValid() && type.isValid()) {
        if (type.value<Ring::ObjectType>() == Ring::ObjectType::Person) {
            /* get name. */
            auto name_qstring = idx.data(static_cast<int>(Ring::Role::Name)).value<QString>();
            auto name_std = name_qstring.toStdString();

            /* get best id. */
            auto id_qstring = idx.data(static_cast<int>(Person::Role::IdOfLastCMUsed)).value<QString>();
            auto id_std = id_qstring.toStdString();

            if (is_selected) {
                text = g_markup_printf_escaped("%s\n<span size=\"smaller\">%s</span>",
                                               name_std.c_str(),
                                               id_std.c_str());
            } else {
                text = g_markup_printf_escaped("%s\n<span size=\"smaller\" color=\"gray\">%s</span>",
                                               name_std.c_str(),
                                               id_std.c_str());
            }
        }
    }
    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(text);
}

/**
 * gtk init function
 */
static void
banned_contacts_view_init(BannedContactsView *self)
{
    BannedContactsViewPrivate *priv = BANNED_CONTACTS_VIEW_GET_PRIVATE(self);

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(self), FALSE);
    /* no need to show the expander since it will always be expanded */
    gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(self), FALSE);
    /* disable default search, we will handle it ourselves via LRC;
     * otherwise the search steals input focus on key presses */
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(self), FALSE);

    GtkQTreeModel *recent_model = gtk_q_tree_model_new(
        PersonModel::instance().bannedPeopleProxy(),
        1,
        0, Qt::DisplayRole, G_TYPE_STRING);

    gtk_tree_view_set_model(GTK_TREE_VIEW(self),
                            GTK_TREE_MODEL(recent_model));


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

    /* name/cm and status renderer */
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
banned_contacts_view_dispose(GObject *object)
{
    BannedContactsView *self = BANNED_CONTACTS_VIEW(object);
    BannedContactsViewPrivate *priv = BANNED_CONTACTS_VIEW_GET_PRIVATE(self);

    G_OBJECT_CLASS(banned_contacts_view_parent_class)->dispose(object);
}

/**
 * gtk finalize function
 */
static void
banned_contacts_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(banned_contacts_view_parent_class)->finalize(object);
}

/**
 * gtk class init function
 */
static void
banned_contacts_view_class_init(BannedContactsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = banned_contacts_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = banned_contacts_view_dispose;
}

/**
 * gtk new function
 */
GtkWidget *
banned_contacts_view_new()
{
    gpointer self = g_object_new(BANNED_CONTACTS_VIEW_TYPE, NULL);

    return (GtkWidget *)self;
}
