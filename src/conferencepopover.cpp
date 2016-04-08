    /*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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

#include "conferencepopover.h"

#include <call.h>
#include <glib/gi18n.h>
#include "models/gtkqsortfiltertreemodel.h"
#include "utils/calling.h"
#include <recentmodel.h>
#include <globalinstances.h>
#include "utils/drawing.h"
#include "native/pixbufmanipulator.h"
#include <contactmethod.h>
#include <callmodel.h>

constexpr static int PHOTO_SIZE {40};

struct _ConferencePopover
{
#if GTK_CHECK_VERSION(3,12,0)
    GtkPopover parent;
#else
    GtkWindow parent;
#endif
};

struct _ConferencePopoverClass
{
#if GTK_CHECK_VERSION(3,12,0)
    GtkPopoverClass parent_class;
#else
    GtkWindowClass parent_class;
#endif
};

typedef struct _ConferencePopoverPrivate ConferencePopoverPrivate;

struct _ConferencePopoverPrivate
{
    Call *call;
};

#if GTK_CHECK_VERSION(3,12,0)
    G_DEFINE_TYPE_WITH_PRIVATE(ConferencePopover, conference_popover, GTK_TYPE_POPOVER);
#else
    G_DEFINE_TYPE_WITH_PRIVATE(ConferencePopover, conference_popover, GTK_TYPE_WINDOW);
#endif


#define CONFERENCE_POPOVER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CONFERENCE_POPOVER_TYPE, ConferencePopoverPrivate))

#if !GTK_CHECK_VERSION(3,12,0)
static gboolean
conference_popover_button_release(GtkWidget *self, GdkEventButton *event)
{
    auto child = gtk_bin_get_child(GTK_BIN(self));

    auto event_widget = gtk_get_event_widget((GdkEvent *) event);

    GtkAllocation child_alloc;

    gtk_widget_get_allocation(child, &child_alloc);

    if (event->x < child_alloc.x ||
        event->x > child_alloc.x + child_alloc.width ||
        event->y < child_alloc.y ||
        event->y > child_alloc.y + child_alloc.height)
        gtk_widget_destroy(self);
    else if (!gtk_widget_is_ancestor(event_widget, self))
        gtk_widget_destroy(self);

    return GDK_EVENT_PROPAGATE;
}

static gboolean
conference_popover_key_press(GtkWidget *self, GdkEventKey *event)
{
    if (event->keyval == GDK_KEY_Escape) {
        gtk_widget_destroy(self);
        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}
#endif

static void
render_contact_photo(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), iter);

    // check if it is the current call
    auto idx_model = RecentModel::instance().peopleProxy()->mapToSource(idx);
    if (RecentModel::instance().selectionModel()->currentIndex() == idx_model) {
        gtk_cell_renderer_set_sensitive(cell, FALSE);
    } else {
        gtk_cell_renderer_set_sensitive(cell, TRUE);
    }

    std::shared_ptr<GdkPixbuf> image;
    /* we only want to render a photo for the top nodes: Person, ContactMethod (, later Conference) */
    QVariant object = idx.data(static_cast<int>(Ring::Role::Object));
    if (idx.isValid() && object.isValid()) {
        QVariant var_photo;
        if (auto person = object.value<Person *>()) {
            var_photo = GlobalInstances::pixmapManipulator().contactPhoto(person, QSize(PHOTO_SIZE, PHOTO_SIZE), false);
        } else if (auto cm = object.value<ContactMethod *>()) {
            /* get photo, note that this should in all cases be the fallback avatar, since there
             * shouldn't be a person associated with this contact method */
            var_photo = GlobalInstances::pixmapManipulator().callPhoto(cm, QSize(PHOTO_SIZE, PHOTO_SIZE), false);
        } else if (auto call = object.value<Call *>()) {
            if (call->type() == Call::Type::CONFERENCE) {
                var_photo = GlobalInstances::pixmapManipulator().callPhoto(call, QSize(PHOTO_SIZE, PHOTO_SIZE), false);
            }
        }
        if (var_photo.isValid()) {
            std::shared_ptr<GdkPixbuf> photo = var_photo.value<std::shared_ptr<GdkPixbuf>>();

            auto unread = idx.data(static_cast<int>(Ring::Role::UnreadTextMessageCount));

            image.reset(ring_draw_unread_messages(photo.get(), unread.toInt()), g_object_unref);
        } else {
            // set the width of the cell rendered to the with of the photo
            // so that the other renderers are shifted to the right
            g_object_set(G_OBJECT(cell), "width", PHOTO_SIZE, NULL);
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

    QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), iter);

    // check if it is the current call
    auto idx_model = RecentModel::instance().peopleProxy()->mapToSource(idx);
    if (RecentModel::instance().selectionModel()->currentIndex() == idx_model) {
        gtk_cell_renderer_set_sensitive(cell, FALSE);
    } else {
        gtk_cell_renderer_set_sensitive(cell, TRUE);
    }

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

                QString name;

                if (var_name.isValid())
                    name = var_name.value<QString>();

                /* we want the color of the status text to be the default color if this iter is
                 * selected so that the treeview is able to invert it against the selection color */
                if (is_selected) {
                    text = g_strdup_printf("%s", name.toUtf8().constData());
                } else {
                    text = g_strdup_printf("%s", name.toUtf8().constData());
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
                    text = g_markup_printf_escaped("%s", var_name.value<QString>().toUtf8().constData());
                } else {
                    auto parent_source = RecentModel::instance().peopleProxy()->mapToSource(idx.parent());
                    if (RecentModel::instance().isConference(parent_source)) {
                        // part of conference, simply display the name
                        auto var_name = idx.data(static_cast<int>(Ring::Role::Name));

                        /* we want the color of the name text to be the default color if this iter is
                         * selected so that the treeview is able to invert it against the selection color */
                        if (is_selected)
                            text = g_strdup_printf("<span size=\"smaller\">%s</span>", var_name.value<QString>().toUtf8().constData());
                        else
                            text = g_strdup_printf("<span size=\"smaller\" color=\"gray\">%s</span>", var_name.value<QString>().toUtf8().constData());
                    } else {
                        // just a call, so display the state
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
                }
            }
            break;
            case Ring::ObjectType::Media:
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
              ConferencePopover *self)
{
    ConferencePopoverPrivate *priv = CONFERENCE_POPOVER_GET_PRIVATE(self);

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

                            // place_new_call(last_used_cm);
                            auto new_call = CallModel::instance().dialingCall(last_used_cm);

                            auto state_changed_conn = std::make_shared<QMetaObject::Connection>();
                            auto current_call = priv->call;
                            *state_changed_conn = QObject::connect(
                                new_call,
                                &Call::stateChanged,
                                [current_call, new_call, state_changed_conn] (Call::State newState, Call::State previousState)
                                    {
                                        QObject::disconnect(*state_changed_conn);
                                        CallModel::instance().createJoinOrMergeConferenceFromCall(current_call, new_call);
                                    }
                                );

                            new_call->performAction(Call::Action::ACCEPT);
                        }
                    }
                }
                break;
                case Ring::ObjectType::ContactMethod:
                {
                    // call the contact method
                    auto cm = idx.data(static_cast<int>(Ring::Role::Object));
                    if (cm.isValid()) {
                        // place_new_call(cm.value<ContactMethod *>());
                        auto new_call = CallModel::instance().dialingCall(cm.value<ContactMethod *>());
                        auto state_changed_conn = std::make_shared<QMetaObject::Connection>();
                        auto current_call = priv->call;
                        *state_changed_conn = QObject::connect(
                            new_call,
                            &Call::stateChanged,
                            [current_call, new_call, state_changed_conn] (Call::State newState, Call::State previousState)
                                {
                                    QObject::disconnect(*state_changed_conn);
                                    CallModel::instance().createJoinOrMergeConferenceFromCall(current_call, new_call);
                                }
                            );

                        new_call->performAction(Call::Action::ACCEPT);
                    }
                }
                break;
                case Ring::ObjectType::Call:
                case Ring::ObjectType::Media:
                // nothing to do for now
                case Ring::ObjectType::COUNT__:
                break;
            }
        }
    }
}

static void
conference_popover_init(ConferencePopover *self)
{
#if GTK_CHECK_VERSION(3,12,0)
    /* for now, destroy the popover on close, as we will construct a new one
     * each time we need it */
    g_signal_connect(self, "closed", G_CALLBACK(gtk_widget_destroy), NULL);
#else
    /* destroy the window on ESC, or when the user clicks outside of it */
    g_signal_connect(self, "button_release_event", G_CALLBACK(conference_popover_button_release), NULL);
    g_signal_connect(self, "key_press_event", G_CALLBACK(conference_popover_key_press), NULL);
#endif

    auto box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(self), box);

    auto label = gtk_label_new(C_("Text at the top of the conference creation popup", "Select a call or contact to add to this conversation"));
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, TRUE, 0);

    auto scroll_window = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll_window), 300);
    gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scroll_window), 300);
    gtk_box_pack_start(GTK_BOX(box), scroll_window, TRUE, TRUE, 0);

    auto tree_view = gtk_tree_view_new();
    gtk_container_add(GTK_CONTAINER(scroll_window), tree_view);

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), FALSE);
    /* no need to show the expander since it will always be expanded */
    gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(tree_view), FALSE);
    /* disable default search, we will handle it ourselves via LRC;
     * otherwise the search steals input focus on key presses */
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree_view), FALSE);

    auto recent_model = gtk_q_sort_filter_tree_model_new(
        RecentModel::instance().peopleProxy(),
        1,
        Qt::DisplayRole, G_TYPE_STRING);

    gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view),
                            GTK_TREE_MODEL(recent_model));

    /* photo and name/contact method column */
    auto area = gtk_cell_area_box_new();
    auto column = gtk_tree_view_column_new_with_area(area);

    /* photo renderer */
    auto renderer = gtk_cell_renderer_pixbuf_new();
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
        tree_view,
        NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_expand_all(GTK_TREE_VIEW(tree_view));

    g_signal_connect(tree_view, "row-activated", G_CALLBACK(activate_item), self);

    gtk_widget_show_all(box);
}

static void
conference_popover_dispose(GObject *object)
{
    G_OBJECT_CLASS(conference_popover_parent_class)->dispose(object);
}

static void
conference_popover_finalize(GObject *object)
{
    G_OBJECT_CLASS(conference_popover_parent_class)->finalize(object);
}
static void
conference_popover_class_init(ConferencePopoverClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = conference_popover_finalize;
    G_OBJECT_CLASS(klass)->dispose = conference_popover_dispose;
}

// static void
// construct_edit_contact_view(ConferencePopover *self, Person *p)
// {
//     g_return_if_fail(IS_CONFERENCE_POPOVER(self));
//     ConferencePopoverPrivate *priv = CONFERENCE_POPOVER_GET_PRIVATE(self);
//
//     priv->editcontactview = edit_contact_view_new(priv->cm, p);
//     g_object_add_weak_pointer(G_OBJECT(priv->editcontactview), (gpointer *)&priv->editcontactview);
//
//     gtk_container_remove(GTK_CONTAINER(self), priv->choosecontactview);
//     gtk_container_add(GTK_CONTAINER(self), priv->editcontactview);
//
// #if !GTK_CHECK_VERSION(3,12,0)
//     /* resize the window to shrink to the new view */
//     gtk_window_resize(GTK_WINDOW(self), 1, 1);
// #endif
//
//     /* destroy this popover when the contact is saved */
//     g_signal_connect_swapped(priv->editcontactview, "person-saved", G_CALLBACK(gtk_widget_destroy), self);
// }
//
// static void
// new_person_clicked(ConferencePopover *self)
// {
//     g_return_if_fail(IS_CONFERENCE_POPOVER(self));
//     construct_edit_contact_view(self, NULL);
// }
//
// static void
// person_selected(ConferencePopover *self, Person *p)
// {
//     g_return_if_fail(IS_CONFERENCE_POPOVER(self));
//     construct_edit_contact_view(self, p);
// }

/**
 * For gtk+ >= 3.12 this will create a GtkPopover pointing to the parent and if
 * given, the GdkRectangle. Otherwise, this will create an undecorated GtkWindow
 * which will be centered on the toplevel window of the given parent.
 * This is to ensure cmpatibility with gtk+3.10.
 */
GtkWidget *
conference_popover_new(Call *call, GtkWidget *parent,
#if !GTK_CHECK_VERSION(3,12,0)
                    G_GNUC_UNUSED
#endif
                    GdkRectangle *rect)
{
    g_return_val_if_fail(call, NULL);

#if GTK_CHECK_VERSION(3,12,0)
    gpointer self = g_object_new(CONFERENCE_POPOVER_TYPE,
                                 "relative-to", parent,
                                 "position", GTK_POS_TOP,
                                 NULL);

    if (rect)
        gtk_popover_set_pointing_to(GTK_POPOVER(self), rect);
#else
    /* get the toplevel parent and try to center on it */
    if (parent && GTK_IS_WIDGET(parent)) {
        parent = gtk_widget_get_toplevel(GTK_WIDGET(parent));
        if (!gtk_widget_is_toplevel(parent)) {
            parent = NULL;
            g_debug("could not get top level parent");
        }
    }

    gpointer self = g_object_new(CONFERENCE_POPOVER_TYPE,
                                 "modal", TRUE,
                                 "transient-for", parent,
                                 "window-position", GTK_WIN_POS_CENTER_ON_PARENT,
                                 "decorated", FALSE,
                                 "resizable", FALSE,
                                 NULL);
#endif

    ConferencePopoverPrivate *priv = CONFERENCE_POPOVER_GET_PRIVATE(self);
    priv->call = call;

    // priv->cm = cm;
    //
    // priv->choosecontactview = choose_contact_view_new(cm);
    // gtk_container_add(GTK_CONTAINER(self), priv->choosecontactview);
    // g_object_add_weak_pointer(G_OBJECT(priv->choosecontactview), (gpointer *)&priv->choosecontactview);
    //
    // g_signal_connect_swapped(priv->choosecontactview, "new-person-clicked", G_CALLBACK(new_person_clicked), self);
    // g_signal_connect_swapped(priv->choosecontactview, "person-selected", G_CALLBACK(person_selected), self);

    return (GtkWidget *)self;
}
