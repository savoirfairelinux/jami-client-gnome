/*
 *  Copyright (C) 2015-2017 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Guillaume Roguew <guillaume.roguez@savoirfairelinux.com>
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
#include <iostream>

// GTK+ related
#include <glib/gi18n.h>

// Qt
#include <QtCore/QItemSelectionModel>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QDateTime>

// LRC
#include <callmodel.h>
#include <call.h>
#include <contactmethod.h>
#include <accountmodel.h>
#include <codecmodel.h>
#include <video/previewmanager.h>
#include <personmodel.h>
#include <globalinstances.h>
#include <categorizedcontactmodel.h>
#include <recentmodel.h>
#include <profilemodel.h>
#include <profile.h>
#include <phonedirectorymodel.h>
#include <availableaccountmodel.h>
#include <contactrequest.h>
#include <pendingcontactrequestmodel.h>

// Ring client
#include "models/gtkqtreemodel.h"
#include "incomingcallview.h"
#include "currentcallview.h"
#include "models/gtkqtreemodel.h"
#include "accountview.h"
#include "dialogs.h"
#include "mediasettingsview.h"
#include "utils/drawing.h"
#include "native/pixbufmanipulator.h"
#include "models/activeitemproxymodel.h"
#include "utils/calling.h"
#include "utils/models.h"
#include "generalsettingsview.h"
#include "utils/accounts.h"
#include "ringwelcomeview.h"
#include "accountmigrationview.h"
#include "accountcreationwizard.h"
#include "conversationsview.h"
#include "chatview.h"
#include "avatarmanipulation.h"
#include "utils/files.h"
#include "pendingcontactrequests.h"
#include "contactrequestcontentview.h"

//TODO move model initialization before
#include <conversationmodel.h>
#include <newcallmodel.h>
#include <contactmodel.h>
#include <availableaccountmodel.h>
#include <lrc.h>
#include <newaccountmodel.h>
#include "accountinfocontainer.h"

#include <iostream>

static constexpr const char* CALL_VIEW_NAME             = "calls";
static constexpr const char* ACCOUNT_CREATION_WIZARD_VIEW_NAME = "account-creation-wizard";
static constexpr const char* ACCOUNT_MIGRATION_VIEW_NAME       = "account-migration-view";
static constexpr const char* GENERAL_SETTINGS_VIEW_NAME = "general";
static constexpr const char* AUDIO_SETTINGS_VIEW_NAME   = "audio";
static constexpr const char* MEDIA_SETTINGS_VIEW_NAME   = "media";
static constexpr const char* ACCOUNT_SETTINGS_VIEW_NAME = "accounts";
static constexpr const char* DEFAULT_VIEW_NAME          = "welcome";
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
    GtkWidget *vbox_left_pane;
    GtkWidget *search_entry;
    GtkWidget *stack_main_view;
    GtkWidget *vbox_call_view;
    GtkWidget *frame_call;
    GtkWidget *welcome_view;
    GtkWidget *button_new_conversation  ;
    GtkWidget *account_settings_view;
    GtkWidget *media_settings_view;
    GtkWidget *general_settings_view;
    GtkWidget *last_settings_view;
    GtkWidget *radiobutton_general_settings;
    GtkWidget *radiobutton_media_settings;
    GtkWidget *radiobutton_account_settings;
    GtkWidget *account_creation_wizard;
    GtkWidget *account_migration_view;
    GtkWidget *spinner_lookup;
    GtkWidget *combobox_account_selector;
    GtkWidget *treeview_contact_requests;
    GtkWidget *scrolled_window_contact_requests;
    GtkWidget *contact_request_view;
    GtkWidget *image_contact_requests_list;

    /* Pending ring usernames lookup for the search entry */
    QMetaObject::Connection username_lookup;

    /* The webkit_chat_container is created once, then reused for all chat views */
    GtkWidget *webkit_chat_container;

    QMetaObject::Connection selected_item_changed;
    QMetaObject::Connection selected_call_over;
    QMetaObject::Connection account_request_added;
    QMetaObject::Connection account_request_accepted;
    QMetaObject::Connection account_request_discarded;
    QMetaObject::Connection account_request_;

    gboolean   show_settings;

    /* fullscreen */
    gboolean is_fullscreen;

    GSettings *settings;

    AccountInfoContainer* accountInfoContainer_;
    lrc::account::Info& accountInfo_;
    std::unique_ptr<lrc::Lrc> lrc_;
};

G_DEFINE_TYPE_WITH_PRIVATE(RingMainWindow, ring_main_window, GTK_TYPE_APPLICATION_WINDOW);

#define RING_MAIN_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RING_MAIN_WINDOW_TYPE, RingMainWindowPrivate))

static gboolean selection_changed(RingMainWindow *win);

static WebKitChatContainer*
get_webkit_chat_container(RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);
    if (!priv->webkit_chat_container)
    {
        priv->webkit_chat_container = webkit_chat_container_new();

        //We don't want it to be deleted, ever.
        g_object_ref(priv->webkit_chat_container);
    }
    return WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container);
}

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
video_double_clicked(RingMainWindow *self)
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
set_pending_contact_request_tab_icon(const Account* account, RingMainWindow* self)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));
    bool is_ring = account && account->protocol() == Account::Protocol::RING;
    gtk_widget_set_visible(priv->scrolled_window_contact_requests, is_ring);

    if (not is_ring)
        return;

    gtk_image_set_from_resource(GTK_IMAGE(priv->image_contact_requests_list),
        (account->pendingContactRequestModel()->rowCount())
        ? "/cx/ring/RingGnome/contact_requests_list_with_notification"
        : "/cx/ring/RingGnome/contact_requests_list");
};

static void
hide_view_clicked(RingMainWindow *self)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    /* clear selection */
    auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_conversations));
    gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_conversations));
    auto selection_contact_request = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_contact_requests));
    gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_contact_request));

}

static void
change_view(RingMainWindow *self, GtkWidget* old, lrc::conversation::Info conversation, GType type)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    leave_full_screen(self);
    gtk_container_remove(GTK_CONTAINER(priv->frame_call), old);

    GtkWidget *new_view = nullptr;

    QObject::disconnect(priv->selected_item_changed);
    QObject::disconnect(priv->selected_call_over);

    if (g_type_is_a(INCOMING_CALL_VIEW_TYPE, type)) {
        // TODO
    } else if (g_type_is_a(CURRENT_CALL_VIEW_TYPE, type)) {
        // TODO
    } else if (g_type_is_a(CHAT_VIEW_TYPE, type)) {
        new_view = chat_view_new(get_webkit_chat_container(self), priv->accountInfoContainer_, conversation);
        g_signal_connect_swapped(new_view, "hide-view-clicked", G_CALLBACK(hide_view_clicked), self);
    } else if (g_type_is_a(CONTACT_REQUEST_CONTENT_VIEW_TYPE, type)) {
        // TODO
    } else {
        // TODO change to first conversation?
        new_view = priv->welcome_view;
    }

    gtk_container_add(GTK_CONTAINER(priv->frame_call), new_view);
    gtk_widget_show(new_view);
}

/* TODO REMOVE
static void
change_view(RingMainWindow *self, GtkWidget* old, QObject *object, GType type)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    leave_full_screen(self);
    gtk_container_remove(GTK_CONTAINER(priv->frame_call), old);

    GtkWidget *new_view = nullptr;

    QObject::disconnect(priv->selected_item_changed);
    QObject::disconnect(priv->selected_call_over);

    if (g_type_is_a(INCOMING_CALL_VIEW_TYPE, type)) {
        if (auto call = qobject_cast<Call *>(object)) {
            new_view = incoming_call_view_new(call, get_webkit_chat_container(self));
            priv->selected_item_changed = QObject::connect(
                call,
                &Call::lifeCycleStateChanged,
                [self] (Call::LifeCycleState, Call::LifeCycleState)
                {g_idle_add((GSourceFunc)selection_changed, self);}
            );
            priv->selected_call_over = QObject::connect(
                call,
                &Call::isOver,
                [self] ()
                {g_idle_add((GSourceFunc)selection_changed, self);}
            );
        } else
            g_warning("Trying to display a view of type IncomingCallView, but the object is not of type Call");
    } else if (g_type_is_a(CURRENT_CALL_VIEW_TYPE, type)) {
        if (auto call = qobject_cast<Call *>(object)) {
            new_view = current_call_view_new(call, get_webkit_chat_container(self));
            g_signal_connect_swapped(new_view, "video-double-clicked", G_CALLBACK(video_double_clicked), self);
            priv->selected_item_changed = QObject::connect(
                call,
                &Call::lifeCycleStateChanged,
                [self] (Call::LifeCycleState, Call::LifeCycleState)
                {g_idle_add((GSourceFunc)selection_changed, self);}
            );
            priv->selected_call_over = QObject::connect(
                call,
                &Call::isOver,
                [self] ()
                {g_idle_add((GSourceFunc)selection_changed, self);}
            );
        } else
            g_warning("Trying to display a view of type CurrentCallView, but the object is not of type Call");
    } else if (g_type_is_a(CHAT_VIEW_TYPE, type)) {
        if (auto person = qobject_cast<Person *>(object)) {
            new_view = chat_view_new_person(get_webkit_chat_container(self), person);
            g_signal_connect_swapped(new_view, "hide-view-clicked", G_CALLBACK(hide_view_clicked), self);

            /* connect to the Person's callAdded signal, because we want to switch to the call view
             * in this case * /
            priv->selected_item_changed = QObject::connect(
                person,
                &Person::callAdded,
                [self] (Call*)
                {g_idle_add((GSourceFunc)selection_changed, self);}
            );
        } else if (auto cm = qobject_cast<ContactMethod *>(object)) {
            new_view = chat_view_new_cm(get_webkit_chat_container(self), cm);
            g_signal_connect_swapped(new_view, "hide-view-clicked", G_CALLBACK(hide_view_clicked), self);

            /* connect to the ContactMethod's callAdded signal, because we want to switch to the
             * call view in this case * /
            priv->selected_item_changed = QObject::connect(
                cm,
                &ContactMethod::callAdded,
                [self] (Call*)
                {g_idle_add((GSourceFunc)selection_changed, self);}
            );
        } else {
            g_warning("Trying to display a veiw of type ChatView, but the object is neither a Person nor a ContactMethod");
        }
    } else if (g_type_is_a(CONTACT_REQUEST_CONTENT_VIEW_TYPE, type)) {
        if (auto contact_request = qobject_cast<ContactRequest *>(object)) {
            new_view = contact_request_content_view_new(contact_request);
            g_signal_connect_swapped(new_view, "hide-view-clicked", G_CALLBACK(hide_view_clicked), self);
        }
    } else {
        // display the welcome view
        new_view = priv->welcome_view;
    }

    gtk_container_add(GTK_CONTAINER(priv->frame_call), new_view);
    gtk_widget_show(new_view);
}*/

/** TODO REMOVE
 * This function determines which view to display in the right panel of the main window based on
 * which item is selected in one of the 3 contact list views (Conversations, Contacts, History). The
 * possible views ares:
 * - incoming call view
 * - current call view
 * - chat view
 * - contact request content view
 * - welcome view (if no valid item is selected)
 *
 * There should never be a conflict of which item should be displayed (ie: which item is selected),
 * as there should only ever be one selection at a time in the 3 views except in the case that the
 * same item is selected in more than one view (see the compare_treeview_selection() function).
 *
 * This function could be called from a g_idle source, so it returns a boolean to remove itself from
 * being called again. The boolean doesn't reflect success nor failure.
 * /
static gboolean
selection_changed(RingMainWindow *win)
{

    g_return_val_if_fail(IS_RING_MAIN_WINDOW(win), G_SOURCE_REMOVE);
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(win));

    auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));

    /* if we're showing the settings, then we need to make sure to remove any possible ongoing call
     * view so that we don't have more than one VideoWidget at a time  * /
    if (priv->show_settings) {
        if (old_view != priv->welcome_view) {
            leave_full_screen(win);
            gtk_container_remove(GTK_CONTAINER(priv->frame_call), old_view);
            gtk_container_add(GTK_CONTAINER(priv->frame_call), priv->welcome_view);
            gtk_widget_show(priv->welcome_view);
        }
        return G_SOURCE_REMOVE;
    }

    /* there should only be one item selected at a time, get which one is selected * /
    auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_conversations));
    auto selection_contacts = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_contacts));
    auto selection_history = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_history));
    auto selection_contact_request = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_contact_requests));

    GtkTreeModel *model = nullptr;
    GtkTreeIter iter;
    QModelIndex idx;

    if (gtk_tree_selection_get_selected(selection_conversations, &model, &iter)) {
        idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
    } else if (gtk_tree_selection_get_selected(selection_contacts, &model, &iter)) {
        idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
    } else if (gtk_tree_selection_get_selected(selection_history, &model, &iter)) {
        idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
    } else if (gtk_tree_selection_get_selected(selection_contact_request, &model, &iter)) {
        idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
    }

    /* check which object type is selected * /
    auto type = idx.data(static_cast<int>(Ring::Role::ObjectType));
    auto object = idx.data(static_cast<int>(Ring::Role::Object));

    if (idx.isValid() && type.isValid() && object.isValid()) {

        /* we prioritize the views in the following order:
         *  - call view (if there is an ongoing call associated with the item)
         *  - chat view built from Person
         *  - chat view built from ContactMethod
         *  - contact request content view built from ContactRequest
         * /

        Person *person = nullptr;
        ContactMethod *cm = nullptr;
        Call *call = nullptr;
        ContactRequest *contact_request = nullptr;

        /* use the RecentModel to see if there are any ongoing calls associated with the selected item * /
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
                 * the associated Person or ContactMethod * /
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
            case Ring::ObjectType::ContactRequest:
            {
                contact_request = object.value<ContactRequest *>();
            }
            break;
            case Ring::ObjectType::Media:
            case Ring::ObjectType::Certificate:
            case Ring::ObjectType::COUNT__:
            // nothing to do
            break;
        }

        /* we prioritize showing the call view * /
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
                        change_view(win, old_view, call, INCOMING_CALL_VIEW_TYPE);
                    break;
                case Call::LifeCycleState::PROGRESS:
                    // check if we're already displaying this call
                    if (IS_CURRENT_CALL_VIEW(old_view))
                        current_call = current_call_view_get_call(CURRENT_CALL_VIEW(old_view));
                    if (current_call != call)
                        change_view(win, old_view, call, CURRENT_CALL_VIEW_TYPE);
                    break;
                case Call::LifeCycleState::COUNT__:
                    g_warning("LifeCycleState should never be COUNT");
                    break;
            }

        } else if (person) {
            /* show chat view constructed from Person object * /

            // check if we're already displaying the chat view for this person
            Person *current_person = nullptr;
            if (IS_CHAT_VIEW(old_view))
                current_person = chat_view_get_person(CHAT_VIEW(old_view));

            if (current_person != person)
                change_view(win, old_view, person, CHAT_VIEW_TYPE);
        } else if (cm) {
            /* show chat view constructed from CM * /

            // check if we're already displaying the chat view for this cm
            ContactMethod *current_cm = nullptr;
            if (IS_CHAT_VIEW(old_view))
                current_cm = chat_view_get_cm(CHAT_VIEW(old_view));

            if (current_cm != cm)
                change_view(win, old_view, cm, CHAT_VIEW_TYPE);

        } else if (contact_request) {
            /* show the contact request page * /
            change_view(win, old_view, contact_request, CONTACT_REQUEST_CONTENT_VIEW_TYPE);

        } else {
            /* not a supported object type, display the welcome view * /
            if (old_view != priv->welcome_view)
                change_view(win, old_view, nullptr, RING_WELCOME_VIEW_TYPE);
        }
    } else {
        /* selection isn't valid, display the welcome view * /
        if (old_view != priv->welcome_view)
            change_view(win, old_view, nullptr, RING_WELCOME_VIEW_TYPE);
    }

    /* we force to check the pending contact request status * /
    if (old_view != priv->welcome_view)
        change_view(win, old_view);

    return G_SOURCE_REMOVE;
}*/

static void
activate_contact_method(RingMainWindow *self, URI uri, Account *account)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    auto cm = PhoneDirectoryModel::instance().getNumber(uri, account);

    if (g_settings_get_boolean(priv->settings, "search-entry-places-call")) {
        place_new_call(cm);

        /* move focus away from entry so that DTMF tones can be entered via the keyboard */
        gtk_widget_child_focus(GTK_WIDGET(self), GTK_DIR_TAB_FORWARD);
    } else {
        // if its a new CM, bring it to the top
        if (cm->lastUsed() == 0)
            cm->setLastUsed(QDateTime::currentDateTime().toTime_t());

        // select cm
        RecentModel::instance().selectionModel()->setCurrentIndex(
            RecentModel::instance().getIndex(cm),
            QItemSelectionModel::ClearAndSelect
        );
    }
    gtk_entry_set_text(GTK_ENTRY(priv->search_entry), "");
}

static void
lookup_username(RingMainWindow *self, URI uri, Account *account)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    gtk_widget_show(priv->spinner_lookup);
    gtk_spinner_start(GTK_SPINNER(priv->spinner_lookup));
    gtk_entry_set_text(GTK_ENTRY(priv->search_entry), "");

    // we need to strip the Scheme section, or else some will contain "sip:"
    QString querry = uri.format(URI::Section::USER_INFO|URI::Section::HOSTNAME|URI::Section::PORT);

    /* disconect previous lookup */
    QObject::disconnect(priv->username_lookup);

    /* connect new lookup */
    priv->username_lookup = QObject::connect(
        &NameDirectory::instance(),
        &NameDirectory::registeredNameFound,
        [self, priv, querry] (Account* account, NameDirectory::LookupStatus status, const QString& address, const QString& name) {

            g_debug("lookup results for %s is %s",
                querry.toUtf8().constData(),
                name.toUtf8().constData()
            );

            if ( name.compare(querry, Qt::CaseInsensitive) != 0 ) {
                g_debug("not from the same lookup match");
                return; // doesn't match
            }

            gtk_entry_set_text(GTK_ENTRY(priv->search_entry), "");

            switch(status)
            {
                case NameDirectory::LookupStatus::SUCCESS:
                {
                    URI result = URI("ring:" + address);
                    activate_contact_method(self, result, account);
                    break;
                }
                case NameDirectory::LookupStatus::INVALID_NAME:
                {
                    //Can't be a ring username, could be something else.
                    auto dialog = gtk_message_dialog_new(
                        GTK_WINDOW(self),
                        GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_ERROR,
                        GTK_BUTTONS_CLOSE,
                        _("Invalid Ring username: %s"),
                        name.toUtf8().constData()
                    );

                    gtk_dialog_run (GTK_DIALOG (dialog));
                    gtk_widget_destroy (dialog);
                    break;
                }
                case NameDirectory::LookupStatus::ERROR:
                {
                    auto dialog = gtk_message_dialog_new(
                        GTK_WINDOW(self),
                        GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_ERROR,
                        GTK_BUTTONS_CLOSE,
                        _("Error resolving username, nameserver is possibly unreachable")
                    );

                    gtk_dialog_run(GTK_DIALOG (dialog));
                    gtk_widget_destroy(dialog);
                    break;
                }
                case NameDirectory::LookupStatus::NOT_FOUND:
                {
                    auto dialog = gtk_message_dialog_new(
                        GTK_WINDOW(self),
                        GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_ERROR,
                        GTK_BUTTONS_CLOSE,
                        _("Could not resolve Ring username: %s"),
                        name.toUtf8().constData()
                    );

                    gtk_dialog_run(GTK_DIALOG (dialog));
                    gtk_widget_destroy(dialog);
                    break;
                }
            }
            gtk_spinner_stop(GTK_SPINNER(priv->spinner_lookup));
            gtk_widget_hide(priv->spinner_lookup);
        }
    );

    /* lookup */
    NameDirectory::instance().lookupName(account, "", querry);
}

/**
 * This is called when someone activates the search entry (clicks enter or on the button).
 * At this point the list is already filtered but on clicking enter we want to either contact a new
 * number or perform a search on the name service. Here we must decide which one the user wants.
 * We'll use the following rules to decide:
 *   * if a user explicitly specifies the scheme ("sip:" or "ring:") then we always use a
 *     corresponding account type (use currentDefaultAccount() with the scheme type)
 *   * if a user doesn't specify the scheme, then we decide based on the user selected account; if
 *     SIP account then we assume its a SIP URI; if RING account then we check if its a ringID,
 *     otherwise we perform a name lookup.
 *
 * Additionally, we need to make sure that the CM found/created either has the same account set as
 * selected by the user, no account set, or else we need to switch the user selected account to the
 * account of the CM (eg: in the case we're switching protocols), or else the item will not be
 * visible in the Converstations list to the user. This is accomplished via the use of
 * currentDefaultAccount().
 */
static void
search_entry_activated(RingMainWindow *self)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    /*TODO REMOVE URI uri = URI(querry);

    // nothing to do if querry is empty or just whitespace
    if (uri.isEmpty()) return;

    /* get account to use * /
    Account *account = nullptr;
    if (uri.schemeType() != URI::SchemeType::NONE) {
        /* explicit SIP, SIPS, or RING; make sure account is of right type * /
        account = AvailableAccountModel::instance().currentDefaultAccount(uri.schemeType());
        if (!account) {
            const gchar *type = uri.schemeType() == URI::SchemeType::RING ? "RING" : "SIP";
            g_warning("entered %s uri, but no active %s accounts", type, type);
        }
    } else {
        /* just take the user selected account * /
        account = AvailableAccountModel::instance().currentDefaultAccount();
    }

    if (!account) {
        g_critical("could not get an account for uri: %s", uri.full().toUtf8().constData());
        return;
    }

    /* if RING and not RingID, perform lookup * /
    if (account->protocol() == Account::Protocol::RING &&
        uri.protocolHint() != URI::ProtocolHint::RING)
    {
        lookup_username(self, uri, account);
    } else {
        /* no lookup, simply use the URI as is * /
        activate_contact_method(self, uri, account);
    }*/
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
settings_clicked(RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    /* check which view to show */
    if (!priv->show_settings) {
        /* show the settings */
        priv->show_settings = TRUE;

        /* make sure we are not showing a call view so we don't have more than one clutter stage at a time */
        //selection_changed(win);

        /* show settings */
        gtk_image_set_from_icon_name(GTK_IMAGE(priv->image_settings), "emblem-ok-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);

        gtk_widget_show(priv->hbox_settings);

        /* make sure to start preview if we're showing the video settings */
        if (priv->last_settings_view == priv->media_settings_view)
            media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(priv->media_settings_view), TRUE);

        /* make sure to show the profile if we're showing the general settings */
        if (priv->last_settings_view == priv->general_settings_view)
            general_settings_view_show_profile(GENERAL_SETTINGS_VIEW(priv->general_settings_view), TRUE);

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

        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), CALL_VIEW_NAME);

        /* show the view which was selected previously */
        //selection_changed(win);
    }
}

static void
show_media_settings(GtkToggleButton *navbutton, RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    if (gtk_toggle_button_get_active(navbutton)) {
        media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(priv->media_settings_view), TRUE);
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
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), GENERAL_SETTINGS_VIEW_NAME);
        priv->last_settings_view = priv->general_settings_view;
    } else {
        general_settings_view_show_profile(GENERAL_SETTINGS_VIEW(priv->general_settings_view), FALSE);
    }
}

static void
show_combobox_account_selector(RingMainWindow *self, gboolean show)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    /* we only want to show the account selector when there is more than 1 eneabled account; so
     * every time we want to show it, we should preform this check */
    gtk_widget_set_visible(priv->combobox_account_selector,
        show && AvailableAccountModel::instance().rowCount() > 1);
}

static void
on_account_creation_completed(RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), CALL_VIEW_NAME);

    /* destroy the wizard */
    if (priv->account_creation_wizard)
    {
        gtk_container_remove(GTK_CONTAINER(priv->stack_main_view), priv->account_creation_wizard);
        gtk_widget_destroy(priv->account_creation_wizard);
    }

    /* show the settings button*/
    gtk_widget_show(priv->ring_settings);

    /* show the account selector */
    show_combobox_account_selector(win, TRUE);

    /* select the newly created account */
    // TODO: the new account might not always be the first one; eg: if the user has an existing
    //       SIP account
    gtk_combo_box_set_active(GTK_COMBO_BOX(priv->combobox_account_selector), 0);
}

static void
show_account_creation_wizard(RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    if (!priv->account_creation_wizard)
    {
        priv->account_creation_wizard = account_creation_wizard_new(false);
        g_object_add_weak_pointer(G_OBJECT(priv->account_creation_wizard), (gpointer *)&priv->account_creation_wizard);
        g_signal_connect_swapped(priv->account_creation_wizard, "account-creation-completed", G_CALLBACK(on_account_creation_completed), win);

        gtk_stack_add_named(GTK_STACK(priv->stack_main_view),
                            priv->account_creation_wizard,
                            ACCOUNT_CREATION_WIZARD_VIEW_NAME);
    }

    /* hide settings button until account creation is complete */
    gtk_widget_hide(priv->ring_settings);
    show_combobox_account_selector(win, FALSE);

    gtk_widget_show(priv->account_creation_wizard);

    gtk_stack_set_visible_child(GTK_STACK(priv->stack_main_view), priv->account_creation_wizard);
}

static void
search_entry_text_changed(GtkSearchEntry *search_entry, RingMainWindow *self)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    /* get the text from the entry */
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(search_entry));
    priv->accountInfoContainer_->accountInfo.conversationModel->setFilter(text);

    /*TODO REMOVE RecentModel::instance().peopleProxy()->setFilterRegExp(QRegExp(text, Qt::CaseInsensitive, QRegExp::FixedString));
    contacts_view_set_filter_string(CONTACTS_VIEW(priv->treeview_contacts), text);
    history_view_set_filter_string(HISTORY_VIEW(priv->treeview_history), text);*/
}

static gboolean
search_entry_key_released(G_GNUC_UNUSED GtkEntry *search_entry, GdkEventKey *key, RingMainWindow *self)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    // if esc key pressed, clear the regex (keep the text, the user might not want to actually delete it)
    /*if (key->keyval == GDK_KEY_Escape) {
        RecentModel::instance().peopleProxy()->setFilterRegExp(QRegExp());
        contacts_view_set_filter_string(CONTACTS_VIEW(priv->treeview_contacts), "");
        history_view_set_filter_string(HISTORY_VIEW(priv->treeview_history), "");
        return GDK_EVENT_STOP;
    }*/

    return GDK_EVENT_PROPAGATE;
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

/**
 * This function determines if two different contact list views (Conversations, Contacts, History)
 * have the same item selected. Note that the same item does not necessarily mean the same object.
 * For example, if the history view has a Call selected and the Conversations view has a Person
 * selected which is associated with the ContactMethod to which that Call was made, then this will
 * evaluate to TRUE.
 */
static bool
compare_treeview_selection(GtkTreeSelection* selection1, GtkTreeSelection* selection2)
{
    g_return_val_if_fail(selection1 && selection2, false);

    if (selection1 == selection2) return true;

    auto idx1 = get_index_from_selection(selection1);
    auto type1 = idx1.data(static_cast<int>(Ring::Role::ObjectType));
    auto object1 = idx1.data(static_cast<int>(Ring::Role::Object));

    auto idx2 = get_index_from_selection(selection2);
    auto type2 = idx2.data(static_cast<int>(Ring::Role::ObjectType));
    auto object2 = idx2.data(static_cast<int>(Ring::Role::Object));

    if (idx1.isValid() && type1.isValid() && object1.isValid() &&
        idx2.isValid() && type2.isValid() && object2.isValid()) {
        Call *call1 = nullptr;
        ContactMethod *cm1 = nullptr;
        Person *person1 = nullptr;

        Call *call2 = nullptr;
        ContactMethod *cm2 = nullptr;
        Person *person2 = nullptr;

        switch(type1.value<Ring::ObjectType>()) {
            case Ring::ObjectType::Person:
                person1 = object1.value<Person *>();
                break;
            case Ring::ObjectType::ContactMethod:
                cm1 = object1.value<ContactMethod *>();
                person1 = cm1->contact();
                break;
            case Ring::ObjectType::Call:
                call1 = object1.value<Call *>();
                cm1 = call1->peerContactMethod();
                person1 = cm1->contact();
                break;
            case Ring::ObjectType::Media:
            case Ring::ObjectType::Certificate:
            case Ring::ObjectType::ContactRequest:
            case Ring::ObjectType::COUNT__:
            // nothing to do
            break;
        }

        switch(type2.value<Ring::ObjectType>()) {
            case Ring::ObjectType::Person:
                person2 = object2.value<Person *>();
                break;
            case Ring::ObjectType::ContactMethod:
                cm2 = object2.value<ContactMethod *>();
                person2 = cm2->contact();
                break;
            case Ring::ObjectType::Call:
                call2 = object2.value<Call *>();
                cm2 = call2->peerContactMethod();
                person2 = cm2->contact();
                break;
            case Ring::ObjectType::Media:
            case Ring::ObjectType::Certificate:
            case Ring::ObjectType::ContactRequest:
            case Ring::ObjectType::COUNT__:
            // nothing to do
            break;
        }

        if (person1 != nullptr && person1 == person2)
            return true;
        if (cm1 != nullptr && cm1 == cm2)
            return true;
        if (call1 != nullptr && call1 == call2)
            return true;
    }

    return false;
}

inline static void
unselect_if_different(GtkTreeSelection *selection1, GtkTreeView* treeview)
{
    auto selection2 = gtk_tree_view_get_selection(treeview);
    if (!compare_treeview_selection(selection1, selection2))
        gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection2));
}

static void
sync_all_view_selection(GtkTreeSelection* selection,
                        GtkTreeView* treeview3)
{
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, nullptr, &iter)) {
        /* something is selected in one view, clear the selection in the other views;
         * unless the current selection is of the same item; we don't try to match the selection in
         * all views since not all items exist in all 3 contact list views
         */

        unselect_if_different(selection, treeview3);
    }
}

static void
conversation_selection_changed(GtkTreeSelection* selection, RingMainWindow* self)
{
    if (gtk_q_tree_model_ignore_selection_change(selection)) return;

    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    sync_all_view_selection(selection,
                            GTK_TREE_VIEW(priv->treeview_contact_requests));

    //selection_changed(self);
}

/*
static void
contact_selection_changed(GtkTreeSelection* selection, RingMainWindow* self)
{
    if (gtk_q_tree_model_ignore_selection_change(selection)) return;

    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    sync_all_view_selection(selection,
                            GTK_TREE_VIEW(priv->treeview_conversations),
                            GTK_TREE_VIEW(priv->treeview_history),
                            GTK_TREE_VIEW(priv->treeview_contact_requests));

    //selection_changed(self);
}

static void
history_selection_changed(GtkTreeSelection* selection, RingMainWindow* self)
{
    if (gtk_q_tree_model_ignore_selection_change(selection)) return;

    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    sync_all_view_selection(selection,
                            GTK_TREE_VIEW(priv->treeview_contacts),
                            GTK_TREE_VIEW(priv->treeview_conversations),
                            GTK_TREE_VIEW(priv->treeview_contact_requests));

    //selection_changed(self);
}
*/

static void
contact_request_selection_changed(GtkTreeSelection* selection, RingMainWindow* self)
{
    if (gtk_q_tree_model_ignore_selection_change(selection)) return;

    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    sync_all_view_selection(selection,
                            GTK_TREE_VIEW(priv->treeview_conversations));

    //selection_changed(self);
}

static gboolean
window_size_changed(GtkWidget *win, GdkEventConfigure *event, gpointer)
{
    auto *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    g_settings_set_int(priv->settings, "window-width", event->width);
    g_settings_set_int(priv->settings, "window-height", event->height);

    return GDK_EVENT_PROPAGATE;
}

static void
search_entry_places_call_changed(GSettings *settings, const gchar *key, RingMainWindow *self)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    if (g_settings_get_boolean(settings, key)) {
        gtk_widget_set_tooltip_text(priv->button_new_conversation, C_("button next to search entry will place a new call", "place call"));
    } else {
        gtk_widget_set_tooltip_text(priv->button_new_conversation, C_("button next to search entry will open chat", "open chat"));
    }
}

static void
handle_account_migrations(RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    /* If there is an existing migration view, remove it */
    if (priv->account_migration_view)
    {
        gtk_widget_destroy(priv->account_migration_view);
        priv->account_migration_view = nullptr;
    }

    QList<Account*> accounts = AccountModel::instance().accountsToMigrate();
    if (!accounts.isEmpty())
    {
        Account* account = accounts.first();

        priv->account_migration_view = account_migration_view_new(account);
        g_signal_connect_swapped(priv->account_migration_view, "account-migration-completed", G_CALLBACK(handle_account_migrations), win);
        g_signal_connect_swapped(priv->account_migration_view, "account-migration-failed", G_CALLBACK(handle_account_migrations), win);

        gtk_widget_hide(priv->ring_settings);
        show_combobox_account_selector(win, FALSE);
        gtk_widget_show(priv->account_migration_view);
        gtk_stack_add_named(
            GTK_STACK(priv->stack_main_view),
            priv->account_migration_view,
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
        show_combobox_account_selector(win, TRUE);
    }
}

/**
 * set the column value by printing the alias and the state of an account in combobox_account_selector.
 */
static void
print_account_and_state(GtkCellLayout* cell_layout,
                        GtkCellRenderer* cell_renderer,
                        GtkTreeModel* tree_model,
                        GtkTreeIter* iter,
                        gpointer* data)
{
    (void) cell_layout; // UNUSED
    (void) data; // UNUSED

    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(tree_model), iter);
    gchar* display_alias = nullptr;

    if (idx.isValid()) {
        auto state = idx.data(static_cast<int>(Account::Role::RegistrationState));
        auto name = idx.data(static_cast<int>(Account::Role::Alias));
        auto account_alias = name.value<QString>().toStdString();

        switch (state.value<Account::RegistrationState>()) {
            case Account::RegistrationState::READY:
            {
                display_alias = g_strdup_printf("%s",account_alias.c_str());
            }
            break;
            case Account::RegistrationState::UNREGISTERED:
            {
                display_alias = g_strdup_printf("<span fgcolor=\"gray\">%s</span>", account_alias.c_str());
            }
            break;
            case Account::RegistrationState::TRYING:
            case Account::RegistrationState::INITIALIZING:
            {
                auto account_alias_i18n = g_markup_printf_escaped (C_("string format will be replaced by the account alias", "%s..."), account_alias.c_str());
                display_alias = g_strdup_printf("<span fgcolor=\"gray\" font_style=\"italic\">%s</span>", account_alias_i18n);
            }
            break;
            case Account::RegistrationState::ERROR:
            {
                auto error_string = g_markup_printf_escaped(C_("string format will be replaced by the account alias", "%s (error)"), account_alias.c_str());
                display_alias = g_strdup_printf("<span fgcolor=\"red\">%s</span>", error_string);
            }
            case Account::RegistrationState::COUNT__:
            break;
        }
    }

    g_object_set(G_OBJECT(cell_renderer), "markup", display_alias, nullptr);
    g_free(display_alias);

}

static void
current_account_changed(RingMainWindow* self, Account* account)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    /* disconnect previous PendingContactRequestModel */
    QObject::disconnect(priv->account_request_added);
    QObject::disconnect(priv->account_request_accepted);
    QObject::disconnect(priv->account_request_discarded);

    set_pending_contact_request_tab_icon(account, self);

    bool is_ring = account && account->protocol() == Account::Protocol::RING;
    if (not is_ring)
        return;

    auto model = account->pendingContactRequestModel();
    auto action = [self, account](ContactRequest* r) {
        (void) r; // UNUSED
        set_pending_contact_request_tab_icon(account, self);
    };

    /* connect current PendingContactRequestModel */
    priv->account_request_added = QObject::connect(model, &PendingContactRequestModel::requestAdded, action);
    priv->account_request_accepted = QObject::connect(model, &PendingContactRequestModel::requestAccepted, action);
    priv->account_request_discarded = QObject::connect(model, &PendingContactRequestModel::requestDiscarded, action);
}

static void
ring_main_window_init(RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);
    gtk_widget_init_template(GTK_WIDGET(win));


    // NOTE dirty hack for gtk...
    // TODO move this
    lrc::account::Info temp;
    priv->accountInfoContainer_ = new AccountInfoContainer(temp);
    priv->accountInfoContainer_->accountInfo = priv->accountInfoContainer_->lrc.getAccountModel().getAccountInfo("a0ae994924d0a494"); // TODO remove this

    /* bind to window size settings */
    priv->settings = g_settings_new_full(get_ring_schema(), nullptr, nullptr);
    auto width = g_settings_get_int(priv->settings, "window-width");
    auto height = g_settings_get_int(priv->settings, "window-height");
    gtk_window_set_default_size(GTK_WINDOW(win), width, height);
    g_signal_connect(win, "configure-event", G_CALLBACK(window_size_changed), nullptr);

    /* search-entry-places-call setting */
    search_entry_places_call_changed(priv->settings, "search-entry-places-call", win);
    g_signal_connect(priv->settings, "changed::search-entry-places-call", G_CALLBACK(search_entry_places_call_changed), win);

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
    g_signal_connect_swapped(priv->ring_settings, "clicked", G_CALLBACK(settings_clicked), win);

    /* add the call view to the main stack */
    gtk_stack_add_named(GTK_STACK(priv->stack_main_view),
                        priv->vbox_call_view,
                        CALL_VIEW_NAME);

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

    /* OLD notebook */
    //~ priv->treeview_conversations = recent_contacts_view_new();
    //~ gtk_container_add(GTK_CONTAINER(priv->scrolled_window_smartview), priv->treeview_conversations);

    /* populate the NEW notebook */
    priv->treeview_conversations = conversations_view_new(priv->accountInfoContainer_);
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_smartview), priv->treeview_conversations);

    auto available_accounts_changed = [win, priv] {
        /* if we're hiding the settings it means we're in the migration or wizard view and we don't
         * want to show the combo box */
        if (gtk_widget_get_visible(priv->ring_settings))
            show_combobox_account_selector(win, TRUE);
    };

    QObject::connect(&AvailableAccountModel::instance(), &QAbstractItemModel::rowsRemoved, available_accounts_changed);
    QObject::connect(&AvailableAccountModel::instance(), &QAbstractItemModel::rowsInserted, available_accounts_changed);
    QObject::connect(AvailableAccountModel::instance().selectionModel(), &QItemSelectionModel::currentChanged,
    [win] (const QModelIndex& current, const QModelIndex& previous) {
        auto account = current.data(static_cast<int>(Account::Role::Object)).value<Account*>();
        current_account_changed(win, account);
    });

    priv->treeview_contact_requests = pending_contact_requests_view_new();
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_contact_requests), priv->treeview_contact_requests);

    /* welcome/default view */
    priv->welcome_view = ring_welcome_view_new();
    g_object_ref(priv->welcome_view); // increase ref because don't want it to be destroyed when not displayed
    gtk_container_add(GTK_CONTAINER(priv->frame_call), priv->welcome_view);
    gtk_widget_show(priv->welcome_view);

    auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_conversations));
    g_signal_connect(selection_conversations, "changed", G_CALLBACK(conversation_selection_changed), win);

    auto selection_contact_request = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_contact_requests));
    g_signal_connect(selection_contact_request, "changed", G_CALLBACK(contact_request_selection_changed), win);

    g_signal_connect_swapped(priv->button_new_conversation, "clicked", G_CALLBACK(search_entry_activated), win);
    g_signal_connect_swapped(priv->search_entry, "activate", G_CALLBACK(search_entry_activated), win);
    g_signal_connect(priv->search_entry, "search-changed", G_CALLBACK(search_entry_text_changed), win);
    g_signal_connect(priv->search_entry, "key-release-event", G_CALLBACK(search_entry_key_released), win);

    auto provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        ".search-entry-style { border: 0; border-radius: 0; } \
        .spinner-style { border: 0; background: white; } \
        .new-conversation-style { border: 0; background: white; transition: all 0.3s ease; border-radius: 0; } \
        .new-conversation-style:hover {  background: #bae5f0; }",
        -1, nullptr
    );
    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    /* make sure the incoming call is the selected call in the CallModel */
    QObject::connect(
        &CallModel::instance(),
        &CallModel::incomingCall,
        [priv](Call* call) {
            // select the revelant account
            auto idx = call->account()->index();                        // from AccountModel
            idx = AvailableAccountModel::instance().mapFromSource(idx); // from AvailableAccountModel

            AvailableAccountModel::instance().selectionModel()->setCurrentIndex(idx,
                                                                                QItemSelectionModel::ClearAndSelect);


            // now make sure its selected
            RecentModel::instance().selectionModel()->clearCurrentIndex();
            CallModel::instance().selectCall(call);
        }
    );

    /* react to digit key press events */
    g_signal_connect(win, "key-press-event", G_CALLBACK(dtmf_pressed), NULL);

    /* set the search entry placeholder text */
    gtk_entry_set_placeholder_text(GTK_ENTRY(priv->search_entry),
                                   C_("Please try to make the translation 50 chars or less so that it fits into the layout", "Search contacts or enter number"));

    /* init chat webkit container so that it starts loading before the first time we need it*/
    get_webkit_chat_container(win);

    if (has_ring_account()) {
        /* user has ring account, so show the call view right away */
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_main_view), priv->vbox_call_view);

        handle_account_migrations(win);
    } else {
        /* user has to create the ring account */
        show_account_creation_wizard(win);
    }

    /* setup account selector */
    gtk_combo_box_set_qmodel(
        GTK_COMBO_BOX(priv->combobox_account_selector),
        &AvailableAccountModel::instance(),
        AvailableAccountModel::instance().selectionModel()
    );

    /* set visibility */
    show_combobox_account_selector(win, TRUE);

    /* layout */
    auto *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->combobox_account_selector), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(priv->combobox_account_selector), renderer, "text", 0, NULL);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(priv->combobox_account_selector),
                                       renderer,
                                       (GtkCellLayoutDataFunc)print_account_and_state,
                                       nullptr, nullptr);

    // we closing any view opened to avoid confusion (especially between SIP and Ring protocols).
    g_signal_connect_swapped(priv->combobox_account_selector, "changed", G_CALLBACK(hide_view_clicked), win);

    // initialize the pending contact request icon.
    current_account_changed(win, get_active_ring_account());

    // New conversation view
    QObject::connect(&*priv->accountInfoContainer_->accountInfo.conversationModel,
    &lrc::ConversationModel::showChatView,
    [win, priv] (lrc::conversation::Info origin) {
        // Change the view if we want a different view.
        auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));

        lrc::conversation::Info current_item;
        if (IS_CHAT_VIEW(old_view))
            current_item = chat_view_get_conversation(CHAT_VIEW(old_view));

        if (current_item.uid != origin.uid) {
            change_view(win, old_view, origin, CHAT_VIEW_TYPE);
        }
    });

    QObject::connect(&*priv->accountInfoContainer_->accountInfo.conversationModel,
    &lrc::ConversationModel::modelUpdated,
    [win, priv] () {
        // Change the view if we want a different view.
        auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));

        lrc::conversation::Info current_item;
        if (IS_CHAT_VIEW(old_view))
            current_item = chat_view_get_conversation(CHAT_VIEW(old_view));

        if (current_item.participants.empty()) {
            change_view(win, old_view, current_item, RING_WELCOME_VIEW_TYPE);
        } else if (!current_item.isUsed) {
            // if current conversation is temporary, change if it's not a contact
            auto uri = current_item.participants.front();
            try {
                // else the contact was added
                priv->accountInfoContainer_->accountInfo.contactModel->getContact(uri);
                chat_view_update_temporary(CHAT_VIEW(old_view));
                gtk_entry_set_text(GTK_ENTRY(priv->search_entry), "");
            } catch (const std::out_of_range&) {
                change_view(win, old_view, current_item, RING_WELCOME_VIEW_TYPE);
            }
        } else {
            // change view if contact was removed
            auto uri = current_item.participants.front();
            try {
                priv->accountInfoContainer_->accountInfo.contactModel->getContact(uri);
            } catch (const std::out_of_range&) {
                // contact was removed
                gtk_entry_set_text(GTK_ENTRY(priv->search_entry), "");
                change_view(win, old_view, current_item, RING_WELCOME_VIEW_TYPE);
            }
        }
    });
}

static void
ring_main_window_dispose(GObject *object)
{
    RingMainWindow *self = RING_MAIN_WINDOW(object);
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    delete priv->accountInfoContainer_;

    QObject::disconnect(priv->selected_item_changed);
    QObject::disconnect(priv->selected_call_over);
    QObject::disconnect(priv->username_lookup);
    QObject::disconnect(priv->account_request_added);
    QObject::disconnect(priv->account_request_accepted);
    QObject::disconnect(priv->account_request_discarded);
    QObject::disconnect(priv->account_request_);

    g_clear_object(&priv->welcome_view);
    g_clear_object(&priv->webkit_chat_container);

    G_OBJECT_CLASS(ring_main_window_parent_class)->dispose(object);
}

static void
ring_main_window_finalize(GObject *object)
{
    RingMainWindow *self = RING_MAIN_WINDOW(object);
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    g_clear_object(&priv->settings);

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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, ring_menu);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, image_ring);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, ring_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, image_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, hbox_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, search_entry);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, stack_main_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, vbox_call_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, frame_call);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, button_new_conversation  );
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_general_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_media_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_account_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, spinner_lookup);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, combobox_account_selector);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, scrolled_window_contact_requests);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, image_contact_requests_list);
}

GtkWidget *
ring_main_window_new (GtkApplication *app)
{
    gpointer win = g_object_new(RING_MAIN_WINDOW_TYPE, "application", app, NULL);

    return (GtkWidget *)win;
}
