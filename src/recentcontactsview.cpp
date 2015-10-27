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
#include <glib/gi18n.h>
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

static constexpr const char* COPY_DATA_KEY = "copy_data";

struct _RecentContactsView
{
    GtkTreeView parent;
};

struct _RecentContactsViewClass
{
    GtkTreeViewClass parent_class;
};

typedef struct _RecentContactsViewPrivate RecentContactsViewPrivate;

struct _RecentContactsViewPrivate
{
    GtkWidget *overlay_button;

    QMetaObject::Connection selection_updated;
};

G_DEFINE_TYPE_WITH_PRIVATE(RecentContactsView, recent_contacts_view, GTK_TYPE_TREE_VIEW);

#define RECENT_CONTACTS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RECENT_CONTACTS_VIEW_TYPE, RecentContactsViewPrivate))

static void
update_call_model_selection(GtkTreeSelection *selection, G_GNUC_UNUSED gpointer user_data)
{
    auto current_proxy = get_index_from_selection(selection);
    auto current = RecentModel::instance().peopleProxy()->mapToSource(current_proxy);
    if (auto call_to_select = RecentModel::instance().getActiveCall(current)) {
        CallModel::instance().selectCall(call_to_select);
    } else {
        CallModel::instance().selectionModel()->clearCurrentIndex();
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
call_contactmethod(G_GNUC_UNUSED GtkWidget *item, ContactMethod *cm)
{
    g_return_if_fail(cm);
    place_new_call(cm);
}

static void
render_contact_photo(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), iter);

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
        else {
            // set the width of the cell rendered to the with of the photo
            // so that the other renderers are shifted to the right
            g_object_set(G_OBJECT(cell), "width", 50, NULL);
        }
    }

    g_object_set(G_OBJECT(cell), "pixbuf", photo.get(), NULL);
}

static void
render_name_and_info(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     GtkTreeView *treeview)
{
    gchar *text = NULL;

    QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), iter);

    // check if this iter is selected
    gboolean is_selected = FALSE;
    if (GTK_IS_TREE_VIEW(treeview)) {
        auto selection = gtk_tree_view_get_selection(treeview);
        is_selected = gtk_tree_selection_iter_is_selected(selection, iter);
    }

    auto type = idx.data(static_cast<int>(Ring::Role::ObjectType));
    if (idx.isValid() && type.isValid()) {
        switch (type.value<Ring::ObjectType>()) {
            case Ring::ObjectType::Person:
            case Ring::ObjectType::ContactMethod:
            {
                auto var_name = idx.data(static_cast<int>(Ring::Role::Name));
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

                /* we want the color of the status text to be the default color if this iter is
                 * selected so that the treeview is able to invert it against the selection color */
                if (is_selected) {
                    text = g_strdup_printf("%s\n<span size=\"smaller\">%s</span>",
                                           name.toUtf8().constData(),
                                           status.toUtf8().constData());
                } else {
                    text = g_strdup_printf("%s\n<span size=\"smaller\" color=\"gray\">%s</span>",
                                           name.toUtf8().constData(),
                                           status.toUtf8().constData());
                }
            }
            break;
            case Ring::ObjectType::Call:
            {
                auto var_status = idx.data(static_cast<int>(Ring::Role::FormattedState));

                QString status;

                if (var_status.isValid())
                    status += var_status.value<QString>();

                /* we want the color of the status text to be the default color if this iter is
                 * selected so that the treeview is able to invert it against the selection color */
                if (is_selected)
                    text = g_strdup_printf("<span size=\"smaller\">%s</span>", status.toUtf8().constData());
                else
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

    auto type = idx.data(static_cast<int>(Ring::Role::ObjectType));
    if (idx.isValid() && type.isValid()) {
        switch (type.value<Ring::ObjectType>()) {
            case Ring::ObjectType::Person:
            case Ring::ObjectType::ContactMethod:
            {
                // check if there are any children (calls); we need to convert to source model in
                // case there is only one
                auto idx_source = RecentModel::instance().peopleProxy()->mapToSource(idx);
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
    auto model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        auto idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), &iter);
        auto type = idx.data(static_cast<int>(Ring::Role::ObjectType));
        if (idx.isValid() && type.isValid()) {
            switch (type.value<Ring::ObjectType>()) {
                case Ring::ObjectType::Person:
                {
                    // call the last used contact method
                    // TODO: if no contact methods have been used, offer a popup to Choose
                    auto p_var = idx.data(static_cast<int>(Ring::Role::Object));
                    if (p_var.isValid()) {
                        auto person = p_var.value<Person *>();
                        auto cms = person->phoneNumbers();

                        if (!cms.isEmpty()) {
                            auto last_used_cm = cms.at(0);
                            for (int i = 1; i < cms.size(); ++i) {
                                auto new_cm = cms.at(i);
                                if (difftime(new_cm->lastUsed(), last_used_cm->lastUsed()) > 0)
                                    last_used_cm = new_cm;
                            }

                            place_new_call(last_used_cm);
                        }
                    }
                }
                break;
                case Ring::ObjectType::ContactMethod:
                {
                    // call the contact method
                    auto cm = idx.data(static_cast<int>(Ring::Role::Object));
                    if (cm.isValid())
                        place_new_call(cm.value<ContactMethod *>());
                }
                break;
                case Ring::ObjectType::Call:
                case Ring::ObjectType::Media:
                // nothing to do for now
                break;
            }
        }
    }
}

static gboolean
create_popup_menu(GtkTreeView *treeview, GdkEventButton *event, G_GNUC_UNUSED gpointer user_data)
{
    /* build popup menu when right clicking on contact item
     * user should be able to copy the contact's name or "number".
     * or add the "number" to his contact list, if not already so
     */

    /* check for right click */
    if (event->button != BUTTON_RIGHT_CLICK || event->type != GDK_BUTTON_PRESS)
        return FALSE;

    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return FALSE;

    GtkWidget *menu = gtk_menu_new();
    QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), &iter);

    /* if Person or CM, give option to call */
    auto type = idx.data(static_cast<int>(Ring::Role::ObjectType));
    auto object = idx.data(static_cast<int>(Ring::Role::Object));
    if (type.isValid() && object.isValid()) {
        switch (type.value<Ring::ObjectType>()) {
            case Ring::ObjectType::Person:
            {
                /* possiblity for multiple numbers */
                auto cms = object.value<Person *>()->phoneNumbers();
                if (cms.size() == 1) {
                    auto item = gtk_menu_item_new_with_mnemonic(_("_Call"));
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
                    g_signal_connect(item,
                                     "activate",
                                     G_CALLBACK(call_contactmethod),
                                     cms.at(0));
                } else if (cms.size() > 1) {
                    auto call_item = gtk_menu_item_new_with_mnemonic(_("_Call"));
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), call_item);
                    auto call_menu = gtk_menu_new();
                    gtk_menu_item_set_submenu(GTK_MENU_ITEM(call_item), call_menu);
                    for (int i = 0; i < cms.size(); ++i) {
                        auto item = gtk_menu_item_new_with_label(cms.at(i)->uri().toUtf8().constData());
                        gtk_menu_shell_append(GTK_MENU_SHELL(call_menu), item);
                        g_signal_connect(item,
                                         "activate",
                                         G_CALLBACK(call_contactmethod),
                                         cms.at(i));
                    }
                }
            }
            break;
            case Ring::ObjectType::ContactMethod:
            {
                auto cm = object.value<ContactMethod *>();
                auto item = gtk_menu_item_new_with_mnemonic(_("_Call"));
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
                g_signal_connect(item,
                                 "activate",
                                 G_CALLBACK(call_contactmethod),
                                 cm);
            }
            break;
            case Ring::ObjectType::Call:
            case Ring::ObjectType::Media:
            // nothing to do for now
            break;
        }
    }

    /* copy name */
    QVariant name_var = idx.data(static_cast<int>(Ring::Role::Name));
    if (name_var.isValid()) {
        gchar *name = g_strdup_printf("%s", name_var.value<QString>().toUtf8().constData());
        GtkWidget *item = gtk_menu_item_new_with_mnemonic(_("_Copy name"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_object_set_data_full(G_OBJECT(item), COPY_DATA_KEY, name, (GDestroyNotify)g_free);
        g_signal_connect(item,
                         "activate",
                         G_CALLBACK(copy_contact_info),
                         NULL);
    }

    /* copy number(s) */
    if (type.isValid() && object.isValid()) {
        switch (type.value<Ring::ObjectType>()) {
            case Ring::ObjectType::Person:
            {
                /* possiblity for multiple numbers */
                auto cms = object.value<Person *>()->phoneNumbers();
                if (cms.size() == 1) {
                    gchar *number = g_strdup_printf("%s",cms.at(0)->uri().toUtf8().constData());
                    auto item = gtk_menu_item_new_with_mnemonic(_("_Copy number"));
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
                    g_object_set_data_full(G_OBJECT(item), COPY_DATA_KEY, number, (GDestroyNotify)g_free);
                    g_signal_connect(item,
                                     "activate",
                                     G_CALLBACK(copy_contact_info),
                                     NULL);
                } else if (cms.size() > 1) {
                    auto copy_item = gtk_menu_item_new_with_mnemonic(_("_Copy number"));
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), copy_item);
                    auto copy_menu = gtk_menu_new();
                    gtk_menu_item_set_submenu(GTK_MENU_ITEM(copy_item), copy_menu);
                    for (int i = 0; i < cms.size(); ++i) {
                        gchar *number = g_strdup_printf("%s",cms.at(i)->uri().toUtf8().constData());
                        auto item = gtk_menu_item_new_with_label(cms.at(i)->uri().toUtf8().constData());
                        gtk_menu_shell_append(GTK_MENU_SHELL(copy_menu), item);
                        g_object_set_data_full(G_OBJECT(item), COPY_DATA_KEY, number, (GDestroyNotify)g_free);
                        g_signal_connect(item,
                                         "activate",
                                         G_CALLBACK(copy_contact_info),
                                         NULL);
                    }
                }
            }
            break;
            case Ring::ObjectType::ContactMethod:
            case Ring::ObjectType::Call:
            case Ring::ObjectType::Media:
                QVariant number_var = idx.data(static_cast<int>(Ring::Role::Number));
                if (number_var.isValid()) {
                    gchar *number = g_strdup_printf("%s", number_var.value<QString>().toUtf8().constData());
                    GtkWidget *item = gtk_menu_item_new_with_mnemonic(_("_Copy number"));
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
                    g_object_set_data_full(G_OBJECT(item), COPY_DATA_KEY, number, (GDestroyNotify)g_free);
                    g_signal_connect(item,
                                     "activate",
                                     G_CALLBACK(copy_contact_info),
                                     NULL);
                }
            break;
        }
    }

     /* if the object is a CM, then give the option to add to contacts*/
     if (type.isValid() && object.isValid()) {
         if (type.value<Ring::ObjectType>() == Ring::ObjectType::ContactMethod) {
             /* get rectangle */
             auto path = gtk_tree_model_get_path(model, &iter);
             auto column = gtk_tree_view_get_column(treeview, 0);
             GdkRectangle rect;
             gtk_tree_view_get_cell_area(treeview, path, column, &rect);
             gtk_tree_view_convert_bin_window_to_widget_coords(treeview, rect.x, rect.y, &rect.x, &rect.y);
             gtk_tree_path_free(path);
             auto add_to = menu_item_add_to_contact(object.value<ContactMethod *>(), GTK_WIDGET(treeview), &rect);
             gtk_menu_shell_append(GTK_MENU_SHELL(menu), add_to);
         }
     }

    /* show menu */
    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);

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
scroll_to_selection(GtkTreeSelection *selection)
{
    GtkTreeModel *model = nullptr;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        auto path = gtk_tree_model_get_path(model, &iter);
        auto treeview = gtk_tree_selection_get_tree_view(selection);
        gtk_tree_view_scroll_to_cell(treeview, path, nullptr, FALSE, 0.0, 0.0);
    }
}

static void
recent_contacts_view_init(RecentContactsView *self)
{
    RecentContactsViewPrivate *priv = RECENT_CONTACTS_VIEW_GET_PRIVATE(self);

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(self), FALSE);
    /* no need to show the expander since it will always be expanded */
    gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(self), FALSE);
    /* disable default search, we will handle it ourselves via LRC;
     * otherwise the search steals input focus on key presses */
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(self), FALSE);

    GtkQSortFilterTreeModel *recent_model = gtk_q_sort_filter_tree_model_new(
        RecentModel::instance().peopleProxy(),
        1,
        Qt::DisplayRole, G_TYPE_STRING);

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

    /* call duration */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "xalign", 1.0, NULL);
    gtk_cell_area_box_pack_end(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_call_duration,
        NULL,
        NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(self), column);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_expand_all(GTK_TREE_VIEW(self));

    g_signal_connect(self, "button-press-event", G_CALLBACK(create_popup_menu), NULL);
    g_signal_connect(self, "row-activated", G_CALLBACK(activate_item), NULL);
    g_signal_connect(recent_model, "row-inserted", G_CALLBACK(expand_if_child), self);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));
    g_signal_connect(selection, "changed", G_CALLBACK(update_call_model_selection), NULL);
    g_signal_connect(selection, "changed", G_CALLBACK(scroll_to_selection), NULL);
    g_signal_connect_swapped(recent_model, "rows-reordered", G_CALLBACK(scroll_to_selection), selection);

    /* try to select the same call as the call model */
    priv->selection_updated = QObject::connect(
        CallModel::instance().selectionModel(),
        &QItemSelectionModel::currentChanged,
        [self, recent_model](const QModelIndex current, G_GNUC_UNUSED const QModelIndex & previous) {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));

            /* select the current */
            auto call = CallModel::instance().getCall(current);
            auto recent_idx = RecentModel::instance().getIndex(call);
            if (recent_idx.isValid()) {
                QModelIndex proxy_selection; // the index to select in the peopleProxy
                if (RecentModel::instance().rowCount(recent_idx.parent()) > 1) {
                    proxy_selection = RecentModel::instance().peopleProxy()->mapFromSource(recent_idx);
                } else {
                    // if single call, select the parent
                    proxy_selection = RecentModel::instance().peopleProxy()->mapFromSource(recent_idx.parent());
                }

                GtkTreeIter new_iter;
                if (gtk_q_sort_filter_tree_model_source_index_to_iter(recent_model, proxy_selection, &new_iter)) {
                    gtk_tree_selection_select_iter(selection, &new_iter);
                }
            }
        }
    );

    gtk_widget_show_all(GTK_WIDGET(self));
}

static void
recent_contacts_view_dispose(GObject *object)
{
    RecentContactsView *self = RECENT_CONTACTS_VIEW(object);
    RecentContactsViewPrivate *priv = RECENT_CONTACTS_VIEW_GET_PRIVATE(self);

    QObject::disconnect(priv->selection_updated);

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
