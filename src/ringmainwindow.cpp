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

#include "ringmainwindow.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "models/gtkqtreemodel.h"
#include <callmodel.h>
#include <call.h>
#include <QtCore/QItemSelectionModel>
#include "incomingcallview.h"
#include "currentcallview.h"
#include <string.h>
#include <contactmethod.h>
#include <QtCore/QSortFilterProxyModel>
#include "models/gtkqsortfiltertreemodel.h"
#include "accountview.h"
#include <accountmodel.h>
#include <audio/codecmodel.h>
#include "dialogs.h"
#include "mediasettingsview.h"
#include <video/previewmanager.h>
#include <personmodel.h>
#include "utils/drawing.h"
#include <memory>
#include <globalinstances.h>
#include "native/pixbufmanipulator.h"
#include "models/activeitemproxymodel.h"
#include <numbercompletionmodel.h>
#include "utils/calling.h"
#include "frequentcontactsview.h"
#include "contactsview.h"
#include <categorizedcontactmodel.h>
#include "historyview.h"
#include "utils/models.h"
#include "generalsettingsview.h"
#include "callsview.h"
#include "utils/accounts.h"

constexpr static const char* CALL_VIEW_NAME             = "calls";
constexpr static const char* CREATE_ACCOUNT_1_VIEW_NAME = "create1";
constexpr static const char* CREATE_ACCOUNT_2_VIEW_NAME = "create2";
constexpr static const char* GENERAL_SETTINGS_VIEW_NAME = "general";
constexpr static const char* AUDIO_SETTINGS_VIEW_NAME   = "audio";
constexpr static const char* MEDIA_SETTINGS_VIEW_NAME   = "media";
constexpr static const char* ACCOUNT_SETTINGS_VIEW_NAME = "accounts";
constexpr static const char* DEFAULT_VIEW_NAME          = "placeholder";
constexpr static const char* VIEW_CONTACTS              = "contacts";
constexpr static const char* VIEW_HISTORY               = "history";
constexpr static const char* VIEW_PRESENCE              = "presence";

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
    GtkWidget *combobox_history_sort;
    GtkWidget *combobox_contacts_sort;
    GtkWidget *vbox_left_pane;
    GtkWidget *vbox_contacts;
    GtkWidget *search_entry;
    GtkWidget *stack_main_view;
    GtkWidget *vbox_call_view;
    GtkWidget *stack_call_view;
    GtkWidget *button_placecall;
    GtkWidget *account_settings_view;
    GtkWidget *media_settings_view;
    GtkWidget *general_settings_view;
    GtkWidget *last_settings_view;
    GtkWidget *radiobutton_general_settings;
    GtkWidget *radiobutton_media_settings;
    GtkWidget *radiobutton_account_settings;
    GtkWidget *label_ring_id;

    QMetaObject::Connection selection_updated;

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

    /* allocd qmodels */
    ActiveItemProxyModel *q_contact_model;
    QSortFilterProxyModel *q_history_model;
    NumberCompletionModel *q_completion_model;
};

G_DEFINE_TYPE_WITH_PRIVATE(RingMainWindow, ring_main_window, GTK_TYPE_APPLICATION_WINDOW);

#define RING_MAIN_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RING_MAIN_WINDOW_TYPE, RingMainWindowPrivate))

static void
call_selection_changed(GtkTreeSelection *selection, gpointer win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(win));

    /* get the current visible stack child */
    GtkWidget *old_call_view = gtk_stack_get_visible_child(GTK_STACK(priv->stack_call_view));

    QModelIndex idx = get_index_from_selection(selection);
    if (idx.isValid()) {
        QVariant state =  idx.data(static_cast<int>(Call::Role::LifeCycleState));
        GtkWidget *new_call_view = NULL;
        char* new_call_view_name = NULL;

        switch(state.value<Call::LifeCycleState>()) {
            case Call::LifeCycleState::CREATION:
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
        switch(state.value<Call::LifeCycleState>()) {
            case Call::LifeCycleState::CREATION:
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

        Call *call = priv->q_completion_model->call();
        call->setDialNumber(number);
        call->performAction(Call::Action::ACCEPT);

        /* make this the currently selected call */
        QModelIndex idx = CallModel::instance()->getIndex(call);
        CallModel::instance()->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);

        /* move focus away from entry so that DTMF tones can be entered via the keyboard */
        gtk_widget_child_focus(GTK_WIDGET(win), GTK_DIR_TAB_FORWARD);
        /* clear the entry */
        gtk_entry_set_text(GTK_ENTRY(priv->search_entry), "");
    }
}

static gboolean
grab_focus_on_widget(GtkWidget *widget)
{
    gtk_widget_grab_focus(widget);
    return G_SOURCE_REMOVE;
}

static void
navbutton_contacts_toggled(G_GNUC_UNUSED GtkToggleButton *navbutton, RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_contacts_history_presence),
                                     VIEW_CONTACTS);

    /* if the stack transition is animated, it will take a few miliseconds;
     * we want to grab focus of the newly displayed treeview only after the
     * animation, to make sure we scroll to the top of the new view */
    g_timeout_add_full(G_PRIORITY_DEFAULT,
                       gtk_stack_get_transition_duration(GTK_STACK(priv->stack_contacts_history_presence)),
                       (GSourceFunc)grab_focus_on_widget,
                       gtk_stack_get_visible_child(GTK_STACK(priv->stack_contacts_history_presence)),
                       NULL);

    /* show the correct sorting combobox */
    gtk_widget_show(priv->combobox_contacts_sort);
    gtk_widget_hide(priv->combobox_history_sort);
}

static void
navbutton_presence_toggled(G_GNUC_UNUSED GtkToggleButton *navbutton, RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_contacts_history_presence),
                                     VIEW_PRESENCE);

    /* if the stack transition is animated, it will take a few miliseconds;
     * we want to grab focus of the newly displayed treeview only after the
     * animation, to make sure we scroll to the top of the new view */
    g_timeout_add_full(G_PRIORITY_DEFAULT,
                       gtk_stack_get_transition_duration(GTK_STACK(priv->stack_contacts_history_presence)),
                       (GSourceFunc)grab_focus_on_widget,
                       gtk_stack_get_visible_child(GTK_STACK(priv->stack_contacts_history_presence)),
                       NULL);

    /* show the correct sorting combobox */
    gtk_widget_hide(priv->combobox_contacts_sort);
    gtk_widget_hide(priv->combobox_history_sort);
}

static void
navbutton_history_toggled(G_GNUC_UNUSED GtkToggleButton *navbutton, RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_contacts_history_presence),
                                     VIEW_HISTORY);

    /* if the stack transition is animated, it will take a few miliseconds;
     * we want to grab focus of the newly displayed treeview only after the
     * animation, to make sure we scroll to the top of the new view */
    g_timeout_add_full(G_PRIORITY_DEFAULT,
                       gtk_stack_get_transition_duration(GTK_STACK(priv->stack_contacts_history_presence)),
                       (GSourceFunc)grab_focus_on_widget,
                       gtk_stack_get_visible_child(GTK_STACK(priv->stack_contacts_history_presence)),
                       NULL);

    /* show the correct sorting combobox */
    gtk_widget_hide(priv->combobox_contacts_sort);
    gtk_widget_show(priv->combobox_history_sort);
}

static gboolean
save_accounts(GtkWidget *working_dialog)
{
    /* save changes to accounts */
    AccountModel::instance()->save();

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

        /* make sure to start preview if we're showing the video settings */
        if (priv->last_settings_view == priv->media_settings_view)
            media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(priv->media_settings_view), TRUE);

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
        media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(priv->media_settings_view), FALSE);
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
        gtk_stack_set_transition_type(GTK_STACK(priv->stack_main_view), GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), GENERAL_SETTINGS_VIEW_NAME);
        priv->last_settings_view = priv->general_settings_view;
    }
}

static gboolean
create_ring_account(RingMainWindow *win)
{
    g_return_val_if_fail(IS_RING_MAIN_WINDOW(win), G_SOURCE_REMOVE);
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    /* create account and set UPnP enabled, as its not by default in the daemon */
    const gchar *alias = gtk_entry_get_text(GTK_ENTRY(priv->entry_alias));
    Account *account = AccountModel::instance()->add(alias, Account::Protocol::RING);
    account->setDisplayName(alias); // set the display name to the same as the alias
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
        g_clear_error(&error);
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
            QString hash = "<span fgcolor=\"black\">" + account->username() + "</span>";
            gtk_label_set_label(GTK_LABEL(priv->label_ring_id), hash.toUtf8().constData());
        } else {
            g_warning("got ring account, but Ring id is empty");
            gtk_label_set_label(GTK_LABEL(priv->label_ring_id),
                                g_strdup_printf("<span fgcolor=\"gray\">%s</span>",
                                                _("fetching RingID...")));
        }
    } else {
        gtk_label_set_label(GTK_LABEL(priv->label_ring_id),
                            g_strdup_printf("<span fgcolor=\"gray\">%s</span>",
                                            _("no Ring account")));
    }
}

static void
search_entry_text_changed(GtkEditable *search_entry, RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    /* get the text from the entry */
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(search_entry));

    if (text && strlen(text) > 0) {
        /* edit the the dialing call (or create a new one) */
        if (auto call = CallModel::instance()->dialingCall()) {
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
    QModelIndex idx = get_qidx_from_filter_model(GTK_TREE_MODEL_FILTER(model), iter);
    if (idx.isValid()) {
        QVariant name = idx.sibling(idx.row(), 1).data(Qt::DisplayRole);
        gchar *text = g_strdup_printf("<span font=\"12\" weight=\"bold\">%s</span>",
                                      name.value<QString>().toUtf8().constData());

        g_object_set(G_OBJECT(cell), "markup", text, NULL);
        g_free(text);
        return;
    }

    g_object_set(G_OBJECT(cell), "markup", NULL, NULL);
}

static void
autocompletion_number_render(G_GNUC_UNUSED GtkCellLayout *cell_layout,
                             GtkCellRenderer *cell,
                             GtkTreeModel *model,
                             GtkTreeIter *iter,
                             G_GNUC_UNUSED gpointer user_data)
{
    QModelIndex idx = get_qidx_from_filter_model(GTK_TREE_MODEL_FILTER(model), iter);
    if (idx.isValid()) {
        QVariant uri = idx.data(Qt::DisplayRole);
        gchar *text = g_strdup_printf("<span font=\"12\">%s</span>",
                                      uri.value<QString>().toUtf8().constData());

        g_object_set(G_OBJECT(cell), "markup", text, NULL);
        g_free(text);
        return;
    }

    g_object_set(G_OBJECT(cell), "markup", NULL, NULL);
}

static void
autocompletion_account_render(G_GNUC_UNUSED GtkCellLayout *cell_layout,
                              GtkCellRenderer *cell,
                              GtkTreeModel *model,
                              GtkTreeIter *iter,
                              G_GNUC_UNUSED gpointer user_data)
{
    QModelIndex idx = get_qidx_from_filter_model(GTK_TREE_MODEL_FILTER(model), iter);
    if (idx.isValid()) {
        QVariant alias = idx.sibling(idx.row(), 2).data(Qt::DisplayRole);
        gchar *text = g_strdup_printf("<span font=\"12\" color=\"gray\">%s</span>",
                                      alias.value<QString>().toUtf8().constData());

        g_object_set(G_OBJECT(cell), "markup", text, NULL);
        g_free(text);
        return;
    }

    g_object_set(G_OBJECT(cell), "markup", NULL, NULL);
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
        priv->q_completion_model->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
        Call *call = priv->q_completion_model->call();
        priv->q_completion_model->callSelectedNumber();

        /* make this the currently selected call */
        QModelIndex idx = CallModel::instance()->getIndex(call);
        CallModel::instance()->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);

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
    QItemSelectionModel *selection = CallModel::instance()->selectionModel();
    QModelIndex idx = selection->currentIndex();
    if (!idx.isValid())
        return GDK_EVENT_PROPAGATE;

    /* make sure that the selected call is in progress */
    Call *call = CallModel::instance()->getCall(idx);
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
                                                                  -1, 24, TRUE, &error);
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

    /* calls view */
    GtkWidget *calls_view = calls_view_new();
    gtk_box_pack_start(GTK_BOX(priv->vbox_left_pane),
                       calls_view,
                       FALSE, TRUE, 0);
    gtk_box_reorder_child(GTK_BOX(priv->vbox_left_pane), calls_view, 0);

    /* connect to call state changes to update relevant view(s) */
    QObject::connect(
        CallModel::instance(),
        &CallModel::callStateChanged,
        [=](Call* call, G_GNUC_UNUSED Call::State previousState) {
            call_state_changed(call, win);
        }
    );

    /* frequent contacts view */
    GtkWidget *frequent_view = frequent_contacts_view_new();
    gtk_box_pack_start(GTK_BOX(priv->vbox_contacts),
                       frequent_view,
                       FALSE, TRUE, 0);
    gtk_box_reorder_child(GTK_BOX(priv->vbox_contacts), frequent_view, 0);

    /* contacts view */
    GtkWidget *contacts_view = contacts_view_new();
    gtk_stack_add_named(GTK_STACK(priv->stack_contacts_history_presence),
                        contacts_view,
                        VIEW_CONTACTS);
    gtk_combo_box_set_qmodel(GTK_COMBO_BOX(priv->combobox_contacts_sort),
                             (QAbstractItemModel *)CategorizedContactModel::SortedProxy::instance()->categoryModel(),
                             CategorizedContactModel::SortedProxy::instance()->categorySelectionModel());

    /* history view */
    GtkWidget *history_view = history_view_new();
    gtk_stack_add_named(GTK_STACK(priv->stack_contacts_history_presence),
                        history_view,
                        VIEW_HISTORY);
    gtk_combo_box_set_qmodel(GTK_COMBO_BOX(priv->combobox_history_sort),
                             (QAbstractItemModel *)CategorizedHistoryModel::SortedProxy::instance()->categoryModel(),
                             CategorizedHistoryModel::SortedProxy::instance()->categorySelectionModel());

    /* presence view/model */
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
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

    /* TODO: make this linked to the client settings so that the last shown view is the same on startup */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->radiobutton_contacts), TRUE);
    gtk_widget_show(priv->combobox_contacts_sort);
    gtk_widget_hide(priv->combobox_history_sort);

    /* TODO: replace stack paceholder view */
    GtkWidget *placeholder_view = gtk_tree_view_new();
    gtk_widget_show(placeholder_view);
    gtk_stack_add_named(GTK_STACK(priv->stack_call_view), placeholder_view, DEFAULT_VIEW_NAME);

    /* connect signals */
    GtkTreeSelection *call_selection = calls_view_get_selection(CALLS_VIEW(calls_view));
    g_signal_connect(call_selection, "changed", G_CALLBACK(call_selection_changed), win);
    g_signal_connect(priv->button_placecall, "clicked", G_CALLBACK(search_entry_placecall), win);
    g_signal_connect(priv->search_entry, "activate", G_CALLBACK(search_entry_placecall), win);

    /* style of search entry */
    gtk_widget_override_font(priv->search_entry, pango_font_description_from_string("15"));

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
        Qt::DisplayRole, G_TYPE_STRING);

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
        CallModel::instance(),
        &CallModel::incomingCall,
        [=](Call* call) {
            CallModel::instance()->selectionModel()->setCurrentIndex(
                CallModel::instance()->getIndex(call), QItemSelectionModel::ClearAndSelect);
        }
    );

    /* display ring id by first getting the active ring account */
    gtk_widget_override_font(priv->label_ring_id, pango_font_description_from_string("monospace"));
    show_ring_id(win, get_active_ring_account());
    QObject::connect(
        AccountModel::instance(),
        &AccountModel::dataChanged,
        [win] () {
            /* check if the active ring account has changed,
             * eg: if it was deleted */
            show_ring_id(win, get_active_ring_account());
        }
    );

    /* react to digit key press events */
    g_signal_connect(win, "key-press-event", G_CALLBACK(dtmf_pressed), NULL);
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

    delete priv->q_contact_model;
    delete priv->q_history_model;
    delete priv->q_completion_model;

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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, vbox_contacts);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, stack_contacts_history_presence);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_contacts);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_history);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_presence);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, combobox_history_sort);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, combobox_contacts_sort);
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_general_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_media_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_account_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, label_ring_id);

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
