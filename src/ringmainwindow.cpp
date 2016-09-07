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
#include "recentcontactsview.h"
#include "chatview.h"
#include "avatarmanipulation.h"

static constexpr const char* CALL_VIEW_NAME             = "calls";
static constexpr const char* CREATE_ACCOUNT_VIEW_NAME   = "wizard";
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
    GtkWidget *treeview_conversations;
    GtkWidget *scrolled_window_contacts;
    GtkWidget *treeview_contacts;
    GtkWidget *scrolled_window_history;
    GtkWidget *treeview_history;
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

    QMetaObject::Connection selected_item_changed;
    QMetaObject::Connection selected_call_over;

    gboolean   show_settings;

    /* account creation */
    GtkWidget *account_creation;
    GtkWidget *image_ring_logo;
    GtkWidget *vbox_account_creation_entry;
    GtkWidget *entry_alias;
    GtkWidget *label_default_name;
    GtkWidget *label_paceholder;
    GtkWidget *label_generating_account;
    GtkWidget *spinner_generating_account;
    GtkWidget *button_account_creation_next;
    GtkWidget *box_avatarselection;
    GtkWidget* avatar_manipulation;

    QMetaObject::Connection hash_updated;

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
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    /* clear selection */
    auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_conversations));
    gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_conversations));
    auto selection_contacts = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_contacts));
    gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_contacts));
    auto selection_history = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_history));
    gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_history));
}

/**
 * This function determines which item is currently selected in one if the 3 contact list views
 * (Conversations, Contacts, History), as only one item at a time should ever be selected, and
 * displays that item as one of the 3 possible views:
 * - incoming call view
 * - current call view
 * - chat view
 * - welcome view (if no valid item is selected)
 *
 * This function could be called from a g_idle source, so it returns a boolean to remove itself from
 * being called again. The boolean doesn't reflect success nor failure.
 */
static gboolean
selection_changed(RingMainWindow *win)
{
    // g_debug("selection changed");

    g_return_val_if_fail(IS_RING_MAIN_WINDOW(win), G_SOURCE_REMOVE);
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(win));

    auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));
    GtkWidget *new_view = nullptr;

    /* if we're showing the settings, then we need to make sure to remove any possible ongoing call
     * view so that we don't have more than one VideoWidget at a time */
    if (priv->show_settings) {
        if (old_view != priv->welcome_view) {
            leave_full_screen(win);
            gtk_container_remove(GTK_CONTAINER(priv->frame_call), old_view);
            gtk_container_add(GTK_CONTAINER(priv->frame_call), priv->welcome_view);
            gtk_widget_show(priv->welcome_view);
        }
        return G_SOURCE_REMOVE;
    }

    /* there should only be one item selected at a time, get which one is selected */
    auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_conversations));
    auto selection_contacts = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_contacts));
    auto selection_history = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_history));

    GtkTreeModel *model = nullptr;
    GtkTreeIter iter;
    QModelIndex idx;

    if (gtk_tree_selection_get_selected(selection_conversations, &model, &iter)) {
        idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), &iter);
    } else if (gtk_tree_selection_get_selected(selection_contacts, &model, &iter)) {
        idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), &iter);
    } else if (gtk_tree_selection_get_selected(selection_history, &model, &iter)) {
        idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), &iter);
    }

    /* check which object type is selected */
    auto type = idx.data(static_cast<int>(Ring::Role::ObjectType));
    auto object = idx.data(static_cast<int>(Ring::Role::Object));

    if (idx.isValid() && type.isValid() && object.isValid()) {

        /* we prioritize the views in the following order:
         *  - call view (if there is an ongoing call associated with the item)
         *  - chat view built from Person
         *  - chat view built from ContactMethod
         */

        Person *person = nullptr;
        ContactMethod *cm = nullptr;
        Call *call = nullptr;

        /* use the RecentModel to see if there are any ongoing calls associated with the selected item */
        switch(type.value<Ring::ObjectType>()) {
            case Ring::ObjectType::Person:
            {
                person = object.value<Person *>();
                auto recent_idx = RecentModel::instance().getIndex(person);
                if (recent_idx.isValid()) {
                    call = RecentModel::instance().getActiveCall(recent_idx);
                }
            }
            break;
            case Ring::ObjectType::ContactMethod:
            {
                cm = object.value<ContactMethod *>();
                auto recent_idx = RecentModel::instance().getIndex(cm);
                if (recent_idx.isValid()) {
                    call = RecentModel::instance().getActiveCall(recent_idx);
                }
            }
            break;
            case Ring::ObjectType::Call:
            {
                /* if the call is over (eg: if it comes from the history model) then we first check
                 * if there is an ongoing call with the same ContactMethod, otherwise we need to get
                 * the associated Person or ContactMethod */
                call = object.value<Call *>();
                if (call->isHistory()) {
                    cm = call->peerContactMethod();
                    call = nullptr;
                    if (cm) {
                        auto recent_idx = RecentModel::instance().getIndex(cm);
                        if (recent_idx.isValid()) {
                            call = RecentModel::instance().getActiveCall(recent_idx);
                        }
                        person = cm->contact();
                    }
                }
            }
            break;
            case Ring::ObjectType::Media:
            case Ring::ObjectType::Certificate:
            case Ring::ObjectType::COUNT__:
            // nothing to do
            break;
        }

        /* we prioritize showing the call view */
        if (call) {
            auto state = call->lifeCycleState();

            Call *current_call = nullptr;

            switch(state) {
                case Call::LifeCycleState::CREATION:
                case Call::LifeCycleState::INITIALIZATION:
                case Call::LifeCycleState::FINISHED:
                    // check if we're already displaying this call
                    if (IS_INCOMING_CALL_VIEW(old_view))
                        current_call = incoming_call_view_get_call(INCOMING_CALL_VIEW(old_view));
                    if (current_call != call)
                        new_view = incoming_call_view_new(call);
                    break;
                case Call::LifeCycleState::PROGRESS:
                    // check if we're already displaying this call
                    if (IS_CURRENT_CALL_VIEW(old_view))
                        current_call = current_call_view_get_call(CURRENT_CALL_VIEW(old_view));
                    if (current_call != call) {
                        new_view = current_call_view_new(call);
                        g_signal_connect(new_view, "video-double-clicked", G_CALLBACK(video_double_clicked), win);
                    }
                    break;
                case Call::LifeCycleState::COUNT__:
                    g_warning("LifeCycleState should never be COUNT");
                    break;
            }

            if (new_view) {
                /* connect to the call's lifeCycleStateChanged signal to know when to change the view */
                QObject::disconnect(priv->selected_item_changed);
                priv->selected_item_changed = QObject::connect(
                    call,
                    &Call::lifeCycleStateChanged,
                    [win] (Call::LifeCycleState, Call::LifeCycleState)
                    {g_idle_add((GSourceFunc)selection_changed, win);}
                );
                /* we want to also change the view when the call is over */
                QObject::disconnect(priv->selected_call_over);
                priv->selected_call_over = QObject::connect(
                    call,
                    &Call::isOver,
                    [win] ()
                    {g_idle_add((GSourceFunc)selection_changed, win);}
                );
            }

        } else if (person) {
            /* show chat view constructed from Person object */

            // check if we're already displaying the chat view for this person
            Person *current_person = nullptr;
            if (IS_CHAT_VIEW(old_view))
                current_person = chat_view_get_person(CHAT_VIEW(old_view));

            if (current_person != person) {
                new_view = chat_view_new_person(person);
                g_signal_connect(new_view, "hide-view-clicked", G_CALLBACK(hide_view_clicked), win);

                /* connect to the Person's callAdded signal, because we want to switch to the call view
                 * in this case */
                QObject::disconnect(priv->selected_item_changed);
                QObject::disconnect(priv->selected_call_over);
                priv->selected_item_changed = QObject::connect(
                    person,
                    &Person::callAdded,
                    [win] (Call*)
                    {g_idle_add((GSourceFunc)selection_changed, win);}
                );
            }
        } else if (cm) {
            /* show chat view constructed from CM */

            // check if we're already displaying the chat view for this cm
            ContactMethod *current_cm = nullptr;
            if (IS_CHAT_VIEW(old_view))
                current_cm = chat_view_get_cm(CHAT_VIEW(old_view));

            if (current_cm != cm) {
                new_view = chat_view_new_cm(cm);
                g_signal_connect(new_view, "hide-view-clicked", G_CALLBACK(hide_view_clicked), win);

                /* connect to the ContactMethod's callAdded signal, because we want to switch to the
                 *call view in this case */
                QObject::disconnect(priv->selected_item_changed);
                QObject::disconnect(priv->selected_call_over);
                priv->selected_item_changed = QObject::connect(
                    cm,
                    &ContactMethod::callAdded,
                    [win] (Call*)
                    {g_idle_add((GSourceFunc)selection_changed, win);}
                );
            }
        } else {
            /* not a supported object type, display the welcome view */
            if (old_view != priv->welcome_view) {
                QObject::disconnect(priv->selected_item_changed);
                QObject::disconnect(priv->selected_call_over);
                new_view = priv->welcome_view;
            }
        }
    } else {
        /* selection isn't valid, display the welcome view */
        if (old_view != priv->welcome_view) {
            QObject::disconnect(priv->selected_item_changed);
            QObject::disconnect(priv->selected_call_over);
            new_view = priv->welcome_view;
        }
    }

    if (new_view) {
        /* make sure we leave full screen, since the view is changing */
        leave_full_screen(win);
        gtk_container_remove(GTK_CONTAINER(priv->frame_call), old_view);
        gtk_container_add(GTK_CONTAINER(priv->frame_call), new_view);
        gtk_widget_show(new_view);
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
        priv->show_settings = TRUE;

        /* make sure we are not showing a call view so we don't have more than one clutter stage at a time */
        selection_changed(win);

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
        selection_changed(win);
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

static gboolean
create_ring_account(RingMainWindow *win)
{
    g_return_val_if_fail(IS_RING_MAIN_WINDOW(win), G_SOURCE_REMOVE);
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    /* create account and set UPnP enabled, as its not by default in the daemon */
    const gchar *alias = gtk_entry_get_text(GTK_ENTRY(priv->entry_alias));
    Account *account = nullptr;

    /* get profile (if so) */
    auto profile = ProfileModel::instance().selectedProfile();

    if (profile) {
        if (alias && strlen(alias) > 0) {
            account = AccountModel::instance().add(alias, Account::Protocol::RING);
            profile->person()->setFormattedName(alias);
        } else {
            auto unknown_alias = C_("The default username / account alias, if none is set by the user", "Unknown");
            account = AccountModel::instance().add(unknown_alias, Account::Protocol::RING);
            profile->person()->setFormattedName(unknown_alias);
        }
    }

    account->setDisplayName(alias); // set the display name to the same as the alias
    account->setUpnpEnabled(TRUE);

    /* wait for hash to be generated to show the main view */
    priv->hash_updated = QObject::connect(
        account,
        &Account::changed,
        [=] (Account *a) {
            QString hash = a->username();
            if (!hash.isEmpty()) {
                /* show the call view */
                gtk_stack_set_transition_type(GTK_STACK(priv->stack_main_view), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
                gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), CALL_VIEW_NAME);

                /* show the settings button*/
                gtk_widget_show(priv->ring_settings);

                //once the account is created we don't want this lambda to be called again
                QObject::disconnect(priv->hash_updated);
            }
        }
    );

    account->performAction(Account::EditAction::SAVE);
    profile->save();

    return G_SOURCE_REMOVE;
}

static void
alias_entry_changed(GtkEditable *entry, RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    const gchar *alias = gtk_entry_get_text(GTK_ENTRY(entry));
    if (!alias || strlen(alias) == 0) {
        gtk_widget_show(priv->label_default_name);
        gtk_widget_hide(priv->label_paceholder);
    } else {
        gtk_widget_hide(priv->label_default_name);
        gtk_widget_show(priv->label_paceholder);
    }
}

static void
account_creation_next_clicked(G_GNUC_UNUSED GtkButton *button, RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    /* show/hide relevant widgets */
    gtk_widget_hide(priv->vbox_account_creation_entry);
    gtk_widget_hide(priv->button_account_creation_next);
    gtk_widget_show(priv->label_generating_account);
    gtk_widget_show(priv->spinner_generating_account);

    /* make sure the AvatarManipulation widget is destroyed so the VideoWidget inside of it is too;
     * NOTE: destorying its parent (box_avatarselection) first will cause a mystery 'BadDrawable'
     * crash due to X error */
    gtk_container_remove(GTK_CONTAINER(priv->box_avatarselection), priv->avatar_manipulation);
    priv->avatar_manipulation = nullptr;

    /* now create account after a short timeout so that the the save doesn't
     * happen freeze the client before the widget changes happen;
     * the timeout function should then display the next step in account creation
     */
    g_timeout_add_full(G_PRIORITY_DEFAULT, 300, (GSourceFunc)create_ring_account, win, NULL);
}

static void
entry_alias_activated(GtkEntry *entry, RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    const gchar *alias = gtk_entry_get_text(GTK_ENTRY(entry));
    if (strlen(alias) > 0)
        gtk_button_clicked(GTK_BUTTON(priv->button_account_creation_next));
}

static void
show_account_creation(RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    gtk_stack_add_named(GTK_STACK(priv->stack_main_view),
                        priv->account_creation,
                        CREATE_ACCOUNT_VIEW_NAME);

    /* hide settings button until account creation is complete */
    gtk_widget_hide(priv->ring_settings);

    /* set ring logo */
    GError *error = NULL;
    GdkPixbuf* logo_ring = gdk_pixbuf_new_from_resource_at_scale("/cx/ring/RingGnome/ring-logo-blue",
                                                                  -1, 50, TRUE, &error);
    if (logo_ring == NULL) {
        g_debug("Could not load logo: %s", error->message);
        g_clear_error(&error);
    } else
        gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_ring_logo), logo_ring);

    /* use the real name / username of the logged in user as the default */
    const char* real_name = g_get_real_name();
    const char* user_name = g_get_user_name();
    g_debug("real_name = %s",real_name);
    g_debug("user_name = %s",user_name);

    /* check first if the real name was determined */
    if (g_strcmp0 (real_name,"Unknown") != 0)
        gtk_entry_set_text(GTK_ENTRY(priv->entry_alias), real_name);
    else
        gtk_entry_set_text(GTK_ENTRY(priv->entry_alias), user_name);

    /* avatar manipulation widget */
    priv->avatar_manipulation = avatar_manipulation_new_from_wizard();
    gtk_box_pack_start(GTK_BOX(priv->box_avatarselection), priv->avatar_manipulation, true, true, 0);

    /* connect signals */
    g_signal_connect(priv->entry_alias, "changed", G_CALLBACK(alias_entry_changed), win);
    g_signal_connect(priv->button_account_creation_next, "clicked", G_CALLBACK(account_creation_next_clicked), win);
    g_signal_connect(priv->entry_alias, "activate", G_CALLBACK(entry_alias_activated), win);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), CREATE_ACCOUNT_VIEW_NAME);
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
conversation_selection_changed(GtkTreeSelection *selection, RingMainWindow *self)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, nullptr, &iter)) {
        /* something is selected in the conversations view, clear the selection in the other views;
         * we only support one selected item at a time
         */
         auto selection_contacts = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_contacts));
         gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_contacts));
         auto selection_history = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_history));
         gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_history));
    }

    selection_changed(self);
}

static void
contact_selection_changed(GtkTreeSelection *selection, RingMainWindow *self)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, nullptr, &iter)) {
        /* something is selected in the contacts view, clear the selection in the other views;
         * we only support one selected item at a time
         */
         auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_conversations));
         gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_conversations));
         auto selection_history = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_history));
         gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_history));
    }

    selection_changed(self);
}

static void
history_selection_changed(GtkTreeSelection *selection, RingMainWindow *self)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, nullptr, &iter)) {
        /* something is selected in the history view, clear the selection in the other views;
         * we only support one selected item at a time
         */
         auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_conversations));
         gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_conversations));
         auto selection_contacts = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_contacts));
         gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_contacts));
    }

    selection_changed(self);
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
        show_account_creation(win);
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
    priv->treeview_conversations = recent_contacts_view_new();
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_smartview), priv->treeview_conversations);

    priv->treeview_contacts = contacts_view_new();
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_contacts), priv->treeview_contacts);

    priv->treeview_history = history_view_new();
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_history), priv->treeview_history);

    /* welcome/default view */
    priv->welcome_view = ring_welcome_view_new();
    g_object_ref(priv->welcome_view); // increase ref because don't want it to be destroyed when not displayed
    gtk_container_add(GTK_CONTAINER(priv->frame_call), priv->welcome_view);
    gtk_widget_show(priv->welcome_view);

    auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_conversations));
    g_signal_connect(selection_conversations, "changed", G_CALLBACK(conversation_selection_changed), win);

    auto selection_contacts = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_contacts));
    g_signal_connect(selection_contacts, "changed", G_CALLBACK(contact_selection_changed), win);

    auto selection_history = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_history));
    g_signal_connect(selection_history, "changed", G_CALLBACK(history_selection_changed), win);

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

    /* react to digit key press events */
    g_signal_connect(win, "key-press-event", G_CALLBACK(dtmf_pressed), NULL);

    /* set the search entry placeholder text */
    gtk_entry_set_placeholder_text(GTK_ENTRY(priv->search_entry),
                                   C_("Please try to make the translation 50 chars or less so that it fits into the layout", "Search contacts or enter number"));
}

static void
ring_main_window_dispose(GObject *object)
{
    RingMainWindow *self = RING_MAIN_WINDOW(object);
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    QObject::disconnect(priv->selected_item_changed);
    QObject::disconnect(priv->selected_call_over);
    g_object_unref(priv->welcome_view); // can now be destroyed

    G_OBJECT_CLASS(ring_main_window_parent_class)->dispose(object);
}

static void
ring_main_window_finalize(GObject *object)
{
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

    /* account creation */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, account_creation);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, image_ring_logo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, vbox_account_creation_entry);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, entry_alias);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, label_default_name);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, label_paceholder);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, label_generating_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, spinner_generating_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, button_account_creation_next);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, box_avatarselection);
}

GtkWidget *
ring_main_window_new (GtkApplication *app)
{
    gpointer win = g_object_new(RING_MAIN_WINDOW_TYPE, "application", app, NULL);

    return (GtkWidget *)win;
}
