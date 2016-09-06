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

#include "ringmainwindow.h"

// system
#include <string.h>
#include <memory>

// GTK+ related
#include <glib/gi18n.h>

// Qt
#include <QtCore/QItemSelectionModel>
#include <QtCore/QSortFilterProxyModel>

// LRC
#include <callmodel.h>
#include <call.h>
#include <contactmethod.h>
#include <accountmodel.h>
#include <codecmodel.h>
#include <video/previewmanager.h>
#include <personmodel.h>
#include <globalinstances.h>
#include <numbercompletionmodel.h>
#include <categorizedcontactmodel.h>
#include <recentmodel.h>
#include <profilemodel.h>
#include <profile.h>

// Ring client
#include "models/gtkqtreemodel.h"
#include "incomingcallview.h"
#include "currentcallview.h"
#include "models/gtkqsortfiltertreemodel.h"
#include "accountview.h"
#include "dialogs.h"
#include "mediasettingsview.h"
#include "utils/drawing.h"
#include "native/pixbufmanipulator.h"
#include "models/activeitemproxymodel.h"
#include "utils/calling.h"
#include "contactsview.h"
#include "historyview.h"
#include "utils/models.h"
#include "generalsettingsview.h"
#include "utils/accounts.h"
#include "ringwelcomeview.h"
#include "accountmigrationview.h"
#include "accountcreationwizard.h"
#include "recentcontactsview.h"
#include "chatview.h"
#include "avatarmanipulation.h"

static constexpr const char* CALL_VIEW_NAME             = "calls";
static constexpr const char* ACCOUNT_CREATION_WIZARD_VIEW_NAME = "account-creation-wizard";
static constexpr const char* ACCOUNT_MIGRATION_VIEW_NAME       = "account-migration-view";
static constexpr const char* GENERAL_SETTINGS_VIEW_NAME = "general";
static constexpr const char* AUDIO_SETTINGS_VIEW_NAME   = "audio";
static constexpr const char* MEDIA_SETTINGS_VIEW_NAME   = "media";
static constexpr const char* ACCOUNT_SETTINGS_VIEW_NAME = "accounts";
static constexpr const char* DEFAULT_VIEW_NAME          = "welcome";
static constexpr const char* VIEW_CONTACTS              = "contacts";
static constexpr const char* VIEW_HISTORY               = "history";
static constexpr const char* VIEW_PRESENCE              = "presence";

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
    GtkWidget *ring_menu;
    GtkWidget *image_ring;
    GtkWidget *ring_settings;
    GtkWidget *image_settings;
    GtkWidget *hbox_settings;
    GtkWidget *scrolled_window_smartview;
    GtkWidget *scrolled_window_contacts;
    GtkWidget *scrolled_window_history;
    GtkWidget *vbox_left_pane;
    GtkWidget *search_entry;
    GtkWidget *stack_main_view;
    GtkWidget *vbox_call_view;
    GtkWidget *frame_call;
    GtkWidget *welcome_view;
    GtkWidget *button_placecall;
    GtkWidget *account_settings_view;
    GtkWidget *media_settings_view;
    GtkWidget *general_settings_view;
    GtkWidget *last_settings_view;
    GtkWidget *radiobutton_general_settings;
    GtkWidget *radiobutton_media_settings;
    GtkWidget *radiobutton_account_settings;

    QMetaObject::Connection selection_updated;

    gboolean   show_settings;

    /* allocd qmodels */
    NumberCompletionModel *q_completion_model;

    /* fullscreen */
    gboolean is_fullscreen;
};

G_DEFINE_TYPE_WITH_PRIVATE(RingMainWindow, ring_main_window, GTK_TYPE_APPLICATION_WINDOW);

#define RING_MAIN_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RING_MAIN_WINDOW_TYPE, RingMainWindowPrivate))

static void
enter_full_screen(RingMainWindow *self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    if (!priv->is_fullscreen) {
        gtk_widget_hide(priv->vbox_left_pane);
        gtk_window_fullscreen(GTK_WINDOW(self));
        priv->is_fullscreen = TRUE;
    }
}

static void
leave_full_screen(RingMainWindow *self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    if (priv->is_fullscreen) {
        gtk_widget_show(priv->vbox_left_pane);
        gtk_window_unfullscreen(GTK_WINDOW(self));
        priv->is_fullscreen = FALSE;
    }
}

static void
video_double_clicked(G_GNUC_UNUSED CurrentCallView *view, RingMainWindow *self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    if (priv->is_fullscreen) {
        leave_full_screen(self);
    } else {
        enter_full_screen(self);
    }
}

static void
hide_view_clicked(G_GNUC_UNUSED GtkWidget *view, RingMainWindow *self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));

    /* clear selection */
    RecentModel::instance().selectionModel()->clear();
}

/**
 * This takes the RecentModel index as the argument and displays the corresponding view:
 * - incoming call view
 * - current call view
 * - chat view
 * - welcome view (if no index is selected)
 */
static void
selection_changed(const QModelIndex& recent_idx, RingMainWindow *win)
{
    // g_debug("selection changed");
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(win));

    /* if we're showing the settings, then nothing needs to be done as the call
       view is not shown */
    if (priv->show_settings) return;

    /* get the current visible stack child */
    GtkWidget *old_call_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));

    /* make sure we leave full screen, since the call selection is changing */
    leave_full_screen(win);

    /* check which object type is selected */
    auto type = recent_idx.data(static_cast<int>(Ring::Role::ObjectType)).value<Ring::ObjectType>();
    auto object = recent_idx.data(static_cast<int>(Ring::Role::Object));
    /* try to get the call model index, in case its a call, since we're still using the CallModel as well */
    auto call_idx = CallModel::instance().getIndex(RecentModel::instance().getActiveCall(recent_idx));

    /* we prioritize showing the call view */
    if (call_idx.isValid()) {
        /* show the call view */
        QVariant state =  call_idx.data(static_cast<int>(Call::Role::LifeCycleState));
        GtkWidget *new_call_view = NULL;

        switch(state.value<Call::LifeCycleState>()) {
            case Call::LifeCycleState::CREATION:
            case Call::LifeCycleState::INITIALIZATION:
            case Call::LifeCycleState::FINISHED:
                new_call_view = incoming_call_view_new();
                incoming_call_view_set_call_info(INCOMING_CALL_VIEW(new_call_view), call_idx);
                break;
            case Call::LifeCycleState::PROGRESS:
                new_call_view = current_call_view_new();
                g_signal_connect(new_call_view, "video-double-clicked", G_CALLBACK(video_double_clicked), win);
                current_call_view_set_call_info(CURRENT_CALL_VIEW(new_call_view), call_idx);
                break;
            case Call::LifeCycleState::COUNT__:
                g_warning("LifeCycleState should never be COUNT");
                break;
        }

        gtk_container_remove(GTK_CONTAINER(priv->frame_call), old_call_view);
        gtk_container_add(GTK_CONTAINER(priv->frame_call), new_call_view);
        gtk_widget_show(new_call_view);
    } else if (type == Ring::ObjectType::Person && object.isValid()) {
        /* show chat view constructed from Person object */
        auto new_chat_view = chat_view_new_person(object.value<Person *>());
        g_signal_connect(new_chat_view, "hide-view-clicked", G_CALLBACK(hide_view_clicked), win);
        gtk_container_remove(GTK_CONTAINER(priv->frame_call), old_call_view);
        gtk_container_add(GTK_CONTAINER(priv->frame_call), new_chat_view);
        gtk_widget_show(new_chat_view);
    } else if (type == Ring::ObjectType::ContactMethod && object.isValid()) {
        /* show chat view constructed from CM */
        auto new_chat_view = chat_view_new_cm(object.value<ContactMethod *>());
        g_signal_connect(new_chat_view, "hide-view-clicked", G_CALLBACK(hide_view_clicked), win);
        gtk_container_remove(GTK_CONTAINER(priv->frame_call), old_call_view);
        gtk_container_add(GTK_CONTAINER(priv->frame_call), new_chat_view);
        gtk_widget_show(new_chat_view);
    } else {
        /* nothing selected that we can display, show the welcome view */
        gtk_container_remove(GTK_CONTAINER(priv->frame_call), old_call_view);
        gtk_container_add(GTK_CONTAINER(priv->frame_call), priv->welcome_view);
    }
}

static gboolean
selected_item_changed(RingMainWindow *win)
{
    // g_debug("item changed");
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(win));

    /* if we're showing the settings, then nothing needs to be done as the call
       view is not shown */
    if (priv->show_settings) return G_SOURCE_REMOVE;

    auto idx_selected = RecentModel::instance().selectionModel()->currentIndex();

    /* we prioritize showing the call view; but if the call is over we go back to showing the chat view */
    if(auto call = RecentModel::instance().getActiveCall(idx_selected)) {
        /* check if we need to change the view */
        auto current_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));
        auto state = call->lifeCycleState();

        /* check what the current state is vs what is displayed */
        switch(state) {
            case Call::LifeCycleState::CREATION:
            case Call::LifeCycleState::FINISHED:
            /* go back to incoming call view;
             * it will show that the call failed and offer to hang it up */
            case Call::LifeCycleState::INITIALIZATION:
                {
                    /* show the incoming call view */
                    if (!IS_INCOMING_CALL_VIEW(current_view)) {
                        auto new_view = incoming_call_view_new();
                        incoming_call_view_set_call_info(INCOMING_CALL_VIEW(new_view), CallModel::instance().getIndex(call));
                        gtk_container_remove(GTK_CONTAINER(priv->frame_call), current_view);
                        gtk_container_add(GTK_CONTAINER(priv->frame_call), new_view);
                        gtk_widget_show(new_view);
                    }
                }
                break;
            case Call::LifeCycleState::PROGRESS:
                {
                    /* show the current call view */
                    if (!IS_CURRENT_CALL_VIEW(current_view)) {
                        auto new_view = current_call_view_new();
                        g_signal_connect(new_view, "video-double-clicked", G_CALLBACK(video_double_clicked), win);
                        current_call_view_set_call_info(CURRENT_CALL_VIEW(new_view), CallModel::instance().getIndex(call));
                        gtk_container_remove(GTK_CONTAINER(priv->frame_call), current_view);
                        gtk_container_add(GTK_CONTAINER(priv->frame_call), new_view);
                        gtk_widget_show(new_view);
                    }
                }
                break;
            case Call::LifeCycleState::COUNT__:
                g_warning("LifeCycleState should never be COUNT");
                break;
        }
    } else if (idx_selected.isValid()) {
        /* otherwise, the call is over and is already removed from the RecentModel */
        auto current_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));
        leave_full_screen(win);

        /* show the chat view */
        if (!IS_CHAT_VIEW(current_view)) {
            auto type = idx_selected.data(static_cast<int>(Ring::Role::ObjectType)).value<Ring::ObjectType>();
            auto object = idx_selected.data(static_cast<int>(Ring::Role::Object));
            if (type == Ring::ObjectType::Person && object.isValid()) {
                /* show chat view constructed from Person object */
                auto new_view = chat_view_new_person(object.value<Person *>());
                g_signal_connect(new_view, "hide-view-clicked", G_CALLBACK(hide_view_clicked), win);
                gtk_container_remove(GTK_CONTAINER(priv->frame_call), current_view);
                gtk_container_add(GTK_CONTAINER(priv->frame_call), new_view);
                gtk_widget_show(new_view);
            } else if (type == Ring::ObjectType::ContactMethod && object.isValid()) {
                /* show chat view constructed from CM */
                auto new_view = chat_view_new_cm(object.value<ContactMethod *>());
                g_signal_connect(new_view, "hide-view-clicked", G_CALLBACK(hide_view_clicked), win);
                gtk_container_remove(GTK_CONTAINER(priv->frame_call), current_view);
                gtk_container_add(GTK_CONTAINER(priv->frame_call), new_view);
                gtk_widget_show(new_view);
            }
        }
    }

    return G_SOURCE_REMOVE;
}

static void
search_entry_placecall(G_GNUC_UNUSED GtkWidget *entry, gpointer win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(win));

    const gchar *number_entered = gtk_entry_get_text(GTK_ENTRY(priv->search_entry));

    if (number_entered && strlen(number_entered) > 0) {
        auto cm = PhoneDirectoryModel::instance().getNumber(number_entered);

        g_debug("dialing to number: %s", cm->uri().toUtf8().constData());

        place_new_call(cm);

        /* move focus away from entry so that DTMF tones can be entered via the keyboard */
        gtk_widget_child_focus(GTK_WIDGET(win), GTK_DIR_TAB_FORWARD);
        /* clear the entry */
        gtk_entry_set_text(GTK_ENTRY(priv->search_entry), "");
    }
}

static gboolean
save_accounts(GtkWidget *working_dialog)
{
    /* save changes to accounts */
    AccountModel::instance().save();

    if (working_dialog)
        gtk_widget_destroy(working_dialog);

    return G_SOURCE_REMOVE;
}

static void
settings_clicked(G_GNUC_UNUSED GtkButton *button, RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    /* check which view to show */
    if (!priv->show_settings) {
        /* show the settings */

        /* make sure we are not showing a call view so we don't have more than one clutter stage at a time */
        selection_changed(QModelIndex(), win);

        /* show settings */
        gtk_image_set_from_icon_name(GTK_IMAGE(priv->image_settings), "emblem-ok-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);

        gtk_widget_show(priv->hbox_settings);

        /* make sure to start preview if we're showing the video settings */
        if (priv->last_settings_view == priv->media_settings_view)
            media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(priv->media_settings_view), TRUE);

        /* make sure to show the profile if we're showing the general settings */
        if (priv->last_settings_view == priv->general_settings_view)
            general_settings_view_show_profile(GENERAL_SETTINGS_VIEW(priv->general_settings_view), TRUE);

        gtk_stack_set_transition_type(GTK_STACK(priv->stack_main_view), GTK_STACK_TRANSITION_TYPE_SLIDE_UP);
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_main_view), priv->last_settings_view);

        priv->show_settings = TRUE;
    } else {
        /* hide the settings */
        priv->show_settings = FALSE;

        /* show working dialog in case save operation takes time */
        GtkWidget *working = ring_dialog_working(GTK_WIDGET(win), NULL);
        gtk_window_present(GTK_WINDOW(working));

        /* now save after the time it takes to transition back to the call view (400ms)
         * the save doesn't happen before the "working" dialog is presented
         * the timeout function should destroy the "working" dialog when done saving
         */
        g_timeout_add_full(G_PRIORITY_DEFAULT, 400, (GSourceFunc)save_accounts, working, NULL);

        /* show calls */
        gtk_image_set_from_icon_name(GTK_IMAGE(priv->image_settings), "emblem-system-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);

        gtk_widget_hide(priv->hbox_settings);

        /* make sure video preview is stopped, in case it was started */
        media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(priv->media_settings_view), FALSE);
        general_settings_view_show_profile(GENERAL_SETTINGS_VIEW(priv->general_settings_view), FALSE);

        gtk_stack_set_transition_type(GTK_STACK(priv->stack_main_view), GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN);
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), CALL_VIEW_NAME);

        /* show the view which was selected previously */
        selection_changed(RecentModel::instance().selectionModel()->currentIndex(), win);
    }
}

static void
show_media_settings(GtkToggleButton *navbutton, RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    if (gtk_toggle_button_get_active(navbutton)) {
        media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(priv->media_settings_view), TRUE);
        gtk_stack_set_transition_type(GTK_STACK(priv->stack_main_view), GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), MEDIA_SETTINGS_VIEW_NAME);
        priv->last_settings_view = priv->media_settings_view;
    } else {
        media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(priv->media_settings_view), FALSE);
    }
}

static void
show_account_settings(GtkToggleButton *navbutton, RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    if (gtk_toggle_button_get_active(navbutton)) {
        gtk_stack_set_transition_type(GTK_STACK(priv->stack_main_view), GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), ACCOUNT_SETTINGS_VIEW_NAME);
        priv->last_settings_view = priv->account_settings_view;
    }
}

static void
show_general_settings(GtkToggleButton *navbutton, RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    if (gtk_toggle_button_get_active(navbutton)) {
        general_settings_view_show_profile(GENERAL_SETTINGS_VIEW(priv->general_settings_view), TRUE);
        gtk_stack_set_transition_type(GTK_STACK(priv->stack_main_view), GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), GENERAL_SETTINGS_VIEW_NAME);
        priv->last_settings_view = priv->general_settings_view;
    } else {
        general_settings_view_show_profile(GENERAL_SETTINGS_VIEW(priv->general_settings_view), FALSE);
    }
}

static void
on_account_creation_completed(RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    gtk_stack_set_transition_type(GTK_STACK(priv->stack_main_view), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
    gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), CALL_VIEW_NAME);

    /* destroy the wizard */
    GtkWidget* account_creation_wizard = gtk_stack_get_child_by_name(
        GTK_STACK(priv->stack_main_view),
        ACCOUNT_CREATION_WIZARD_VIEW_NAME
    );
    if (account_creation_wizard)
    {
        gtk_container_remove(GTK_CONTAINER(priv->stack_main_view), account_creation_wizard);
        gtk_widget_destroy(account_creation_wizard);
    }

    /* show the settings button*/
    gtk_widget_show(priv->ring_settings);
}

static void
show_account_creation_wizard(RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    auto account_creation_wizard = gtk_stack_get_child_by_name(
        GTK_STACK(priv->stack_main_view),
        ACCOUNT_CREATION_WIZARD_VIEW_NAME
    );

    if (!account_creation_wizard)
    {
        account_creation_wizard = account_creation_wizard_new(false);
        g_signal_connect_swapped(account_creation_wizard, "account-creation-completed", G_CALLBACK(on_account_creation_completed), win);

        gtk_stack_add_named(GTK_STACK(priv->stack_main_view),
                            account_creation_wizard,
                            ACCOUNT_CREATION_WIZARD_VIEW_NAME);
    }

    /* hide settings button until account creation is complete */
    gtk_widget_hide(priv->ring_settings);

    gtk_widget_show(account_creation_wizard);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), ACCOUNT_CREATION_WIZARD_VIEW_NAME);
}

static void
search_entry_text_changed(GtkEditable *search_entry, RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    /* get the text from the entry */
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(search_entry));

    if (text && strlen(text) > 0) {
        /* edit the the dialing call (or create a new one) */
        if (auto call = CallModel::instance().dialingCall()) {
            call->setDialNumber(text);
            priv->q_completion_model->setCall(call);
        }
    } else {
        if (auto call = priv->q_completion_model->call()) {
            if (call->lifeCycleState() == Call::LifeCycleState::CREATION)
                call->performAction(Call::Action::REFUSE);
        }
    }
}

static gboolean
completion_match_func(G_GNUC_UNUSED GtkEntryCompletion *completion,
                      G_GNUC_UNUSED const gchar *key,
                      G_GNUC_UNUSED GtkTreeIter *iter,
                      G_GNUC_UNUSED RingMainWindow *win)
{
    /* the model is updated by lrc and should only every contain matching entries
     * so always return TRUE */
    return TRUE;
}

static QModelIndex
get_qidx_from_filter_model(GtkTreeModelFilter *filter_model,
                           GtkTreeIter *filter_iter)
{
    GtkTreeModel *child_model = gtk_tree_model_filter_get_model(filter_model);
    GtkTreeIter child_iter;
    gtk_tree_model_filter_convert_iter_to_child_iter(
        GTK_TREE_MODEL_FILTER(filter_model),
        &child_iter,
        filter_iter);

    return gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(child_model), &child_iter);
}

static void
autocompletion_photo_render(G_GNUC_UNUSED GtkCellLayout *cell_layout,
                            GtkCellRenderer *cell,
                            GtkTreeModel *model,
                            GtkTreeIter *iter,
                            G_GNUC_UNUSED gpointer user_data)
{
    QModelIndex idx = get_qidx_from_filter_model(GTK_TREE_MODEL_FILTER(model), iter);
    if (idx.isValid()) {
        QVariant photo_var = idx.sibling(idx.row(), 1).data(Qt::DecorationRole);
        if (photo_var.isValid()) {
            std::shared_ptr<GdkPixbuf> photo = photo_var.value<std::shared_ptr<GdkPixbuf>>();
            GdkPixbuf *scaled = gdk_pixbuf_scale_simple(photo.get(),
                                                        20, 20,
                                                        GDK_INTERP_BILINEAR);

            g_object_set(G_OBJECT(cell), "pixbuf", scaled, NULL);
            g_object_unref(scaled);
            return;
        }
    }

    g_object_set(G_OBJECT(cell), "pixbuf", NULL, NULL);
}

static void
autocompletion_name_render(G_GNUC_UNUSED GtkCellLayout *cell_layout,
                           GtkCellRenderer *cell,
                           GtkTreeModel *model,
                           GtkTreeIter *iter,
                           G_GNUC_UNUSED gpointer user_data)
{
    gchar *text = nullptr;
    QModelIndex idx = get_qidx_from_filter_model(GTK_TREE_MODEL_FILTER(model), iter);
    if (idx.isValid()) {
        QVariant name = idx.sibling(idx.row(), 1).data(Qt::DisplayRole);
        text = g_markup_printf_escaped("<span weight=\"bold\">%s</span>",
                                       name.value<QString>().toUtf8().constData());
    }

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(text);
}

static void
autocompletion_number_render(G_GNUC_UNUSED GtkCellLayout *cell_layout,
                             GtkCellRenderer *cell,
                             GtkTreeModel *model,
                             GtkTreeIter *iter,
                             G_GNUC_UNUSED gpointer user_data)
{
    gchar *text = nullptr;
    QModelIndex idx = get_qidx_from_filter_model(GTK_TREE_MODEL_FILTER(model), iter);
    if (idx.isValid()) {
        QVariant uri = idx.data(Qt::DisplayRole);
        text = g_markup_escape_text(uri.value<QString>().toUtf8().constData(), -1);
    }

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(text);
}

static void
autocompletion_account_render(G_GNUC_UNUSED GtkCellLayout *cell_layout,
                              GtkCellRenderer *cell,
                              GtkTreeModel *model,
                              GtkTreeIter *iter,
                              G_GNUC_UNUSED gpointer user_data)
{
    gchar *text = nullptr;
    QModelIndex idx = get_qidx_from_filter_model(GTK_TREE_MODEL_FILTER(model), iter);
    if (idx.isValid()) {
        QVariant alias = idx.sibling(idx.row(), 2).data(Qt::DisplayRole);
        text = g_markup_printf_escaped("<span color=\"gray\">%s</span>",
                                       alias.value<QString>().toUtf8().constData());
    }

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(text);
}

static gboolean
select_autocompletion(G_GNUC_UNUSED GtkEntryCompletion *widget,
                      GtkTreeModel *model,
                      GtkTreeIter *iter,
                      RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);
    if (idx.isValid()) {
        auto cm = priv->q_completion_model->number(idx);

        place_new_call(cm);

        /* move focus away from entry so that DTMF tones can be entered via the keyboard */
        gtk_widget_child_focus(GTK_WIDGET(win), GTK_DIR_TAB_FORWARD);

        /* clear the entry */
        gtk_entry_set_text(GTK_ENTRY(priv->search_entry), "");
    } else {
        g_warning("autocompletion selection is not a valid index!");
    }
    return TRUE;
}

static gboolean
dtmf_pressed(RingMainWindow *win,
              GdkEventKey *event,
              G_GNUC_UNUSED gpointer user_data)
{
    g_return_val_if_fail(event->type == GDK_KEY_PRESS, GDK_EVENT_PROPAGATE);

    /* we want to react to digit key presses, as long as a GtkEntry is not the
     * input focus
     */
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(win));
    if (GTK_IS_ENTRY(focus))
        return GDK_EVENT_PROPAGATE;

    /* make sure that a call is selected*/
    QItemSelectionModel *selection = CallModel::instance().selectionModel();
    QModelIndex idx = selection->currentIndex();
    if (!idx.isValid())
        return GDK_EVENT_PROPAGATE;

    /* make sure that the selected call is in progress */
    Call *call = CallModel::instance().getCall(idx);
    Call::LifeCycleState state = call->lifeCycleState();
    if (state != Call::LifeCycleState::PROGRESS)
        return GDK_EVENT_PROPAGATE;

    /* filter out cretain MOD masked key presses so that, for example, 'Ctrl+c'
     * does not result in a 'c' being played.
     * we filter Ctrl, Alt, and SUPER/HYPER/META keys */
    if ( event->state
        & ( GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ))
        return GDK_EVENT_PROPAGATE;

    /* pass the character that was entered to be played by the daemon;
     * the daemon will filter out invalid DTMF characters */
    guint32 unicode_val = gdk_keyval_to_unicode(event->keyval);
    QString val = QString::fromUcs4(&unicode_val, 1);
    call->playDTMF(val);
    g_debug("attemptingto play DTMF tone during ongoing call: %s", val.toUtf8().constData());

    /* always propogate the key, so we don't steal accelerators/shortcuts */
    return GDK_EVENT_PROPAGATE;
}

static void
handle_account_migrations(RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    /* If there is an existing migration view, remove it */
    GtkWidget* old_view = gtk_stack_get_child_by_name(GTK_STACK(priv->stack_main_view), ACCOUNT_MIGRATION_VIEW_NAME);
    if (old_view)
    {
        gtk_container_remove(GTK_CONTAINER(priv->stack_main_view), old_view);
    }

    QList<Account*> accounts = AccountModel::instance().accountsToMigrate();
    if (!accounts.isEmpty())
    {
        Account* account = accounts.first();
        g_debug("Migrating account: %s", account->id().toStdString().c_str());

        GtkWidget* account_migration_view = account_migration_view_new(account);
        g_signal_connect_swapped(account_migration_view, "account-migration-completed", G_CALLBACK(handle_account_migrations), win);
        g_signal_connect_swapped(account_migration_view, "account-migration-failed", G_CALLBACK(handle_account_migrations), win);

        gtk_widget_hide(priv->ring_settings);
        gtk_widget_show(account_migration_view);
        gtk_stack_add_named(
            GTK_STACK(priv->stack_main_view),
            account_migration_view,
            ACCOUNT_MIGRATION_VIEW_NAME
        );
        gtk_stack_set_visible_child_name(
            GTK_STACK(priv->stack_main_view),
            ACCOUNT_MIGRATION_VIEW_NAME
        );
    }
    else
    {
        gtk_widget_show(priv->ring_settings);
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
        g_clear_error(&error);
    } else
        gtk_window_set_icon(GTK_WINDOW(win), icon);

    /* set menu icon */
    GdkPixbuf* image_ring = gdk_pixbuf_new_from_resource_at_scale("/cx/ring/RingGnome/ring-symbol-blue",
                                                                  -1, 16, TRUE, &error);
    if (image_ring == NULL) {
        g_debug("Could not load icon: %s", error->message);
        g_clear_error(&error);
    } else
        gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_ring), image_ring);

    /* ring menu */
    GtkBuilder *builder = gtk_builder_new_from_resource("/cx/ring/RingGnome/ringgearsmenu.ui");
    GMenuModel *menu = G_MENU_MODEL(gtk_builder_get_object(builder, "menu"));
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(priv->ring_menu), menu);
    g_object_unref(builder);

    /* settings icon */
    gtk_image_set_from_icon_name(GTK_IMAGE(priv->image_settings), "emblem-system-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);

    /* connect settings button signal */
    g_signal_connect(priv->ring_settings, "clicked", G_CALLBACK(settings_clicked), win);

    /* add the call view to the main stack */
    gtk_stack_add_named(GTK_STACK(priv->stack_main_view),
                        priv->vbox_call_view,
                        CALL_VIEW_NAME);

    if (has_ring_account()) {
        /* user has ring account, so show the call view right away */
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_main_view), priv->vbox_call_view);
    } else {
        /* user has to create the ring account */
        show_account_creation_wizard(win);
    }

    /* init the settings views */
    priv->account_settings_view = account_view_new();
    gtk_stack_add_named(GTK_STACK(priv->stack_main_view), priv->account_settings_view, ACCOUNT_SETTINGS_VIEW_NAME);

    priv->media_settings_view = media_settings_view_new();
    gtk_stack_add_named(GTK_STACK(priv->stack_main_view), priv->media_settings_view, MEDIA_SETTINGS_VIEW_NAME);

    priv->general_settings_view = general_settings_view_new();
    gtk_stack_add_named(GTK_STACK(priv->stack_main_view), priv->general_settings_view, GENERAL_SETTINGS_VIEW_NAME);

    /* make the setting we will show first the active one */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->radiobutton_general_settings), TRUE);
    priv->last_settings_view = priv->general_settings_view;

    /* connect the settings button signals to switch settings views */
    g_signal_connect(priv->radiobutton_media_settings, "toggled", G_CALLBACK(show_media_settings), win);
    g_signal_connect(priv->radiobutton_account_settings, "toggled", G_CALLBACK(show_account_settings), win);
    g_signal_connect(priv->radiobutton_general_settings, "toggled", G_CALLBACK(show_general_settings), win);

    /* populate the notebook */
    auto smart_view = recent_contacts_view_new();
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_smartview), smart_view);

    auto contacts_view = contacts_view_new();
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_contacts), contacts_view);

    auto history_view = history_view_new();
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_history), history_view);

    /* welcome/default view */
    priv->welcome_view = ring_welcome_view_new();
    g_object_ref(priv->welcome_view);
    // gtk_stack_add_named(GTK_STACK(priv->stack_call_view), welcome_view, DEFAULT_VIEW_NAME);
    gtk_container_add(GTK_CONTAINER(priv->frame_call), priv->welcome_view);
    gtk_widget_show(priv->welcome_view);

    /* call/chat selection */
    QObject::connect(
       RecentModel::instance().selectionModel(),
       &QItemSelectionModel::currentChanged,
       [win](const QModelIndex current, G_GNUC_UNUSED const QModelIndex & previous) {
            if (auto call = RecentModel::instance().getActiveCall(current)) {
                /* if the call is on hold, we want to put it off hold automatically
                 * when switching to it */
                if (call->state() == Call::State::HOLD)
                    call << Call::Action::HOLD;
            }
            selection_changed(current, win);
        }
    );

    /* connect to dataChanged of the RecentModel to see if we need to change the view */
    QObject::connect(
        &RecentModel::instance(),
        &RecentModel::dataChanged,
        [win](const QModelIndex & topLeft, G_GNUC_UNUSED const QModelIndex & bottomRight, G_GNUC_UNUSED const QVector<int> & roles) {
            /* it is possible for dataChanged to be emitted inside of a dataChanged handler or
             * some other signal; since the connection is via a lambda, Qt would cause the
             * handler to be called directly. This is not behaviour we usually want, so we call our
             * function via g_idle so that it gets called after the initial handler is done.
             */
            if (topLeft == RecentModel::instance().selectionModel()->currentIndex())
                g_idle_add((GSourceFunc)selected_item_changed, win);
        }
    );

    g_signal_connect(priv->button_placecall, "clicked", G_CALLBACK(search_entry_placecall), win);
    g_signal_connect(priv->search_entry, "activate", G_CALLBACK(search_entry_placecall), win);

    /* autocompletion */
    priv->q_completion_model = new NumberCompletionModel();

    /* autocompletion renderers */
    GtkCellArea *completion_area = gtk_cell_area_box_new();

    /* photo renderer */
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(completion_area),
                                 renderer,
                                 TRUE,  /* expand */
                                 TRUE,  /* align */
                                 TRUE); /* fixed size */

    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(completion_area),
                                       renderer,
                                       (GtkCellLayoutDataFunc)autocompletion_photo_render,
                                       NULL, NULL);

    /* name renderer */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(completion_area),
                                 renderer,
                                 TRUE,  /* expand */
                                 TRUE,  /* align */
                                 TRUE); /* fixed size */

    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(completion_area),
                                       renderer,
                                       (GtkCellLayoutDataFunc)autocompletion_name_render,
                                       NULL, NULL);

    /* number renderer */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(completion_area),
                                 renderer,
                                 TRUE,  /* expand */
                                 TRUE,  /* align */
                                 TRUE); /* fixed size */

    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(completion_area),
                                       renderer,
                                       (GtkCellLayoutDataFunc)autocompletion_number_render,
                                       NULL, NULL);
    /* account renderer */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(completion_area),
                                 renderer,
                                 TRUE,  /* expand */
                                 TRUE,  /* align */
                                 TRUE); /* fixed size */

    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(completion_area),
                                       renderer,
                                       (GtkCellLayoutDataFunc)autocompletion_account_render,
                                       NULL, NULL);

    GtkEntryCompletion *entry_completion = gtk_entry_completion_new_with_area(completion_area);

    GtkQTreeModel *completion_model = gtk_q_tree_model_new(
        (QAbstractItemModel *)priv->q_completion_model,
        1,
        0, Qt::DisplayRole, G_TYPE_STRING);

    gtk_entry_completion_set_model(entry_completion, GTK_TREE_MODEL(completion_model));

    gtk_entry_set_completion(GTK_ENTRY(priv->search_entry), entry_completion);
    gtk_entry_completion_set_match_func(
        entry_completion,
        (GtkEntryCompletionMatchFunc) completion_match_func,
        NULL,
        NULL);

    /* connect signal to when text is entered in the entry */
    g_signal_connect(priv->search_entry, "changed", G_CALLBACK(search_entry_text_changed), win);
    g_signal_connect(entry_completion, "match-selected", G_CALLBACK(select_autocompletion), win);

    /* connect to incoming call and focus */
    QObject::connect(
        &CallModel::instance(),
        &CallModel::incomingCall,
        [=](Call* call) {
            CallModel::instance().selectionModel()->setCurrentIndex(
                CallModel::instance().getIndex(call), QItemSelectionModel::ClearAndSelect);
        }
    );

    /* react to digit key press events */
    g_signal_connect(win, "key-press-event", G_CALLBACK(dtmf_pressed), NULL);

    /* set the search entry placeholder text */
    gtk_entry_set_placeholder_text(GTK_ENTRY(priv->search_entry),
                                   C_("Please try to make the translation 50 chars or less so that it fits into the layout", "Search contacts or enter number"));

    handle_account_migrations(win);
}

static void
ring_main_window_dispose(GObject *object)
{
    RingMainWindow *self = RING_MAIN_WINDOW(object);
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    QObject::disconnect(priv->selection_updated);

    G_OBJECT_CLASS(ring_main_window_parent_class)->dispose(object);
}

static void
ring_main_window_finalize(GObject *object)
{
    RingMainWindow *self = RING_MAIN_WINDOW(object);
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    G_OBJECT_CLASS(ring_main_window_parent_class)->finalize(object);
}

static void
ring_main_window_class_init(RingMainWindowClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = ring_main_window_finalize;
    G_OBJECT_CLASS(klass)->dispose = ring_main_window_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/ringmainwindow.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, vbox_left_pane);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, scrolled_window_smartview);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, scrolled_window_contacts);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, scrolled_window_history);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, ring_menu);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, image_ring);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, ring_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, image_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, hbox_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, search_entry);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, stack_main_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, vbox_call_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, frame_call);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, button_placecall);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_general_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_media_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_account_settings);
}

GtkWidget *
ring_main_window_new (GtkApplication *app)
{
    gpointer win = g_object_new(RING_MAIN_WINDOW_TYPE, "application", app, NULL);

    return (GtkWidget *)win;
}
