/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
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
#include "models/gtkqtreemodel.h"
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
#include <QtCore/QMimeData>
#include "utils/drawing.h"
#include <numbercategory.h>
#include "contactpopupmenu.h"

static constexpr const char* CALL_TARGET    = "CALL_TARGET";
static constexpr int         CALL_TARGET_ID = 0;

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
    GtkWidget *popup_menu;

    QMetaObject::Connection selection_updated;
    QMetaObject::Connection layout_changed;
};

G_DEFINE_TYPE_WITH_PRIVATE(RecentContactsView, recent_contacts_view, GTK_TYPE_TREE_VIEW);

#define RECENT_CONTACTS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RECENT_CONTACTS_VIEW_TYPE, RecentContactsViewPrivate))

static void
update_selection(GtkTreeSelection *selection, G_GNUC_UNUSED gpointer user_data)
{
    auto current_proxy = get_index_from_selection(selection);
    auto current = RecentModel::instance().peopleProxy()->mapToSource(current_proxy);

    RecentModel::instance().selectionModel()->setCurrentIndex(current, QItemSelectionModel::ClearAndSelect);

    // update the CallModel selection since we rely on the UserActionModel
    if (auto call_to_select = RecentModel::instance().getActiveCall(current)) {
        CallModel::instance().selectCall(call_to_select);
    } else {
        CallModel::instance().selectionModel()->clearCurrentIndex();
    }
}

static void
render_contact_photo(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);

    std::shared_ptr<GdkPixbuf> image;
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
        } else if (auto call = object.value<Call *>()) {
            if (call->type() == Call::Type::CONFERENCE) {
                var_photo = GlobalInstances::pixmapManipulator().callPhoto(call, QSize(50, 50), false);
            }
        }
        if (var_photo.isValid()) {
            std::shared_ptr<GdkPixbuf> photo = var_photo.value<std::shared_ptr<GdkPixbuf>>();

            auto unread = idx.data(static_cast<int>(Ring::Role::UnreadTextMessageCount));

            image.reset(ring_draw_unread_messages(photo.get(), unread.toInt()), g_object_unref);
        } else {
            // set the width of the cell rendered to the with of the photo
            // so that the other renderers are shifted to the right
            g_object_set(G_OBJECT(cell), "width", 50, NULL);
        }
    }

    g_object_set(G_OBJECT(cell), "pixbuf", image.get(), NULL);
}

static void
render_name_and_info(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     GtkTreeView *treeview)
{
    gchar *text = NULL;

    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);

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
                    text = g_markup_printf_escaped("%s\n<span size=\"smaller\">%s</span>",
                                                   name.toUtf8().constData(),
                                                   status.toUtf8().constData());
                } else {
                    text = g_markup_printf_escaped("%s\n<span size=\"smaller\" color=\"gray\">%s</span>",
                                                   name.toUtf8().constData(),
                                                   status.toUtf8().constData());
                }
            }
            break;
            case Ring::ObjectType::Call:
            {
                // check if it is a conference
                auto idx_source = RecentModel::instance().peopleProxy()->mapToSource(idx);
                auto is_conference = RecentModel::instance().isConference(idx_source);

                if (is_conference) {
                    auto var_name = idx.data(static_cast<int>(Ring::Role::Name));
                    text = g_markup_escape_text(var_name.value<QString>().toUtf8().constData(), -1);
                } else {
                    auto parent_source = RecentModel::instance().peopleProxy()->mapToSource(idx.parent());
                    if (RecentModel::instance().isConference(parent_source)) {
                        // part of conference, simply display the name
                        auto var_name = idx.data(static_cast<int>(Ring::Role::Name));

                        /* we want the color of the name text to be the default color if this iter is
                         * selected so that the treeview is able to invert it against the selection color */
                        if (is_selected) {
                            text = g_markup_printf_escaped("<span size=\"smaller\">%s</span>",
                                                           var_name.value<QString>().toUtf8().constData());
                        } else {
                            text = g_markup_printf_escaped("<span size=\"smaller\" color=\"gray\">%s</span>",
                                                           var_name.value<QString>().toUtf8().constData());
                        }
                    } else {
                        // just a call, so display the state
                        auto var_status = idx.data(static_cast<int>(Ring::Role::FormattedState));

                        QString status;

                        if (var_status.isValid())
                            status += var_status.value<QString>();

                        /* we want the color of the status text to be the default color if this iter is
                         * selected so that the treeview is able to invert it against the selection color */
                        if (is_selected) {
                            text = g_markup_printf_escaped("<span size=\"smaller\">%s</span>",
                                                           status.toUtf8().constData());
                        } else {
                            text = g_markup_printf_escaped("<span size=\"smaller\" color=\"gray\">%s</span>",
                                                           status.toUtf8().constData());
                        }
                    }
                }
            }
            break;
            case Ring::ObjectType::Media:
            case Ring::ObjectType::Certificate:
            case Ring::ObjectType::ContactRequest:
            // nothing to do for now
            case Ring::ObjectType::COUNT__:
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

    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);

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
                    text = g_markup_escape_text(duration.value<QString>().toUtf8().constData(), -1);
                }
            }
            break;
            case Ring::ObjectType::Call:
            {
                // do not display the duration if the call is part of a conference
                auto parent_source = RecentModel::instance().peopleProxy()->mapToSource(idx.parent());
                auto in_conference = RecentModel::instance().isConference(parent_source);

                if (!in_conference) {
                    auto duration = idx.data(static_cast<int>(Ring::Role::Length));

                    if (duration.isValid())
                        text = g_markup_escape_text(duration.value<QString>().toUtf8().constData(), -1);
                }
            }
            break;
            case Ring::ObjectType::Media:
            case Ring::ObjectType::Certificate:
            case Ring::ObjectType::ContactRequest:
            // nothing to do for now
            case Ring::ObjectType::COUNT__:
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
        QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
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
                case Ring::ObjectType::Certificate:
                case Ring::ObjectType::ContactRequest:
                // nothing to do for now
                case Ring::ObjectType::COUNT__:
                break;
            }
        }
    }
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
on_drag_data_get(GtkWidget        *treeview,
                 G_GNUC_UNUSED GdkDragContext *context,
                 GtkSelectionData *data,
                 G_GNUC_UNUSED guint info,
                 G_GNUC_UNUSED guint time,
                 G_GNUC_UNUSED gpointer user_data)
{
    g_return_if_fail(IS_RECENT_CONTACTS_VIEW(treeview));

    /* we always drag the selected row */
    auto selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        auto path_str = gtk_tree_model_get_string_from_iter(model, &iter);

        gtk_selection_data_set(data,
                               gdk_atom_intern_static_string(CALL_TARGET),
                               8, /* bytes */
                               (guchar *)path_str,
                               strlen(path_str) + 1);

        g_free(path_str);
    } else {
        g_warning("drag selection not valid");
    }
}

static gboolean
on_drag_drop(GtkWidget      *treeview,
             GdkDragContext *context,
             gint            x,
             gint            y,
             guint           time,
             G_GNUC_UNUSED gpointer user_data)
{
    g_return_val_if_fail(IS_RECENT_CONTACTS_VIEW(treeview), FALSE);

    GtkTreePath *path = NULL;
    GtkTreeViewDropPosition drop_pos;

    if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(treeview),
                                          x, y, &path, &drop_pos)) {

        GdkAtom target_type = gtk_drag_dest_find_target(treeview, context, NULL);

        if (target_type != GDK_NONE) {
            g_debug("can drop");
            gtk_drag_get_data(treeview, context, target_type, time);
            return TRUE;
        }

        gtk_tree_path_free(path);
    }

    return FALSE;
}

static gboolean
on_drag_motion(GtkWidget      *treeview,
               GdkDragContext *context,
               gint            x,
               gint            y,
               guint           time,
               G_GNUC_UNUSED gpointer user_data)
{
    g_return_val_if_fail(IS_RECENT_CONTACTS_VIEW(treeview), FALSE);

    GtkTreePath *path = NULL;
    GtkTreeViewDropPosition drop_pos;

    if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(treeview),
                                          x, y, &path, &drop_pos)) {
        // we only want to drop on a row, not before or after
        if (drop_pos == GTK_TREE_VIEW_DROP_BEFORE) {
            gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(treeview), path, GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);
        } else if (drop_pos == GTK_TREE_VIEW_DROP_AFTER) {
            gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(treeview), path, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
        }
        gdk_drag_status(context, gdk_drag_context_get_suggested_action(context), time);
        return TRUE;
    } else {
        // not a row in the treeview, so we cannot drop
        return FALSE;
    }
}

static void
on_drag_data_received(GtkWidget        *treeview,
                      GdkDragContext   *context,
                      gint              x,
                      gint              y,
                      GtkSelectionData *data,
                      G_GNUC_UNUSED guint info,
                      guint             time,
                      G_GNUC_UNUSED gpointer user_data)
{
    g_return_if_fail(IS_RECENT_CONTACTS_VIEW(treeview));

    gboolean success = FALSE;

    /* get the source and destination calls */
    auto path_str_source = (gchar *)gtk_selection_data_get_data(data);
    auto type = gtk_selection_data_get_data_type(data);
    g_debug("data type: %s", gdk_atom_name(type));
    if (path_str_source && strlen(path_str_source) > 0) {
        g_debug("source path: %s", path_str_source);

        /* get the destination path */
        GtkTreePath *dest_path = NULL;
        if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(treeview), x, y, &dest_path, NULL)) {
            auto model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

            GtkTreeIter source;
            gtk_tree_model_get_iter_from_string(model, &source, path_str_source);
            auto idx_source_proxy = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &source);

            GtkTreeIter dest;
            gtk_tree_model_get_iter(model, &dest, dest_path);
            auto idx_dest_proxy = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &dest);

            // get call objects and indeces from RecentModel indeces being drag and dropped
            auto idx_source = RecentModel::instance().peopleProxy()->mapToSource(idx_source_proxy);
            auto idx_dest = RecentModel::instance().peopleProxy()->mapToSource(idx_dest_proxy);
            auto call_source = RecentModel::instance().getActiveCall(idx_source);
            auto call_dest = RecentModel::instance().getActiveCall(idx_dest);
            auto idx_call_source = CallModel::instance().getIndex(call_source);
            auto idx_call_dest = CallModel::instance().getIndex(call_dest);

            if (idx_call_source.isValid() && idx_call_dest.isValid()) {
                QModelIndexList source_list;
                source_list << idx_call_source;
                auto mimeData = CallModel::instance().mimeData(source_list);
                auto action = Call::DropAction::Conference;
                mimeData->setProperty("dropAction", action);

                if (CallModel::instance().dropMimeData(mimeData, Qt::MoveAction, idx_call_dest.row(), idx_call_dest.column(), idx_call_dest.parent())) {
                    success = TRUE;
                } else {
                    g_warning("could not drop mime data");
                }
            } else {
                g_warning("source or dest call not valid");
            }

            gtk_tree_path_free(dest_path);
        }
    }

    gtk_drag_finish(context, success, FALSE, time);
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

    GtkQTreeModel *recent_model = gtk_q_tree_model_new(
        RecentModel::instance().peopleProxy(),
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

    g_signal_connect(self, "row-activated", G_CALLBACK(activate_item), NULL);
    g_signal_connect(recent_model, "row-inserted", G_CALLBACK(expand_if_child), self);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));
    g_signal_connect(selection, "changed", G_CALLBACK(update_selection), NULL);
    g_signal_connect(selection, "changed", G_CALLBACK(scroll_to_selection), NULL);
    g_signal_connect_swapped(recent_model, "rows-reordered", G_CALLBACK(scroll_to_selection), selection);

    /* update the selection based on the RecentModel */
    priv->selection_updated = QObject::connect(
        RecentModel::instance().selectionModel(),
        &QItemSelectionModel::currentChanged,
        [self, recent_model](const QModelIndex current, G_GNUC_UNUSED const QModelIndex & previous) {
            auto selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));

            auto current_proxy = RecentModel::instance().peopleProxy()->mapFromSource(current);

            if (current.isValid()) {
                /* select the current */
                GtkTreeIter new_iter;
                if (gtk_q_tree_model_source_index_to_iter(recent_model, current_proxy, &new_iter)) {
                    gtk_tree_selection_select_iter(selection, &new_iter);
                }
            } else {
                gtk_tree_selection_unselect_all(selection);
            }
        }
    );

    /* we may need to update the selection when the layout changes */
    priv->layout_changed = QObject::connect(
        RecentModel::instance().peopleProxy(),
        &QAbstractItemModel::layoutChanged,
        [self, recent_model]() {
            auto idx = RecentModel::instance().selectionModel()->currentIndex();
            auto selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));

            auto idx_proxy = RecentModel::instance().peopleProxy()->mapFromSource(idx);

            if (idx_proxy.isValid()) {
                /* select the current */
                GtkTreeIter iter;
                if (gtk_q_tree_model_source_index_to_iter(recent_model, idx_proxy, &iter)) {
                    gtk_tree_selection_select_iter(selection, &iter);
                }
            } else {
                gtk_tree_selection_unselect_all(selection);
            }
        }
    );

    /* drag and drop */
    static GtkTargetEntry targetentries[] = {
        { (gchar *)CALL_TARGET, GTK_TARGET_SAME_WIDGET, CALL_TARGET_ID },
    };

    gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(self),
        GDK_BUTTON1_MASK, targetentries, 1, (GdkDragAction)(GDK_ACTION_DEFAULT | GDK_ACTION_MOVE));

    gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(self),
        targetentries, 1, GDK_ACTION_DEFAULT);

    g_signal_connect(self, "drag-data-get", G_CALLBACK(on_drag_data_get), nullptr);
    g_signal_connect(self, "drag-drop", G_CALLBACK(on_drag_drop), nullptr);
    g_signal_connect(self, "drag-motion", G_CALLBACK(on_drag_motion), nullptr);
    g_signal_connect(self, "drag_data_received", G_CALLBACK(on_drag_data_received), nullptr);

    /* init popup menu */
    priv->popup_menu = contact_popup_menu_new(GTK_TREE_VIEW(self));
    g_signal_connect_swapped(self, "button-press-event", G_CALLBACK(contact_popup_menu_show), priv->popup_menu);

    gtk_widget_show_all(GTK_WIDGET(self));
}

static void
recent_contacts_view_dispose(GObject *object)
{
    RecentContactsView *self = RECENT_CONTACTS_VIEW(object);
    RecentContactsViewPrivate *priv = RECENT_CONTACTS_VIEW_GET_PRIVATE(self);

    QObject::disconnect(priv->selection_updated);
    QObject::disconnect(priv->layout_changed);
    gtk_widget_destroy(priv->popup_menu);

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
