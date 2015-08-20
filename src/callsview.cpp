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

#include "callsview.h"

#include <gtk/gtk.h>
#include "models/gtkqtreemodel.h"
#include <callmodel.h>
#include <QtCore/QItemSelectionModel>
#include "utils/models.h"
#include <delegates/delegatemanager.h>
#include "delegates/pixbufdelegate.h"
#include <QtCore/QSize>
#include "defines.h"
#include "utils/menus.h"

struct _CallsView
{
    GtkRevealer parent;
};

struct _CallsViewClass
{
    GtkRevealerClass parent_class;
};

typedef struct _CallsViewPrivate CallsViewPrivate;

struct _CallsViewPrivate
{
    GtkWidget *treeview_calls;
    QMetaObject::Connection selection_updated;
    QMetaObject::Connection calls_added;
    QMetaObject::Connection calls_removed;
};

G_DEFINE_TYPE_WITH_PRIVATE(CallsView, calls_view, GTK_TYPE_REVEALER);

#define CALLS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CALLS_VIEW_TYPE, CallsViewPrivate))

static void
update_call_model_selection(GtkTreeSelection *selection, G_GNUC_UNUSED gpointer user_data)
{
    QModelIndex current = get_index_from_selection(selection);
    if (current.isValid()) {

        /* make sure we don't call HOLD more than once on the same index, by
         * checking which one is currently selected */
        auto current_selection = CallModel::instance()->selectionModel()->currentIndex();
        if (current != current_selection) {
            /* if the call is on hold, we want to put it off hold automatically
             * when switching to it */
            auto call = CallModel::instance()->getCall(current);
            if (call->state() == Call::State::HOLD) {
                call << Call::Action::HOLD;
            }

            CallModel::instance()->selectionModel()->setCurrentIndex(current, QItemSelectionModel::ClearAndSelect);
        }
    } else {
        CallModel::instance()->selectionModel()->clearCurrentIndex();
    }
}

static void
render_call_photo(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                  GtkCellRenderer *cell,
                  GtkTreeModel *tree_model,
                  GtkTreeIter *iter,
                  G_GNUC_UNUSED gpointer data)
{
    /* get call */
    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(tree_model), iter);
    if (idx.isValid()) {
        QVariant var_c = idx.data(static_cast<int>(Call::Role::Object));
        Call *c = var_c.value<Call *>();

        /* we only want to show the photo once the call is past creation
         * since before then we likely don't know the contact yet anyways */
        if (c->lifeCycleState() != Call::LifeCycleState::CREATION) {
            /* get photo */
            QVariant var_p = getDelegateManager()->getPixmapManipulationDelegate()->callPhoto(c, QSize(50, 50), false);
            std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
            g_object_set(G_OBJECT(cell), "pixbuf", photo.get(), NULL);
            return;
        }
    }

    /* otherwise, make sure its an empty pixbuf */
    g_object_set(G_OBJECT(cell), "pixbuf", NULL, NULL);
}

static void
render_name_and_contact_method(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                               GtkCellRenderer *cell,
                               GtkTreeModel *tree_model,
                               GtkTreeIter *iter,
                               G_GNUC_UNUSED gpointer data)
{
    gchar *text = NULL;
    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(tree_model), iter);
    if (idx.isValid()) {
        QVariant state = idx.data(static_cast<int>(Call::Role::LifeCycleState));
        QVariant name = idx.data(static_cast<int>(Call::Role::Name));
        QVariant number = idx.data(static_cast<int>(Call::Role::Number));

        /* only show the number being entered while in creation state */
        if (state.value<Call::LifeCycleState>() == Call::LifeCycleState::CREATION) {
            text = g_strdup_printf("%s", number.value<QString>().toUtf8().constData());
        } else {
            text = g_strdup_printf("%s\n <span fgcolor=\"gray\">%s</span>",
                                   name.value<QString>().toUtf8().constData(),
                                   number.value<QString>().toUtf8().constData());
        }
    }

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(text);
}

static gboolean
create_popup_menu(GtkTreeView *treeview, GdkEventButton *event, G_GNUC_UNUSED gpointer user_data)
{
    /* build popup menu when right clicking on a call
     * user should be able to add the "number" to their contacts if it is not already so
     */

    /* check for right click */
    if (event->button != BUTTON_RIGHT_CLICK || event->type != GDK_BUTTON_PRESS)
        return FALSE;

    GtkWidget *menu = gtk_menu_new();

    /* get the call object from the selected item
     * check if it is already linked to a person, if not, then offer to either
     * add to a new or existing contact */
    auto selection = gtk_tree_view_get_selection(treeview);
    const auto& idx = get_index_from_selection(selection);
    const auto& var_c = idx.data(static_cast<int>(Call::Role::Object));
    if (idx.isValid() && var_c.isValid()) {
        if (auto call = var_c.value<Call *>()) {
            auto contactmethod = call->peerContactMethod();
            if (!contact_method_has_contact(contactmethod)) {
                GtkTreeIter iter;
                GtkTreeModel *model;
                gtk_tree_selection_get_selected(selection, &model, &iter);
                auto path = gtk_tree_model_get_path(model, &iter);
                auto column = gtk_tree_view_get_column(treeview, 0);
                GdkRectangle rect;
                gtk_tree_view_get_cell_area(treeview, path, column, &rect);
                gtk_tree_view_convert_bin_window_to_widget_coords(treeview, rect.x, rect.y, &rect.x, &rect.y);
                gtk_tree_path_free(path);
                auto add_to = menu_item_add_to_contact(contactmethod, GTK_WIDGET(treeview), &rect);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), add_to);

                /* no other items, so show menu here */
                gtk_widget_show_all(menu);
                gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);

                return TRUE;
            }
        }
    }

    return FALSE;
}

static void
calls_view_init(CallsView *self)
{
    CallsViewPrivate *priv = CALLS_VIEW_GET_PRIVATE(self);

    /* hide if there are no calls */
    gtk_revealer_set_reveal_child(GTK_REVEALER(self),
                                  CallModel::instance()->rowCount());
    priv->calls_added = QObject::connect(
        CallModel::instance(),
        &QAbstractItemModel::rowsInserted,
        [=] (G_GNUC_UNUSED const QModelIndex &parent,
             G_GNUC_UNUSED int first,
             G_GNUC_UNUSED int last)
        {
            gtk_revealer_set_reveal_child(GTK_REVEALER(self),
                                          CallModel::instance()->rowCount());
        }
    );

    priv->calls_removed = QObject::connect(
        CallModel::instance(),
        &QAbstractItemModel::rowsRemoved,
        [=] (G_GNUC_UNUSED const QModelIndex &parent,
             G_GNUC_UNUSED int first,
             G_GNUC_UNUSED int last)
        {
            gtk_revealer_set_reveal_child(GTK_REVEALER(self),
                                          CallModel::instance()->rowCount());
        }
    );

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_bottom(box, 10);
    gtk_container_add(GTK_CONTAINER(self), box);

    /* current calls label */
    GtkWidget *label = gtk_label_new("Current Calls");
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, TRUE, 10);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(box), scrolled_window, FALSE, TRUE, 0);

    /* disable vertical scroll... we always want all the calls to be visible */
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_NEVER);

    priv->treeview_calls = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(priv->treeview_calls), FALSE);
    /* disable default search, we will handle it ourselves via LRC;
     * otherwise the search steals input focus on key presses */
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(priv->treeview_calls), FALSE);
    gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(priv->treeview_calls), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), priv->treeview_calls);

    /* call model */
    GtkQTreeModel *call_model;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    call_model = gtk_q_tree_model_new(
        CallModel::instance(),
        4,
        Call::Role::Name, G_TYPE_STRING,
        Call::Role::Number, G_TYPE_STRING,
        Call::Role::Length, G_TYPE_STRING,
        Call::Role::State, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_calls), GTK_TREE_MODEL(call_model));

    /* call photo, name/number column */
    GtkCellArea *area = gtk_cell_area_box_new();
    column = gtk_tree_view_column_new_with_area(area);
    gtk_tree_view_column_set_title(column, "Call");

    /* photo renderer */
    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    /* get the photo */
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_call_photo,
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

    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_calls), column);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, TRUE);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Duration", renderer, "text", 2, NULL);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_calls), column);
    gtk_tree_view_column_set_expand(column, FALSE);

    /* connect signals to and from the slection model of the call model */
    priv->selection_updated = QObject::connect(
        CallModel::instance()->selectionModel(),
        &QItemSelectionModel::currentChanged,
        [=](const QModelIndex current, const QModelIndex & previous) {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_calls));

            /* first unselect the previous */
            if (previous.isValid()) {
                GtkTreeIter old_iter;
                if (gtk_q_tree_model_source_index_to_iter(call_model, previous, &old_iter)) {
                    gtk_tree_selection_unselect_iter(selection, &old_iter);
                } else {
                    g_warning("Trying to unselect invalid GtkTreeIter");
                }
            }

            /* select the current */
            if (current.isValid()) {
                GtkTreeIter new_iter;
                if (gtk_q_tree_model_source_index_to_iter(call_model, current, &new_iter)) {
                    gtk_tree_selection_select_iter(selection, &new_iter);
                } else {
                    g_warning("SelectionModel of CallModel changed to invalid QModelIndex?");
                }
            }
        }
    );

    GtkTreeSelection *call_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_calls));
    g_signal_connect(call_selection, "changed", G_CALLBACK(update_call_model_selection), NULL);

    g_signal_connect(priv->treeview_calls, "button-press-event", G_CALLBACK(create_popup_menu), NULL);

    gtk_widget_show_all(GTK_WIDGET(self));
}

static void
calls_view_dispose(GObject *object)
{
    CallsView *self = CALLS_VIEW(object);
    CallsViewPrivate *priv = CALLS_VIEW_GET_PRIVATE(self);

    QObject::disconnect(priv->selection_updated);
    QObject::disconnect(priv->calls_added);
    QObject::disconnect(priv->calls_removed);

    G_OBJECT_CLASS(calls_view_parent_class)->dispose(object);
}

static void
calls_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(calls_view_parent_class)->finalize(object);
}

static void
calls_view_class_init(CallsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = calls_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = calls_view_dispose;
}

GtkWidget *
calls_view_new()
{
    gpointer self = g_object_new(CALLS_VIEW_TYPE, NULL);

    return (GtkWidget *)self;
}

GtkTreeSelection *
calls_view_get_selection(CallsView *calls_view)
{
    g_return_val_if_fail(IS_CALLS_VIEW(calls_view), NULL);
    CallsViewPrivate *priv = CALLS_VIEW_GET_PRIVATE(calls_view);

    return gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_calls));
}
