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
#include <call.h>
#include <QtCore/QItemSelectionModel>
#include "incomingcallview.h"
#include "currentcallview.h"
#include <string.h>
#include <categorizedhistorymodel.h>
#include <contactmethod.h>
#include <QtCore/QSortFilterProxyModel>
#include "models/gtkqsortfiltertreemodel.h"
#include "accountview.h"
#include <accountmodel.h>
#include <audio/codecmodel.h>
#include "dialogs.h"
#include "videosettingsview.h"
#include <video/previewmanager.h>
#include "defines.h"

#define CALL_VIEW_NAME "calls"
#define CREATE_ACCOUNT_1_VIEW_NAME "create1"
#define CREATE_ACCOUNT_2_VIEW_NAME "create2"
#define GENERAL_SETTINGS_VIEW_NAME "general"
#define AUDIO_SETTINGS_VIEW_NAME "audio"
#define VIDEO_SETTINGS_VIEW_NAME "video"
#define ACCOUNT_SETTINGS_VIEW_NAME "accounts"
#define DEFAULT_VIEW_NAME "placeholder"
#define VIEW_CONTACTS "contacts"
#define VIEW_HISTORY "history"
#define VIEW_PRESENCE "presence"

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
    GtkWidget *hbox_search;
    GtkWidget *hbox_settings;
    GtkWidget *stack_contacts_history_presence;
    GtkWidget *radiobutton_contacts;
    GtkWidget *radiobutton_history;
    GtkWidget *radiobutton_presence;
    GtkWidget *treeview_call;
    GtkWidget *search_entry;
    GtkWidget *stack_main_view;
    GtkWidget *vbox_call_view;
    GtkWidget *stack_call_view;
    GtkWidget *button_placecall;
    GtkWidget *account_settings_view;
    GtkWidget *video_settings_view;
    GtkWidget *last_settings_view;
    GtkWidget *radiobutton_audio_settings;
    GtkWidget *radiobutton_general_settings;
    GtkWidget *radiobutton_video_settings;
    GtkWidget *radiobutton_account_settings;
    GtkWidget *entry_ring_id;

    Account *active_ring_account;
    QMetaObject::Connection active_ring_account_updates;

    gboolean   show_settings;

    /* account creation */
    GtkWidget *account_creation_1;
    GtkWidget *image_ring_logo;
    GtkWidget *label_enter_alias;
    GtkWidget *entry_alias;
    GtkWidget *label_generating_account;
    GtkWidget *spinner_generating_account;
    GtkWidget *button_account_creation_next;
    GtkWidget *account_creation_2;
    GtkWidget *entry_hash;
    GtkWidget *button_account_creation_done;

    QMetaObject::Connection hash_updated;
};

G_DEFINE_TYPE_WITH_PRIVATE(RingMainWindow, ring_main_window, GTK_TYPE_APPLICATION_WINDOW);

#define RING_MAIN_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RING_MAIN_WINDOW_TYPE, RingMainWindowPrivate))

static QModelIndex
get_index_from_selection(GtkTreeSelection *selection)
{
    GtkTreeIter iter;
    GtkTreeModel *model = NULL;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        if (GTK_IS_Q_TREE_MODEL(model))
            return gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
        else if (GTK_IS_Q_SORT_FILTER_TREE_MODEL(model))
            return gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), &iter);
    }
    return QModelIndex();
}

static void
update_call_model_selection(GtkTreeSelection *selection, G_GNUC_UNUSED gpointer user_data)
{
    QModelIndex current = get_index_from_selection(selection);
    if (current.isValid())
        CallModel::instance()->selectionModel()->setCurrentIndex(current, QItemSelectionModel::ClearAndSelect);
    else
        CallModel::instance()->selectionModel()->clearCurrentIndex();
}

static void
call_selection_changed(GtkTreeSelection *selection, gpointer win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(win));

    /* get the current visible stack child */
    GtkWidget *old_call_view = gtk_stack_get_visible_child(GTK_STACK(priv->stack_call_view));

    QModelIndex idx = get_index_from_selection(selection);
    if (idx.isValid()) {
        QVariant state = CallModel::instance()->data(idx, static_cast<int>(Call::Role::LifeCycleState));
        GtkWidget *new_call_view = NULL;
        char* new_call_view_name = NULL;

        /* FIXME: change when fixed
         * switch(state.value<Call::LifeCycleState>()) { */
        Call::LifeCycleState lifecyclestate = (Call::LifeCycleState)state.toUInt();
        switch(lifecyclestate) {
            case Call::LifeCycleState::INITIALIZATION:
            case Call::LifeCycleState::FINISHED:
                new_call_view = incoming_call_view_new();
                incoming_call_view_set_call_info(INCOMING_CALL_VIEW(new_call_view), idx);
                /* use the pointer of the call as a unique name */
                new_call_view_name = g_strdup_printf("%p_incoming", (void *)CallModel::instance()->getCall(idx));
                break;
            case Call::LifeCycleState::PROGRESS:
                new_call_view = current_call_view_new();
                current_call_view_set_call_info(CURRENT_CALL_VIEW(new_call_view), idx);
                /* use the pointer of the call as a unique name */
                new_call_view_name = g_strdup_printf("%p_current", (void *)CallModel::instance()->getCall(idx));
                break;
            case Call::LifeCycleState::COUNT__:
                g_warning("LifeCycleState should never be COUNT");
                break;
        }

        gtk_stack_add_named(GTK_STACK(priv->stack_call_view), new_call_view, new_call_view_name);
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_call_view), new_call_view);
        g_free(new_call_view_name);

    } else {
        /* nothing selected in the call model, so show the default screen */

        /* TODO: replace stack paceholder view */
        gtk_stack_set_transition_type(GTK_STACK(priv->stack_call_view), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_call_view), DEFAULT_VIEW_NAME);
        gtk_stack_set_transition_type(GTK_STACK(priv->stack_call_view), GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);

    }

    /* check if we changed the visible child */
    GtkWidget *current_call_view = gtk_stack_get_visible_child(GTK_STACK(priv->stack_call_view));
    if (current_call_view != old_call_view && old_call_view != NULL) {
        /* if the previous child was a call view, then remove it from
         * the stack; removing it should destory it since there should not
         * be any other references to it */
        if (IS_INCOMING_CALL_VIEW(old_call_view) || IS_CURRENT_CALL_VIEW(old_call_view)) {
            gtk_container_remove(GTK_CONTAINER(priv->stack_call_view), old_call_view);
        }
    }
}

static void
call_state_changed(Call *call, gpointer win)
{
    g_debug("call state changed");
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(win));

    /* check if the call that changed state is the same as the selected call */
    QModelIndex idx_selected = CallModel::instance()->selectionModel()->currentIndex();

    if( idx_selected.isValid() && call == CallModel::instance()->getCall(idx_selected)) {
        g_debug("selected call state changed");
        /* check if we need to change the view */
        GtkWidget *old_call_view = gtk_stack_get_visible_child(GTK_STACK(priv->stack_call_view));
        GtkWidget *new_call_view = NULL;
        QVariant state = CallModel::instance()->data(idx_selected, static_cast<int>(Call::Role::LifeCycleState));

        /* check what the current state is vs what is displayed */

        /* FIXME: change when fixed
         * switch(state.value<Call::LifeCycleState>()) { */
        Call::LifeCycleState lifecyclestate = (Call::LifeCycleState)state.toUInt();
        switch(lifecyclestate) {
            case Call::LifeCycleState::INITIALIZATION:
                /* LifeCycleState cannot go backwards, so it should not be possible
                 * that the call is displayed as current (meaning that its in progress)
                 * but have the state 'initialization' */
                if (IS_CURRENT_CALL_VIEW(old_call_view))
                    g_warning("call displayed as current, but is in state of initialization");
                break;
            case Call::LifeCycleState::PROGRESS:
                if (IS_INCOMING_CALL_VIEW(old_call_view)) {
                    /* change from incoming to current */
                    new_call_view = current_call_view_new();
                    current_call_view_set_call_info(CURRENT_CALL_VIEW(new_call_view), idx_selected);
                    /* use the pointer of the call as a unique name */
                    char* new_call_view_name = NULL;
                    new_call_view_name = g_strdup_printf("%p_current", (void *)CallModel::instance()->getCall(idx_selected));
                    gtk_stack_add_named(GTK_STACK(priv->stack_call_view), new_call_view, new_call_view_name);
                    g_free(new_call_view_name);
                    gtk_stack_set_transition_type(GTK_STACK(priv->stack_call_view), GTK_STACK_TRANSITION_TYPE_SLIDE_UP);
                    gtk_stack_set_visible_child(GTK_STACK(priv->stack_call_view), new_call_view);
                    gtk_stack_set_transition_type(GTK_STACK(priv->stack_call_view), GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
                }
                break;
            case Call::LifeCycleState::FINISHED:
                /* do nothing, either call view is valid for this state */
                break;
            case Call::LifeCycleState::COUNT__:
                g_warning("LifeCycleState should never be COUNT");
                break;
        }

        /* check if we changed the visible child */
        GtkWidget *current_call_view = gtk_stack_get_visible_child(GTK_STACK(priv->stack_call_view));
        if (current_call_view != old_call_view && old_call_view != NULL) {
            /* if the previous child was a call view, then remove it from
             * the stack; removing it should destory it since there should not
             * be any other references to it */
            if (IS_INCOMING_CALL_VIEW(old_call_view) || IS_CURRENT_CALL_VIEW(old_call_view)) {
                gtk_container_remove(GTK_CONTAINER(priv->stack_call_view), old_call_view);
            }
        }
    }
}

static void
search_entry_placecall(G_GNUC_UNUSED GtkWidget *entry, gpointer win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(win));

    const gchar *number_entered = gtk_entry_get_text(GTK_ENTRY(priv->search_entry));

    if (number_entered && strlen(number_entered) > 0) {
        /* detect Ring hash */
        gboolean is_ring_hash = FALSE;
        if (strlen(number_entered) == 40) {
            is_ring_hash = TRUE;
            /* must be 40 chars long and alphanumeric */
            for (int i = 0; i < 40 && is_ring_hash; ++i) {
                if (!g_ascii_isalnum(number_entered[i]))
                    is_ring_hash = FALSE;
            }
        }

        QString number = QString{number_entered};

        if (is_ring_hash)
            number = "ring:" + number;

        g_debug("dialing to number: %s", number.toUtf8().constData());
        Call *call = CallModel::instance()->dialingCall();
        call->setDialNumber(number);
        call->performAction(Call::Action::ACCEPT);

        /* make this the currently selected call */
        QModelIndex idx = CallModel::instance()->getIndex(call);
        CallModel::instance()->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
    }
}

static void
call_history_item(GtkTreeView *tree_view,
                  GtkTreePath *path,
                  G_GNUC_UNUSED GtkTreeViewColumn *column,
                  G_GNUC_UNUSED gpointer user_data)
{
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);

    /* get iter */
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), &iter);

        QVariant contact_method = idx.data(static_cast<int>(Call::Role::ContactMethod));
        /* create new call */
        if (contact_method.value<ContactMethod*>()) {
            Call *call = CallModel::instance()->dialingCall();
            call->setDialNumber(contact_method.value<ContactMethod*>());
            call->performAction(Call::Action::ACCEPT);

            /* make this the currently selected call */
            QModelIndex call_idx = CallModel::instance()->getIndex(call);
            CallModel::instance()->selectionModel()->setCurrentIndex(call_idx, QItemSelectionModel::ClearAndSelect);
        } else
            g_warning("contact method is empty");
    }
}

static void
navbutton_contacts_toggled(GtkToggleButton *navbutton, RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    if (gtk_toggle_button_get_active(navbutton)) {

        const gchar *visible = gtk_stack_get_visible_child_name(GTK_STACK(priv->stack_contacts_history_presence));

        if (visible) {
            /* contacts is left of both history and presence, so always slide right to show it */
            gtk_stack_set_transition_type(GTK_STACK(priv->stack_contacts_history_presence), GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
            gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_contacts_history_presence), VIEW_CONTACTS);
        } else {
            g_warning("should always have a visible child in the nav stack");
            gtk_stack_set_transition_type(GTK_STACK(priv->stack_contacts_history_presence), GTK_STACK_TRANSITION_TYPE_NONE);
            gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_contacts_history_presence), VIEW_CONTACTS);
        }
    }
}

static void
navbutton_presence_toggled(GtkToggleButton *navbutton, RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    if (gtk_toggle_button_get_active(navbutton)) {

        const gchar *visible = gtk_stack_get_visible_child_name(GTK_STACK(priv->stack_contacts_history_presence));
        if (visible) {
            /* presence is right of both history and contacts, so always slide left to show it */
            gtk_stack_set_transition_type(GTK_STACK(priv->stack_contacts_history_presence), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
            gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_contacts_history_presence), VIEW_PRESENCE);
        } else {
            g_warning("should always have a visible child in the nav stack");
            gtk_stack_set_transition_type(GTK_STACK(priv->stack_contacts_history_presence), GTK_STACK_TRANSITION_TYPE_NONE);
            gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_contacts_history_presence), VIEW_PRESENCE);
        }
    }
}

static void
navbutton_history_toggled(GtkToggleButton *navbutton, RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    if (gtk_toggle_button_get_active(navbutton)) {

        const gchar *visible = gtk_stack_get_visible_child_name(GTK_STACK(priv->stack_contacts_history_presence));
        if (visible) {
            if (strcmp(visible, VIEW_CONTACTS) == 0) {
                /* history is right of contacts, so slide left to show it */
                gtk_stack_set_transition_type(GTK_STACK(priv->stack_contacts_history_presence), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
                gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_contacts_history_presence), VIEW_HISTORY);
            } else if (strcmp(visible, VIEW_PRESENCE) == 0) {
                /* history is left of presence, so slide right to show it */
                gtk_stack_set_transition_type(GTK_STACK(priv->stack_contacts_history_presence), GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
                gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_contacts_history_presence), VIEW_HISTORY);
            }
        } else {
            g_warning("should always have a visible child in the nav stack");
            gtk_stack_set_transition_type(GTK_STACK(priv->stack_contacts_history_presence), GTK_STACK_TRANSITION_TYPE_NONE);
            gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_contacts_history_presence), VIEW_HISTORY);
        }
    }
}

static gboolean
save_accounts(GtkWidget *working_dialog)
{
    /* save changes to accounts */
    AccountModel::instance()->save();
    /* save changes to codecs */
    for (int i = 0; i < AccountModel::instance()->rowCount(); i++) {
        QModelIndex idx = AccountModel::instance()->index(i, 0);
        AccountModel::instance()->getAccountByModelIndex(idx)->codecModel()->save();
    }

    if (working_dialog)
        gtk_widget_destroy(working_dialog);

    return G_SOURCE_REMOVE;
}

static void
settings_clicked(G_GNUC_UNUSED GtkButton *button, RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    /* toggle show settings */
    priv->show_settings = !priv->show_settings;

    /* check which view to show */
    if (priv->show_settings) {
        /* show settings */
        gtk_image_set_from_icon_name(GTK_IMAGE(priv->image_settings), "emblem-ok-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);

        gtk_widget_hide(priv->hbox_search);
        gtk_widget_show(priv->hbox_settings);

        gtk_stack_set_transition_type(GTK_STACK(priv->stack_main_view), GTK_STACK_TRANSITION_TYPE_SLIDE_UP);
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_main_view), priv->last_settings_view);
    } else {
        /* show working dialog in case save operation takes time */
        GtkWidget *working = ring_dialog_working(GTK_WIDGET(win), NULL);
        gtk_window_present(GTK_WINDOW(working));

        /* now save after the time it takes to transition back to the call view (400ms)
         * the save doesn't happen before the "working" dialog is presented
         * the timeout function should destroy the "working" dialog when done saving
         */
        g_timeout_add_full(G_PRIORITY_DEFAULT, 400, (GSourceFunc)save_accounts, working, NULL);

        /* show calls */
        gtk_image_set_from_icon_name(GTK_IMAGE(priv->image_settings), "emblem-system-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);

        gtk_widget_show(priv->hbox_search);
        gtk_widget_hide(priv->hbox_settings);

        gtk_stack_set_transition_type(GTK_STACK(priv->stack_main_view), GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN);
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), CALL_VIEW_NAME);

        /* make sure video preview is stopped, in case it was started */
        Video::PreviewManager::instance()->stopPreview();
    }
}

static void
show_video_settings(GtkToggleButton *navbutton, RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    if (gtk_toggle_button_get_active(navbutton)) {
        gtk_stack_set_transition_type(GTK_STACK(priv->stack_main_view), GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), VIDEO_SETTINGS_VIEW_NAME);
        priv->last_settings_view = priv->video_settings_view;
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

static gboolean
has_ring_account()
{
    /* check if a Ring account exists */
    int a_count = AccountModel::instance()->rowCount();
    for (int i = 0; i < a_count; ++i) {
        QModelIndex idx = AccountModel::instance()->index(i, 0);
        QVariant protocol = idx.data(static_cast<int>(Account::Role::Proto));
        if ((Account::Protocol)protocol.toUInt() == Account::Protocol::RING)
            return TRUE;
    }

    return FALSE;
}

static gboolean
create_ring_account(RingMainWindow *win)
{
    g_return_val_if_fail(IS_RING_MAIN_WINDOW(win), G_SOURCE_REMOVE);
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    /* create account and set UPnP enabled, as its not by default in the daemon */
    const gchar *alias = gtk_entry_get_text(GTK_ENTRY(priv->entry_alias));
    Account *account = AccountModel::instance()->add(alias, Account::Protocol::RING);
    account->setUpnpEnabled(TRUE);

    /* wait for hash to be generated to show the next view */
    priv->hash_updated = QObject::connect(
        account,
        &Account::changed,
        [=] (Account *a) {
            QString hash = a->username();
            if (!hash.isEmpty()) {
                /* set the hash */
                gtk_entry_set_text(GTK_ENTRY(priv->entry_hash), hash.toUtf8().constData());

                /* show the next accont creation view */
                gtk_stack_set_transition_type(GTK_STACK(priv->stack_main_view), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
                gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), CREATE_ACCOUNT_2_VIEW_NAME);

                /* select the hash text */
                gtk_widget_grab_focus(priv->entry_hash);
                gtk_editable_select_region(GTK_EDITABLE(priv->entry_hash), 0, -1);
            }
        }
    );

    account->performAction(Account::EditAction::SAVE);

    return G_SOURCE_REMOVE;
}

static void
alias_entry_changed(GtkEditable *entry, RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    const gchar *alias = gtk_entry_get_text(GTK_ENTRY(entry));
    if (strlen(alias) > 0) {
        /* enable "next" button */
        gtk_widget_set_sensitive(priv->button_account_creation_next, TRUE);
    } else {
        /* disable "next" button, as we require an alias */
        gtk_widget_set_sensitive(priv->button_account_creation_next, FALSE);
    }
}

static void
account_creation_next_clicked(G_GNUC_UNUSED GtkButton *button, RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    /* show/hide relevant widgets */
    gtk_widget_hide(priv->label_enter_alias);
    gtk_widget_hide(priv->entry_alias);
    gtk_widget_hide(priv->button_account_creation_next);
    gtk_widget_show(priv->label_generating_account);
    gtk_widget_show(priv->spinner_generating_account);

    /* now create account after a short timeout so that the the save doesn't
     * happen freeze the client before the widget changes happen;
     * the timeout function should then display the next step in account creation
     */
    g_timeout_add_full(G_PRIORITY_DEFAULT, 300, (GSourceFunc)create_ring_account, win, NULL);
}

static void
account_creation_done_clicked(G_GNUC_UNUSED GtkButton *button, RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    QObject::disconnect(priv->hash_updated);

    /* show the call view */
    gtk_stack_set_transition_type(GTK_STACK(priv->stack_main_view), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
    gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), CALL_VIEW_NAME);

    /* show the search bar and settings */
    gtk_widget_show(priv->hbox_search);
    gtk_widget_show(priv->ring_settings);

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
                        priv->account_creation_1,
                        CREATE_ACCOUNT_1_VIEW_NAME);

    gtk_stack_add_named(GTK_STACK(priv->stack_main_view),
                        priv->account_creation_2,
                        CREATE_ACCOUNT_2_VIEW_NAME);

    /* hide search bar and settings until account creation is complete */
    gtk_widget_hide(priv->hbox_search);
    gtk_widget_hide(priv->ring_settings);

    /* set ring logo */
    GError *error = NULL;
    GdkPixbuf* logo_ring = gdk_pixbuf_new_from_resource_at_scale("/cx/ring/RingGnome/ring-logo-blue",
                                                                  -1, 75, TRUE, &error);
    if (logo_ring == NULL) {
        g_debug("Could not load logo: %s", error->message);
        g_error_free(error);
    } else
        gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_ring_logo), logo_ring);

    /* style of alias and hash entry; give them a larger font */
    gtk_widget_override_font(priv->entry_alias, pango_font_description_from_string("15"));
    gtk_widget_override_font(priv->entry_hash, pango_font_description_from_string("monospace 15"));

    /* connect signals */
    g_signal_connect(priv->entry_alias, "changed", G_CALLBACK(alias_entry_changed), win);
    g_signal_connect(priv->button_account_creation_next, "clicked", G_CALLBACK(account_creation_next_clicked), win);
    g_signal_connect(priv->button_account_creation_done, "clicked", G_CALLBACK(account_creation_done_clicked), win);
    g_signal_connect(priv->entry_alias, "activate", G_CALLBACK(entry_alias_activated), win);
    g_signal_connect_swapped(priv->entry_hash, "activate", G_CALLBACK(gtk_button_clicked), priv->button_account_creation_done);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), CREATE_ACCOUNT_1_VIEW_NAME);
}

static void
show_ring_id(RingMainWindow *win, Account *account) {
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    /* display the ring id, if we found a ring account */
    if (account) {
        if (!account->username().isEmpty()) {
            gtk_entry_set_text(GTK_ENTRY(priv->entry_ring_id), account->username().toUtf8().constData());
        } else {
            g_warning("got ring account, but Ring id is empty");
            gtk_entry_set_text(GTK_ENTRY(priv->entry_ring_id), "");
            gtk_entry_set_placeholder_text(GTK_ENTRY(priv->entry_ring_id), "fetching Ring ID...");
        }
    } else {
        gtk_entry_set_text(GTK_ENTRY(priv->entry_ring_id), "");
        gtk_entry_set_placeholder_text(GTK_ENTRY(priv->entry_ring_id), "no Ring account found");
    }

}

static void
get_active_ring_account(RingMainWindow *win)
{
    /* get the users Ring account
     * if multiple accounts exist, get the first one which is registered,
     * if none, then the first one which is enabled,
     * if none, then the first one in the list of ring accounts
     */
    Account *registered_account = NULL;
    Account *enabled_account = NULL;
    Account *ring_account = NULL;
    int a_count = AccountModel::instance()->rowCount();
    for (int i = 0; i < a_count && !registered_account; ++i) {
        QModelIndex idx = AccountModel::instance()->index(i, 0);
        Account *account = AccountModel::instance()->getAccountByModelIndex(idx);
        if (account->protocol() == Account::Protocol::RING) {
            /* got RING account, check if active */
            if (account->isEnabled()) {
                /* got enabled account, check if connected */
                if (account->registrationState() == Account::RegistrationState::READY) {
                    /* got registered account, use this one */
                    registered_account = enabled_account = ring_account = account;
                    // g_debug("got registered account: %s", ring_account->alias().toUtf8().constData());
                } else {
                    /* not registered, but enabled, use if its the first one */
                    if (!enabled_account) {
                        enabled_account = ring_account = account;
                        // g_debug("got enabled ring accout: %s", ring_account->alias().toUtf8().constData());
                    }
                }
            } else {
                /* not enabled, but a Ring account, use if its the first one */
                if (!ring_account) {
                    ring_account = account;
                    // g_debug("got ring account: %s", ring_account->alias().toUtf8().constData());
                }
            }
        }
    }

    show_ring_id(win, ring_account);
}

static void
render_call_direction(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                      GtkCellRenderer *cell,
                      GtkTreeModel *tree_model,
                      GtkTreeIter *iter,
                      G_GNUC_UNUSED gpointer data)
{
    /* check if this is a top level item (the fuzzy date item),
     * in this case we don't want to show a call direction */
    gchar *render_direction = NULL;
    GtkTreeIter parent;
    if (gtk_tree_model_iter_parent(tree_model, &parent, iter)) {
        /* get direction and missed values */
        GValue value = G_VALUE_INIT;
        gtk_tree_model_get_value(tree_model, iter, 3, &value);
        Call::Direction direction = (Call::Direction)g_value_get_int(&value);
        g_value_unset(&value);

        gtk_tree_model_get_value(tree_model, iter, 4, &value);
        gboolean missed = g_value_get_boolean(&value);
        g_value_unset(&value);

        switch (direction) {
            case Call::Direction::INCOMING:
                if (missed)
                    render_direction = g_strdup_printf("<span fgcolor=\"red\" font=\"monospace\">&#8601;</span>");
                else
                    render_direction = g_strdup_printf("<span fgcolor=\"green\" font=\"monospace\">&#8601;</span>");
            break;
            case Call::Direction::OUTGOING:
                if (missed)
                    render_direction = g_strdup_printf("<span fgcolor=\"red\" font=\"monospace\">&#8599;</span>");
                else
                    render_direction = g_strdup_printf("<span fgcolor=\"green\" font=\"monospace\">&#8599;</span>");
            break;
        }
    }
    g_object_set(G_OBJECT(cell), "markup", render_direction, NULL);
    g_free(render_direction);
}

static void
copy_history_item(G_GNUC_UNUSED GtkWidget *item, GtkTreeView *treeview)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    QModelIndex idx = get_index_from_selection(selection);

    if (idx.isValid()) {
        GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

        const gchar* number = idx.data(static_cast<int>(Call::Role::Number)).toString().toUtf8().constData();
        gtk_clipboard_set_text(clip, number, -1);
    }
}

/* TODO: can't seem to delete just one item for now, add when supported in backend
 * static void
 * delete_history_item(G_GNUC_UNUSED GtkWidget *item, GtkTreeView *treeview)
 * {
 *     GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
 *     QModelIndex idx = get_index_from_selection(selection);
 *
 *     if (idx.isValid()) {
 *         g_debug("deleting history item");
 *         CategorizedHistoryModel::instance()->removeRow(idx.row(), idx.parent());
 *     }
 * }
 */

static gboolean
history_popup_menu(G_GNUC_UNUSED GtkWidget *widget, GdkEventButton *event, GtkTreeView *treeview)
{
    /* build popup menu when right clicking on history item
     * user should be able to copy the "number",
     * delete history item or all of the history,
     * and eventualy add the number to a contact
     */

    /* check for right click */
    if (event->button != BUTTON_RIGHT_CLICK || event->type != GDK_BUTTON_PRESS)
        return FALSE;

    GtkWidget *menu = gtk_menu_new();

    /* copy */
    GtkWidget *item = gtk_menu_item_new_with_mnemonic("_Copy");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(copy_history_item), treeview);

    /* TODO: delete history entry
     * gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
     * item = gtk_menu_item_new_with_mnemonic("_Delete entry");
     * gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
     * g_signal_connect(item, "activate", G_CALLBACK(delete_history_item), treeview);
     */

    /* show menu */
    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);

    return FALSE; /* continue to default handler */
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
    GdkPixbuf* image_ring = gdk_pixbuf_new_from_resource_at_scale("/cx/ring/RingGnome/ring-symbol-blue",
                                                                  -1, 24, TRUE, &error);
    if (image_ring == NULL) {
        g_debug("Could not load icon: %s", error->message);
        g_error_free(error);
    } else
        gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_ring), image_ring);

    /* ring menu */
    GtkBuilder *builder = gtk_builder_new_from_resource("/cx/ring/RingGnome/ringgearsmenu.ui");
    GMenuModel *menu = G_MENU_MODEL(gtk_builder_get_object(builder, "menu"));
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(priv->ring_menu), menu);
    g_object_unref(builder);

    /* settings icon */
    gtk_image_set_from_icon_name(GTK_IMAGE(priv->image_settings), "emblem-system-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);

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

    priv->video_settings_view = video_settings_view_new();
    gtk_stack_add_named(GTK_STACK(priv->stack_main_view), priv->video_settings_view, VIDEO_SETTINGS_VIEW_NAME);

    /* make the setting we will show first the active one */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->radiobutton_video_settings), TRUE);
    priv->last_settings_view = priv->video_settings_view;

    /* connect the settings button signals to switch settings views */
    g_signal_connect(priv->radiobutton_video_settings, "toggled", G_CALLBACK(show_video_settings), win);
    g_signal_connect(priv->radiobutton_account_settings, "toggled", G_CALLBACK(show_account_settings), win);

    /* call model */
    GtkQTreeModel *call_model;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    call_model = gtk_q_tree_model_new(CallModel::instance(), 4,
        Call::Role::Name, G_TYPE_STRING,
        Call::Role::Number, G_TYPE_STRING,
        Call::Role::Length, G_TYPE_STRING,
        Call::Role::State, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_call), GTK_TREE_MODEL(call_model));

    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", 0, NULL);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_call), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Duration", renderer, "text", 2, NULL);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_call), column);

    /* connect signals to and from UserActionModel to sync selection betwee
     * the QModel and the GtkTreeView */
    QObject::connect(
        CallModel::instance()->selectionModel(),
        &QItemSelectionModel::currentChanged,
        [=](const QModelIndex & current, const QModelIndex & previous) {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_call));

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

    GtkTreeSelection *call_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_call));
    g_signal_connect(call_selection, "changed", G_CALLBACK(update_call_model_selection), NULL);

    /* connect to call state changes to update relevant view(s) */
    QObject::connect(
        CallModel::instance(),
        &CallModel::callStateChanged,
        [=](Call* call, G_GNUC_UNUSED Call::State previousState) {
            call_state_changed(call, win);
        }
    );

    /* contacts view/model */
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *treeview_contacts = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_contacts), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), treeview_contacts);
    gtk_widget_show_all(scrolled_window);
    gtk_stack_add_named(GTK_STACK(priv->stack_contacts_history_presence),
                        scrolled_window,
                        VIEW_CONTACTS);

    /* history view/model */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *treeview_history = gtk_tree_view_new();
    /* make headers visible to allow column resizing */
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_history), TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), treeview_history);
    gtk_widget_show_all(scrolled_window);
    gtk_stack_add_named(GTK_STACK(priv->stack_contacts_history_presence),
                        scrolled_window,
                        VIEW_HISTORY);
    /* TODO: make this linked to the client settings so that the last shown view is the same on startup */
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_contacts_history_presence),
                                scrolled_window);


    /* sort the history in descending order by date */
    QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(CategorizedHistoryModel::instance());
    proxyModel->setSourceModel(CategorizedHistoryModel::instance());
    proxyModel->setSortRole(static_cast<int>(Call::Role::Date));
    proxyModel->sort(0,Qt::DescendingOrder);

    GtkQSortFilterTreeModel *history_model = gtk_q_sort_filter_tree_model_new(
        (QSortFilterProxyModel *)proxyModel,
        5,
        Qt::DisplayRole, G_TYPE_STRING,
        Call::Role::Number, G_TYPE_STRING,
        Call::Role::FormattedDate, G_TYPE_STRING,
        Call::Role::Direction, G_TYPE_INT,
        Call::Role::Missed, G_TYPE_BOOLEAN);
    gtk_tree_view_set_model( GTK_TREE_VIEW(treeview_history), GTK_TREE_MODEL(history_model) );

    /* name column, also used for call direction and fuzzy date for top level items */
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "Name");

    /* call direction */
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);

    /* display the call direction with arrows */
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_call_direction,
        NULL,
        NULL);

    /* name or time category column */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_attributes(column, renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_history), column);
    gtk_tree_view_column_set_resizable(column, TRUE);

    /* "number" column */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    column = gtk_tree_view_column_new_with_attributes("Number", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_history), column);
    gtk_tree_view_column_set_resizable(column, TRUE);

    /* date column */
    renderer = gtk_cell_renderer_text_new ();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    column = gtk_tree_view_column_new_with_attributes ("Date", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_history), column);
    gtk_tree_view_column_set_resizable(column, TRUE);

    /* expand the first row, which should be the most recent calls */
    gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview_history),
                             gtk_tree_path_new_from_string("0"),
                             FALSE);

    g_signal_connect(treeview_history, "row-activated", G_CALLBACK(call_history_item), NULL);
    g_signal_connect(treeview_history, "button-press-event", G_CALLBACK(history_popup_menu), treeview_history);

    /* presence view/model */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *treeview_presence = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_presence), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), treeview_presence);
    gtk_widget_show_all(scrolled_window);
    gtk_stack_add_named(GTK_STACK(priv->stack_contacts_history_presence),
                        scrolled_window,
                        VIEW_PRESENCE);

    /* connect signals to change the contacts/history/presence stack view */
    g_signal_connect(priv->radiobutton_contacts, "toggled", G_CALLBACK(navbutton_contacts_toggled), win);
    g_signal_connect(priv->radiobutton_history, "toggled", G_CALLBACK(navbutton_history_toggled), win);
    g_signal_connect(priv->radiobutton_presence, "toggled", G_CALLBACK(navbutton_presence_toggled), win);

    /* TODO: replace stack paceholder view */
    GtkWidget *placeholder_view = gtk_tree_view_new();
    gtk_widget_show(placeholder_view);
    gtk_stack_add_named(GTK_STACK(priv->stack_call_view), placeholder_view, DEFAULT_VIEW_NAME);

    /* connect signals */
    g_signal_connect(call_selection, "changed", G_CALLBACK(call_selection_changed), win);
    g_signal_connect(priv->button_placecall, "clicked", G_CALLBACK(search_entry_placecall), win);
    g_signal_connect(priv->search_entry, "activate", G_CALLBACK(search_entry_placecall), win);

    /* style of search entry */
    gtk_widget_override_font(priv->search_entry, pango_font_description_from_string("monospace 15"));

    /* connect to incoming call and focus */
    QObject::connect(
        CallModel::instance(),
        &CallModel::incomingCall,
        [=](Call* call) {
            CallModel::instance()->selectionModel()->setCurrentIndex(
                CallModel::instance()->getIndex(call), QItemSelectionModel::ClearAndSelect);
        }
    );

    /* display ring id by first getting the active ring account */
    gtk_widget_override_font(priv->entry_ring_id, pango_font_description_from_string("monospace"));
    gtk_widget_set_size_request(priv->entry_ring_id, 400, 35);
    get_active_ring_account(win);
    QObject::connect(
        AccountModel::instance(),
        &AccountModel::dataChanged,
        [=] () {
            /* check if the active ring account has changed,
             * eg: if it was deleted */
            get_active_ring_account(win);
        }
    );
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

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/ringmainwindow.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, treeview_call);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, stack_contacts_history_presence);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_contacts);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_history);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_presence);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, ring_menu);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, image_ring);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, ring_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, image_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, hbox_search);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, hbox_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, search_entry);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, stack_main_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, vbox_call_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, stack_call_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, button_placecall);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_audio_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_general_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_video_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_account_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, entry_ring_id);

    /* account creation */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, account_creation_1);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, image_ring_logo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, label_enter_alias);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, entry_alias);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, label_generating_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, spinner_generating_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, button_account_creation_next);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, account_creation_2);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, entry_hash);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, button_account_creation_done);
}

GtkWidget *
ring_main_window_new (GtkApplication *app)
{
    gpointer win = g_object_new(RING_MAIN_WINDOW_TYPE, "application", app, NULL);

    return (GtkWidget *)win;
}
