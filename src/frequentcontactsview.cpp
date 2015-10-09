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

#include "frequentcontactsview.h"

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
#include <phonedirectorymodel.h>
#include <call.h>
#include "utils/menus.h"

static constexpr const char* COPY_DATA_KEY = "copy_data";

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
frequent_contacts_view_init(FrequentContactsView *self)
{
    gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_VERTICAL);

    /* frequent contacts/numbers */
    GtkWidget *treeview_frequent = gtk_tree_view_new();
    /* set can-focus to false so that the scrollwindow doesn't jump to try to
     * contain the top of the treeview */
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_frequent), FALSE);
    gtk_box_pack_start(GTK_BOX(self), treeview_frequent, TRUE, TRUE, 0);
    /* no need to show the expander since it will always be expanded */
    gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(treeview_frequent), FALSE);
    /* disable default search, we will handle it ourselves via LRC;
     * otherwise the search steals input focus on key presses */
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(treeview_frequent), FALSE);

    auto bookmark_model = gtk_list_store_new(3, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);

    GtkTreeIter iter;
    for (int i = 0; i < 10; i++) {
      // Add a new row to the model
      gtk_list_store_append(bookmark_model, &iter);
      gtk_list_store_set(bookmark_model, &iter,
                         0, i,
                         1, "string 1",
                         2, "string 2",
                         -1);
    }

    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview_frequent),
                            GTK_TREE_MODEL(bookmark_model));

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("1", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_frequent), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("2", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_frequent), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("3", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_frequent), column);

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
