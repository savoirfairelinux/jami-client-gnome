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

#include "ringmainwindow.h"

#include <gtk/gtk.h>
#include "models/gtkqtreemodel.h"
#include <callmodel.h>
#include "incomingcallview.h"

struct _RingMainWindow
{
    GtkApplicationWindow parent;
};

struct _RingMainWindowClass
{
    GtkApplicationWindowClass parent_class;
};

typedef struct _RingMainWindowPrivate RingMainWindowPrivate;

struct _RingMainWindowPrivate
{
    GtkWidget *gears;
    GtkWidget *gears_image;
    GtkWidget *treeview_call;
    GtkWidget *search_entry;

    GtkWidget *stack_main_view;
};

G_DEFINE_TYPE_WITH_PRIVATE(RingMainWindow, ring_main_window, GTK_TYPE_APPLICATION_WINDOW);

#define RING_MAIN_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RING_MAIN_WINDOW_TYPE, RingMainWindowPrivate))

static QModelIndex
get_index_from_selection(GtkTreeSelection *selection)
{
    GtkTreeIter iter;
    GtkTreeModel *model = NULL;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
    } else {
        return QModelIndex();
    }
}

static void
call_selection_changed(GtkTreeSelection *selection, gpointer win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(win));
    Call *call = NULL;
    // Call::State state;

    QModelIndex idx = get_index_from_selection(selection);
    if (idx.isValid()) {
        // state = call->state();
        // if (state == Call::State::INCOMING) {
            GtkWidget *new_call_view = incoming_call_view_new();
            incoming_call_view_set_call_info(INCOMING_CALL_VIEW(new_call_view), idx);
            gtk_stack_add_named(GTK_STACK(priv->stack_main_view), new_call_view, "new call");
            gtk_stack_set_transition_type(GTK_STACK(priv->stack_main_view), GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
            gtk_stack_set_transition_duration (GTK_STACK(priv->stack_main_view), 300);
            gtk_stack_set_visible_child(GTK_STACK(priv->stack_main_view), new_call_view);
        // }
    } else {
        g_debug("could not get call from selection");
    }
}

static void
ring_main_window_init(RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);
    gtk_widget_init_template(GTK_WIDGET(win));

     /* set window icon */
    GError *error = NULL;
    GdkPixbuf* icon = gdk_pixbuf_new_from_resource("/cx/ring/RingGnome/ring-symbol-blue", &error);
    if (icon == NULL) {
        g_debug("Could not load icon: %s", error->message);
        g_error_free(error);
    } else
        gtk_window_set_icon(GTK_WINDOW(win), icon);

    /* set menu icon */
    GdkPixbuf* ring_gears = gdk_pixbuf_new_from_resource_at_scale("/cx/ring/RingGnome/ring-logo-blue",
                                                                  -1, 22, TRUE, &error);
    if (ring_gears == NULL) {
        g_debug("Could not load icon: %s", error->message);
        g_error_free(error);
    } else
        gtk_image_set_from_pixbuf(GTK_IMAGE(priv->gears_image), ring_gears);

    /* gears menu */
    GtkBuilder *builder = gtk_builder_new_from_resource("/cx/ring/RingGnome/ringgearsmenu.ui");
    GMenuModel *menu = G_MENU_MODEL(gtk_builder_get_object(builder, "menu"));
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(priv->gears), menu);
    g_object_unref(builder);

    /* call model */
    GtkQTreeModel *model;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    model = gtk_q_tree_model_new(CallModel::instance(), 4,
        Call::Role::Name, G_TYPE_STRING,
        Call::Role::Number, G_TYPE_STRING,
        Call::Role::Length, G_TYPE_STRING,
        Call::Role::CallState, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_call), GTK_TREE_MODEL(model) );

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW (priv->treeview_call), column);

    // renderer = gtk_cell_renderer_text_new();
    // column = gtk_tree_view_column_new_with_attributes("Number", renderer, "text", 1, NULL);
    // gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_call), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Duration", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_call), column);

    // renderer = gtk_cell_renderer_text_new();
    // column = gtk_tree_view_column_new_with_attributes("State", renderer, "text", 3, NULL);
    // gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_call), column);

    /* stack paceholder view */
    GtkWidget *placeholder_view = gtk_tree_view_new();
    gtk_widget_show(placeholder_view);
    gtk_stack_add_named(GTK_STACK(priv->stack_main_view), placeholder_view, "placeholder");

    /* connect signals */
    GtkTreeSelection *call_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_call));
    g_signal_connect(call_selection, "changed", G_CALLBACK(call_selection_changed), win);
}

static void
ring_main_window_class_init(RingMainWindowClass *klass)
{
    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/ringmainwindow.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, treeview_call);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, gears);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, gears_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, search_entry);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, stack_main_view);
}

GtkWidget *
ring_main_window_new (GtkApplication *app)
{
    return (GtkWidget *)g_object_new(RING_MAIN_WINDOW_TYPE, "application", app, NULL);
}
