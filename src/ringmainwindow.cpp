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
#include <trustrequest.h>

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
#include "utils/files.h"
#include "pendingcontactrequests.h"
#include "contactrequestcontentview.h"

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
    GtkWidget *treeview_pending_trust;
    GtkWidget *scrolled_window_contact_requests;
    GtkWidget *contact_request_view;

    /* Pending ring usernames lookup for the search entry */
    QMetaObject::Connection username_lookup;
    std::string* pending_username_lookup;

    /* The webkit_chat_container is created once, then reused for all chat views */
    GtkWidget *webkit_chat_container;

    QMetaObject::Connection selected_item_changed;
    QMetaObject::Connection selected_call_over;

    gboolean   show_settings;

    /* fullscreen */
    gboolean is_fullscreen;

    GSettings *settings;
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
    auto selection_contact_request = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_pending_trust));
    gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_contact_request));

}

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
            g_signal_connect(new_view, "video-double-clicked", G_CALLBACK(video_double_clicked), self);
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
            g_signal_connect(new_view, "hide-view-clicked", G_CALLBACK(hide_view_clicked), self);

            /* connect to the Person's callAdded signal, because we want to switch to the call view
             * in this case */
            priv->selected_item_changed = QObject::connect(
                person,
                &Person::callAdded,
                [self] (Call*)
                {g_idle_add((GSourceFunc)selection_changed, self);}
            );
        } else if (auto cm = qobject_cast<ContactMethod *>(object)) {
            new_view = chat_view_new_cm(get_webkit_chat_container(self), cm);
            g_signal_connect(new_view, "hide-view-clicked", G_CALLBACK(hide_view_clicked), self);

            /* connect to the ContactMethod's callAdded signal, because we want to switch to the
             * call view in this case */
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
        if (auto trust_request = qobject_cast<TrustRequest *>(object)) {
            new_view = contact_request_content_view_new(trust_request);
            g_signal_connect(new_view, "close_pending_contact_request_content_view-clicked", G_CALLBACK(hide_view_clicked), self);
        }
    } else {
        // display the welcome view
        new_view = priv->welcome_view;
    }

    gtk_container_add(GTK_CONTAINER(priv->frame_call), new_view);
    gtk_widget_show(new_view);
}

/**
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
 */
static gboolean
selection_changed(RingMainWindow *win)
{

    g_return_val_if_fail(IS_RING_MAIN_WINDOW(win), G_SOURCE_REMOVE);
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(win));

    auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));

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
    auto selection_contact_request = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_pending_trust));

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

    /* check which object type is selected */
    auto type = idx.data(static_cast<int>(Ring::Role::ObjectType));
    auto object = idx.data(static_cast<int>(Ring::Role::Object));

    if (idx.isValid() && type.isValid() && object.isValid()) {

        /* we prioritize the views in the following order:
         *  - call view (if there is an ongoing call associated with the item)
         *  - chat view built from Person
         *  - chat view built from ContactMethod
         *  - contact request content view built from TrustRequest
         */

        Person *person = nullptr;
        ContactMethod *cm = nullptr;
        Call *call = nullptr;
        TrustRequest *trust_request = nullptr;

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
            case Ring::ObjectType::TrustRequest:
            {
                trust_request = object.value<TrustRequest *>();
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
            /* show chat view constructed from Person object */

            // check if we're already displaying the chat view for this person
            Person *current_person = nullptr;
            if (IS_CHAT_VIEW(old_view))
                current_person = chat_view_get_person(CHAT_VIEW(old_view));

            if (current_person != person)
                change_view(win, old_view, person, CHAT_VIEW_TYPE);
        } else if (cm) {
            /* show chat view constructed from CM */

            // check if we're already displaying the chat view for this cm
            ContactMethod *current_cm = nullptr;
            if (IS_CHAT_VIEW(old_view))
                current_cm = chat_view_get_cm(CHAT_VIEW(old_view));

            if (current_cm != cm)
                change_view(win, old_view, cm, CHAT_VIEW_TYPE);

        } else if (trust_request) {
            /* show the trust request page */
            change_view(win, old_view, trust_request, CONTACT_REQUEST_CONTENT_VIEW_TYPE);

        } else {
            /* not a supported object type, display the welcome view */
            if (old_view != priv->welcome_view)
                change_view(win, old_view, nullptr, RING_WELCOME_VIEW_TYPE);
        }
    } else {
        /* selection isn't valid, display the welcome view */
        if (old_view != priv->welcome_view)
            change_view(win, old_view, nullptr, RING_WELCOME_VIEW_TYPE);
    }

    return G_SOURCE_REMOVE;
}

static void
process_search_entry_contact_method(RingMainWindow *self, const URI& uri)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    auto cm = PhoneDirectoryModel::instance().getNumber(uri);

    if (g_settings_get_boolean(priv->settings, "search-entry-places-call")) {
        place_new_call(cm);

        /* move focus away from entry so that DTMF tones can be entered via the keyboard */
        gtk_widget_child_focus(GTK_WIDGET(self), GTK_DIR_TAB_FORWARD);
    } else {
        // if its a new CM, bring it to the top
        if (cm->lastUsed() == 0)
            cm->setLastUsed(QDateTime::currentDateTime().toTime_t());

        // select cm
        RecentModel::instance().selectionModel()->setCurrentIndex(RecentModel::instance().getIndex(cm), QItemSelectionModel::ClearAndSelect);
    }
    gtk_entry_set_text(GTK_ENTRY(priv->search_entry), "");
}

static void
search_entry_activated(RingMainWindow *self)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    const auto *querry = gtk_entry_get_text(GTK_ENTRY(priv->search_entry));

    URI uri = URI(querry);


    // make sure the userinfo part isn't empty, only specifying a protocol isn't enough
    if (!uri.userinfo().isEmpty()) {

        gboolean lookup_username = FALSE;

        /* get the account with which we will do the lookup */
        auto ring_account = AvailableAccountModel::instance().currentDefaultAccount(URI::SchemeType::RING);

        if (uri.protocolHint() == URI::ProtocolHint::RING_USERNAME ) {
            lookup_username = TRUE;
        } else if (
            uri.protocolHint() != URI::ProtocolHint::RING && // not a RingID
            uri.schemeType() == URI::SchemeType::NONE // scheme type not specified
        ) {
            // if no scheme type has been specified, determine ring vs sip by the first available account
            auto idx = AvailableAccountModel::instance().index(0, 0);
            if (idx.isValid()) {
                auto account = idx.data((int)Ring::Role::Object).value<Account *>();
                if (account && account->protocol() == Account::Protocol::RING)
                    lookup_username = TRUE;
            }
        }

        if (lookup_username)
        {
            gtk_spinner_start(GTK_SPINNER(priv->spinner_lookup));
            *priv->pending_username_lookup = querry;
            gtk_entry_set_text(GTK_ENTRY(priv->search_entry), "");

            QString username_to_lookup = uri.format(
                URI::Section::USER_INFO |
                URI::Section::HOSTNAME |
                URI::Section::PORT
            );

            QObject::disconnect(priv->username_lookup);
            priv->username_lookup = QObject::connect(
                &NameDirectory::instance(),
                &NameDirectory::registeredNameFound,
                [self, priv, username_to_lookup] (G_GNUC_UNUSED const Account* account, NameDirectory::LookupStatus status, const QString& address, const QString& name) {

                    auto name_qbarray = name.toLatin1();
                    if ( strcmp(priv->pending_username_lookup->data(), name_qbarray.data()) != 0 )
                        return;

                    gtk_entry_set_text(GTK_ENTRY(priv->search_entry), "");

                    switch(status)
                    {
                        case NameDirectory::LookupStatus::SUCCESS:
                        {
                            URI uri = URI("ring:" + address);
                            process_search_entry_contact_method(self, uri);
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
                    *priv->pending_username_lookup = "";
                    gtk_spinner_stop(GTK_SPINNER(priv->spinner_lookup));
                }
            );

            NameDirectory::instance().lookupName(ring_account, QString(), username_to_lookup);
        }
        else
        {
            *priv->pending_username_lookup = "";
            gtk_spinner_stop(GTK_SPINNER(priv->spinner_lookup));
            process_search_entry_contact_method(self, uri);
        }
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

static void
on_account_creation_completed(RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    gtk_stack_set_transition_type(GTK_STACK(priv->stack_main_view), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
    gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), CALL_VIEW_NAME);

    /* destroy the wizard */
    if (priv->account_creation_wizard)
    {
        gtk_container_remove(GTK_CONTAINER(priv->stack_main_view), priv->account_creation_wizard);
        gtk_widget_destroy(priv->account_creation_wizard);
    }

    /* show the settings button*/
    gtk_widget_show(priv->ring_settings);
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

    gtk_widget_show(priv->account_creation_wizard);

    gtk_stack_set_visible_child(GTK_STACK(priv->stack_main_view), priv->account_creation_wizard);
}

static void
search_entry_text_changed(GtkSearchEntry *search_entry, RingMainWindow *self)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    /* get the text from the entry */
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(search_entry));

    RecentModel::instance().peopleProxy()->setFilterRegExp(QRegExp(text, Qt::CaseInsensitive, QRegExp::FixedString));
    contacts_view_set_filter_string(CONTACTS_VIEW(priv->treeview_contacts), text);
    history_view_set_filter_string(HISTORY_VIEW(priv->treeview_history), text);
}

static gboolean
search_entry_key_released(GtkEntry *search_entry, GdkEventKey *key, RingMainWindow *self)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    // if esc key pressed, clear the regex (keep the text, the user might not want to actually delete it)
    if (key->keyval == GDK_KEY_Escape) {
        RecentModel::instance().peopleProxy()->setFilterRegExp(QRegExp());
        contacts_view_set_filter_string(CONTACTS_VIEW(priv->treeview_contacts), "");
        history_view_set_filter_string(HISTORY_VIEW(priv->treeview_history), "");
        return GDK_EVENT_STOP;
    }

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
static gboolean
compare_treeview_selection(GtkTreeSelection *selection1, GtkTreeSelection *selection2)
{
    g_return_val_if_fail(selection1 && selection2, FALSE);

    if (selection1 == selection2) return TRUE;

    auto idx1 = get_index_from_selection(selection1);
    auto type1 = idx1.data(static_cast<int>(Ring::Role::ObjectType));
    auto object1 = idx1.data(static_cast<int>(Ring::Role::Object));

    auto idx2 = get_index_from_selection(selection2);
    auto type2 = idx2.data(static_cast<int>(Ring::Role::ObjectType));
    auto object2 = idx2.data(static_cast<int>(Ring::Role::Object));

    if (idx1.isValid() && type1.isValid() && object1.isValid() && idx2.isValid() && type2.isValid() && object2.isValid()) {
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
            case Ring::ObjectType::COUNT__:
            case Ring::ObjectType::TrustRequest:
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
            case Ring::ObjectType::TrustRequest:
            case Ring::ObjectType::COUNT__:
            // nothing to do
            break;
        }

        if (person1 != nullptr && person1 == person2)
            return TRUE;
        if (cm1 != nullptr && cm1 == cm2)
            return TRUE;
        if (call1 != nullptr && call1 == call2)
            return TRUE;
    }

    return FALSE;
}

static void
conversation_selection_changed(GtkTreeSelection *selection, RingMainWindow *self)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    GtkTreeIter iter;
    GtkTreeModel *model = nullptr;
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        /* something is selected in the conversations view, clear the selection in the other views;
         * unless the current selection is of the same item; we don't try to match the selection in
         * all views since not all items exist in all 3 contact list views
         */

        auto selection_contacts = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_contacts));
        if (!compare_treeview_selection(selection, selection_contacts))
            gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_contacts));

        auto selection_history = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_history));
        if (!compare_treeview_selection(selection, selection_history))
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
         * unless the current selection is of the same item; we don't try to match the selection in
         * all views since not all items exist in all 3 contact list views
         */
        auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_conversations));
        if (!compare_treeview_selection(selection, selection_conversations))
            gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_conversations));

        auto selection_history = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_history));
        if (!compare_treeview_selection(selection, selection_history))
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
         * unless the current selection is of the same item; we don't try to match the selection in
         * all views since not all items exist in all 3 contact list views
         */

        auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_conversations));
        if (!compare_treeview_selection(selection, selection_conversations))
            gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_conversations));

        auto selection_contacts = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_contacts));
        if (!compare_treeview_selection(selection, selection_contacts))
            gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_contacts));
    }

    selection_changed(self);
}

static void
contact_request_selection_changed(GtkTreeSelection *selection, RingMainWindow *self)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, nullptr, &iter)) {
        /* something is selected in the history view, clear the selection in the other views;
         * unless the current selection is of the same item; we don't try to match the selection in
         * all views since not all items exist in all 3 contact list views
         */

        auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_conversations));
        if (!compare_treeview_selection(selection, selection_conversations))
            gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_conversations));

        auto selection_contacts = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_contacts));
        if (!compare_treeview_selection(selection, selection_contacts))
            gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_contacts));
    }

    selection_changed(self);
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
    }
}

static void
selected_account_changed(GtkComboBox *gtk_combo_box, ChatView *self)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    int nbr = gtk_combo_box_get_active(gtk_combo_box);

    QModelIndex idx = AccountModel::instance().index(nbr, 0);
    auto account = AccountModel::instance().getAccountByModelIndex(idx);

    AccountModel::instance().setSelectedAccount(account);
}

static void
state_to_string(GtkCellLayout* cell_layout,
                GtkCellRenderer* cell_redenrer,
                GtkTreeModel* tree_model,
                GtkTreeIter* iter,
                gpointer* data)
{
    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(tree_model), iter);
    auto account = AccountModel::instance().getAccountByModelIndex(idx);
    auto accountAlias = account->alias();

    gchar* account_alias = g_markup_escape_text(accountAlias.toUtf8().constData(), -1);
    gchar* display_alias = NULL;

    if (idx.isValid()) {
        auto account = AccountModel::instance().getAccountByModelIndex(idx);

        switch (account->registrationState()) {
            case Account::RegistrationState::READY:
            {
                display_alias = g_strdup_printf("%s",account_alias);
            }
            break;
            case Account::RegistrationState::UNREGISTERED:
            {
                display_alias = g_strdup_printf("<span fgcolor=\"gray\">%s</span>", account_alias);
            }
            break;
            case Account::RegistrationState::TRYING:
            case Account::RegistrationState::INITIALIZING:
            {
                display_alias = g_strdup_printf("<span fgcolor=\"gray\" font_style=\"italic\">%s...</span>", account_alias);
            }
            break;
            case Account::RegistrationState::ERROR:
            case Account::RegistrationState::COUNT__:
            {
                display_alias = g_strdup_printf("<span fgcolor=\"red\">%s (error)</span>", account_alias);
            }
            break;
        }
    }

    g_object_set(G_OBJECT(cell_redenrer), "markup", display_alias, NULL);
    g_free(display_alias);
    g_free(account_alias);

}

static void
ring_main_window_init(RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);
    gtk_widget_init_template(GTK_WIDGET(win));

    priv->pending_username_lookup = new std::string; // see object finilize for delete

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
    priv->treeview_conversations = recent_contacts_view_new();
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_smartview), priv->treeview_conversations);

    priv->treeview_contacts = contacts_view_new();
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_contacts), priv->treeview_contacts);

    priv->treeview_history = history_view_new();
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_history), priv->treeview_history);

    priv->treeview_pending_trust = pending_contact_requests_view_new();
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_contact_requests), priv->treeview_pending_trust);

    /* use this event to refresh the treeview_conversations when account selection changed */
    QObject::connect(&AccountModel::instance(), &AccountModel::selectedAccountChanged, [priv](Account* a){
        // next line will refresh the recentmodel so the treeview will do
        RecentModel::instance().peopleProxy()->setFilterRegExp("");

        // set the good index to the combox. Required when the selected account changed by different way than selecting
        // from the combo box.
        gtk_combo_box_set_active(GTK_COMBO_BOX(priv->combobox_account_selector),
                                 a->index().row());

    });

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

    auto selection_contact_request = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_pending_trust));
    g_signal_connect(selection_contact_request, "changed", G_CALLBACK(contact_request_selection_changed), win);

    g_signal_connect_swapped(priv->button_new_conversation, "clicked", G_CALLBACK(search_entry_activated), win);
    g_signal_connect_swapped(priv->search_entry, "activate", G_CALLBACK(search_entry_activated), win);
    g_signal_connect(priv->search_entry, "search-changed", G_CALLBACK(search_entry_text_changed), win);
    g_signal_connect(priv->search_entry, "key-release-event", G_CALLBACK(search_entry_key_released), win);

    /* make sure the incoming call is the selected call in the CallModel */
    QObject::connect(
        &CallModel::instance(),
        &CallModel::incomingCall,
        [priv](Call* call) {
            // clear the regex to make sure the call is shown
            RecentModel::instance().peopleProxy()->setFilterRegExp(QRegExp());
            contacts_view_set_filter_string(CONTACTS_VIEW(priv->treeview_contacts),"");
            history_view_set_filter_string(HISTORY_VIEW(priv->treeview_history), "");

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

    handle_account_migrations(win);

    /* model for the combobox for Account chooser */
    auto account_model = gtk_q_tree_model_new(&AccountModel::instance(), 1,
        0, Account::Role::Alias, G_TYPE_STRING);

    gtk_combo_box_set_model(GTK_COMBO_BOX(priv->combobox_account_selector), GTK_TREE_MODEL(account_model));

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

    /* layout */
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->combobox_account_selector), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(priv->combobox_account_selector), renderer, "text", 0, NULL);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(priv->combobox_account_selector),
                                       renderer,
                                       (GtkCellLayoutDataFunc)state_to_string,
                                       NULL, NULL);

    g_signal_connect(priv->combobox_account_selector, "changed", G_CALLBACK(selected_account_changed), win);

    /* init the selection for the account selector */
    gtk_combo_box_set_active(GTK_COMBO_BOX(priv->combobox_account_selector), 0);

    g_object_unref(account_model);
}

static void
ring_main_window_dispose(GObject *object)
{
    RingMainWindow *self = RING_MAIN_WINDOW(object);
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    QObject::disconnect(priv->selected_item_changed);
    QObject::disconnect(priv->selected_call_over);
    QObject::disconnect(priv->username_lookup);

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

    delete priv->pending_username_lookup;
    priv->pending_username_lookup = nullptr;

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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, button_new_conversation  );
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_general_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_media_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, radiobutton_account_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, spinner_lookup);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, combobox_account_selector);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, scrolled_window_contact_requests);
}

GtkWidget *
ring_main_window_new (GtkApplication *app)
{
    gpointer win = g_object_new(RING_MAIN_WINDOW_TYPE, "application", app, NULL);

    return (GtkWidget *)win;
}
