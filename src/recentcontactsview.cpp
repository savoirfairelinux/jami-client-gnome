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
 */

#include "recentcontactsview.h"

#include <gtk/gtk.h>
// #include "models/gtkqtreemodel.h"
#include "models/gtkqsortfiltertreemodel.h"
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

#define COPY_DATA_KEY "copy_data"

struct _RecentContactsView
{
    GtkBox parent;
};

struct _RecentContactsViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _RecentContactsViewPrivate RecentContactsViewPrivate;

struct _RecentContactsViewPrivate
{
    GtkWidget *treeview_recent;
    GtkWidget *overlay_button;
};

G_DEFINE_TYPE_WITH_PRIVATE(RecentContactsView, recent_contacts_view, GTK_TYPE_BOX);

#define RECENT_CONTACTS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RECENT_CONTACTS_VIEW_TYPE, RecentContactsViewPrivate))

static void
update_call_model_selection(GtkTreeSelection *selection, G_GNUC_UNUSED gpointer user_data)
{
    auto current_proxy = get_index_from_selection(selection);
    auto current = RecentModel::instance()->peopleProxy()->mapToSource(current_proxy);
    Call *call_to_select = RecentModel::instance()->getActiveCall(current);
    auto to_select = CallModel::instance()->getIndex(call_to_select);
    if (to_select.isValid()) {
        /* make sure we don't call HOLD more than once on the same index, by
         * checking which one is currently selected */
        auto current_selection = CallModel::instance()->selectionModel()->currentIndex();
        if (to_select != current_selection) {
            /* if the call is on hold, we want to put it off hold automatically
             * when switching to it */
            if (call_to_select->state() == Call::State::HOLD) {
                call_to_select << Call::Action::HOLD;
            }

            CallModel::instance()->selectionModel()->setCurrentIndex(to_select, QItemSelectionModel::ClearAndSelect);
        }
    } else {
        CallModel::instance()->selectionModel()->clearCurrentIndex();
    }
}

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
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), iter);
    // QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);


    std::shared_ptr<GdkPixbuf> photo;
    /* we only want to render a photo for the top nodes: Person, ContactMethod (, later Conference) */
    QVariant object = idx.data(static_cast<int>(Ring::Role::Object));
    if (idx.isValid() && object.isValid()) {
        QVariant var_photo;
        if (auto person = object.value<Person *>()) {
            var_photo = GlobalInstances::pixmapManipulator().contactPhoto(person, QSize(50, 50), false);
        } else if (auto cm = object.value<ContactMethod *>()) {
            /* get photo, note that this should in all cases be the fallback avatar, since there
             * shouldn't be a person associated with this contact method */
            var_photo = GlobalInstances::pixmapManipulator().callPhoto(cm, QSize(50, 50), false);
        }
        if (var_photo.isValid())
            photo = var_photo.value<std::shared_ptr<GdkPixbuf>>();
    }

    g_object_set(G_OBJECT(cell), "pixbuf", photo.get(), NULL);
}

static void
render_name_and_info(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    gchar *text = NULL;

    QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), iter);
    // QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);

    auto type = idx.data(static_cast<int>(Ring::Role::ObjectType));
    if (idx.isValid() && type.isValid()) {
        switch (type.value<Ring::ObjectType>()) {
            case Ring::ObjectType::Person:
            case Ring::ObjectType::ContactMethod:
            {
                auto var_name = idx.data(Qt::DisplayRole);
                auto var_lastused = idx.data(static_cast<int>(Ring::Role::LastUsed));
                auto var_status = idx.data(static_cast<int>(Ring::Role::FormattedState));

                QString name, status;

                if (var_name.isValid())
                    name = var_name.value<QString>();

                // show the status if there is a call, otherwise the last used date/time
                if (var_status.isValid()) {
                    status += var_status.value<QString>();
                }else if (var_lastused.isValid()) {
                    auto date_time = var_lastused.value<QDateTime>();
                    auto category = HistoryTimeCategoryModel::timeToHistoryConst(date_time.toTime_t());

                    // if it is 'today', then we only want to show the time
                    if (category != HistoryTimeCategoryModel::HistoryConst::Today) {
                        status += HistoryTimeCategoryModel::timeToHistoryCategory(date_time.toTime_t());
                    }
                    // we only want to show the time if it is less than a week ago
                    if (category < HistoryTimeCategoryModel::HistoryConst::A_week_ago) {
                        if (!status.isEmpty())
                            status += ", ";
                        status += QLocale::system().toString(date_time.time(), QLocale::ShortFormat);
                    }
                }

                text = g_strdup_printf("%s\n<span size=\"smaller\" color=\"gray\">%s</span>",
                                       name.toUtf8().constData(),
                                       status.toUtf8().constData());
            }
            break;
            case Ring::ObjectType::Call:
            {
                auto var_status = idx.data(static_cast<int>(Ring::Role::FormattedState));

                QString status;

                if (var_status.isValid())
                    status += var_status.value<QString>();

                text = g_strdup_printf("<span size=\"smaller\" color=\"gray\">%s</span>", status.toUtf8().constData());
            }
            break;
            case Ring::ObjectType::Media:
            // nothing to do for now
            break;
        }
    }

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(text);
}

static void
render_call_duration(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    gchar *text = NULL;

    QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), iter);
    // QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);

    auto type = idx.data(static_cast<int>(Ring::Role::ObjectType));
    if (idx.isValid() && type.isValid()) {
        switch (type.value<Ring::ObjectType>()) {
            case Ring::ObjectType::Person:
            case Ring::ObjectType::ContactMethod:
            {
                // check if there are any children (calls); we need to convert to source model in
                // case there is only one
                auto idx_source = RecentModel::instance()->peopleProxy()->mapToSource(idx);
                auto duration = idx.data(static_cast<int>(Ring::Role::Length));
                if (idx_source.isValid()
                    && (idx_source.model()->rowCount(idx_source) == 1)
                    && duration.isValid())
                {
                    text = g_strdup_printf("%s", duration.value<QString>().toUtf8().constData());
                }
            }
            break;
            case Ring::ObjectType::Call:
            {
                auto duration = idx.data(static_cast<int>(Ring::Role::Length));

                if (duration.isValid())
                    text = g_strdup_printf("%s", duration.value<QString>().toUtf8().constData());
            }
            break;
            case Ring::ObjectType::Media:
            // nothing to do for now
            break;
        }
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
    // GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    // GtkTreeIter iter;
    // if (gtk_tree_model_get_iter(model, &iter, path)) {
    //     QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
    //     if (idx.isValid()) {
    //         QVariant var_n = idx.data(static_cast<int>(Call::Role::ContactMethod));
    //         if (var_n.isValid())
    //             place_new_call(var_n.value<ContactMethod *>());
    //     }
    // }
}

static gboolean
create_popup_menu(GtkTreeView *treeview, GdkEventButton *event, G_GNUC_UNUSED gpointer user_data)
{
    // /* build popup menu when right clicking on contact item
    //  * user should be able to copy the contact's name or "number".
    //  * or add the "number" to his contact list, if not already so
    //  */
    //
    // /* check for right click */
    // if (event->button != BUTTON_RIGHT_CLICK || event->type != GDK_BUTTON_PRESS)
    //     return FALSE;
    //
    // GtkTreeIter iter;
    // GtkTreeModel *model;
    // GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    // if (!gtk_tree_selection_get_selected(selection, &model, &iter))
    //     return FALSE;
    //
    // GtkWidget *menu = gtk_menu_new();
    // QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
    //
    // /* get name and number */
    // QVariant c = idx.data(static_cast<int>(Call::Role::Name));
    // QVariant n = idx.data(static_cast<int>(Call::Role::Number));
    //
    // /* copy name */
    // gchar *name = g_strdup_printf("%s", c.value<QString>().toUtf8().constData());
    // GtkWidget *item = gtk_menu_item_new_with_mnemonic(_("_Copy name"));
    // gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    // g_object_set_data_full(G_OBJECT(item), COPY_DATA_KEY, name, (GDestroyNotify)g_free);
    // g_signal_connect(item,
    //                  "activate",
    //                  G_CALLBACK(copy_contact_info),
    //                  NULL);
    //
    // gchar *number = g_strdup_printf("%s", n.value<QString>().toUtf8().constData());
    // item = gtk_menu_item_new_with_mnemonic(_("_Copy number"));
    // gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    // g_object_set_data_full(G_OBJECT(item), COPY_DATA_KEY, number, (GDestroyNotify)g_free);
    // g_signal_connect(item,
    //                  "activate",
    //                  G_CALLBACK(copy_contact_info),
    //                  NULL);
    //
    //  /* get the call object from the selected item
    //   * check if it is already linked to a person, if not, then offer to either
    //   * add to a new or existing contact */
    //  const auto& var_cm = idx.data(static_cast<int>(Call::Role::ContactMethod));
    //  if (idx.isValid() && var_cm.isValid()) {
    //      if (auto contactmethod = var_cm.value<ContactMethod *>()) {
    //          if (!contact_method_has_contact(contactmethod)) {
    //              /* get rectangle */
    //              auto path = gtk_tree_model_get_path(model, &iter);
    //              auto column = gtk_tree_view_get_column(treeview, 0);
    //              GdkRectangle rect;
    //              gtk_tree_view_get_cell_area(treeview, path, column, &rect);
    //              gtk_tree_view_convert_bin_window_to_widget_coords(treeview, rect.x, rect.y, &rect.x, &rect.y);
    //              gtk_tree_path_free(path);
    //              auto add_to = menu_item_add_to_contact(contactmethod, GTK_WIDGET(treeview), &rect);
    //              gtk_menu_shell_append(GTK_MENU_SHELL(menu), add_to);
    //          }
    //      }
    //  }
    //
    // /* show menu */
    // gtk_widget_show_all(menu);
    // gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);

    return TRUE; /* we handled the event */
}

static void
expand_if_child(G_GNUC_UNUSED GtkTreeModel *tree_model,
                GtkTreePath  *path,
                G_GNUC_UNUSED GtkTreeIter  *iter,
                GtkTreeView  *treeview)
{
    if (gtk_tree_path_get_depth(path) > 1)
        gtk_tree_view_expand_to_path(treeview, path);
}

static void
recent_contacts_view_init(RecentContactsView *self)
{
    RecentContactsViewPrivate *priv = RECENT_CONTACTS_VIEW_GET_PRIVATE(self);

    gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_VERTICAL);

    GtkWidget *treeview_recent = gtk_tree_view_new();
    priv->treeview_recent = treeview_recent;
    gtk_box_pack_start(GTK_BOX(self), treeview_recent, TRUE, TRUE, 0);

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_recent), FALSE);
    /* no need to show the expander since it will always be expanded */
    gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(treeview_recent), FALSE);
    /* disable default search, we will handle it ourselves via LRC;
     * otherwise the search steals input focus on key presses */
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(treeview_recent), FALSE);
    gtk_tree_view_set_level_indentation(GTK_TREE_VIEW(treeview_recent), 10);

    GtkQSortFilterTreeModel *recent_model = gtk_q_sort_filter_tree_model_new(
        RecentModel::instance()->peopleProxy(),
        2,
        Qt::DisplayRole, G_TYPE_STRING);

    // GtkQTreeModel *recent_model = gtk_q_tree_model_new(
    //     RecentModel::instance(),
    //     1,
    //     Qt::DisplayRole, G_TYPE_STRING);

    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview_recent),
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
        NULL,
        NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_recent), column);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, TRUE);

    /* call duration column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Duration", renderer, NULL);
    // gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_recent), column);
    // gtk_tree_view_column_set_expand(column, FALSE);
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_call_duration,
        NULL,
        NULL);

    gtk_tree_view_expand_all(GTK_TREE_VIEW(treeview_recent));

    // g_signal_connect(treeview_recent, "button-press-event", G_CALLBACK(create_popup_menu), NULL);
    // g_signal_connect(treeview_recent, "row-activated", G_CALLBACK(activate_item), NULL);
    g_signal_connect(recent_model, "row-inserted", G_CALLBACK(expand_if_child), treeview_recent);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview_recent));
    g_signal_connect(selection, "changed", G_CALLBACK(update_call_model_selection), NULL);

    gtk_widget_show_all(GTK_WIDGET(self));
}

static void
recent_contacts_view_dispose(GObject *object)
{
    G_OBJECT_CLASS(recent_contacts_view_parent_class)->dispose(object);
}

static void
recent_contacts_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(recent_contacts_view_parent_class)->finalize(object);
}

static void
recent_contacts_view_class_init(RecentContactsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = recent_contacts_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = recent_contacts_view_dispose;
}

GtkWidget *
recent_contacts_view_new()
{
    gpointer self = g_object_new(RECENT_CONTACTS_VIEW_TYPE, NULL);

    return (GtkWidget *)self;
}
