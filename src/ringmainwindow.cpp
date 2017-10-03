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

// GTK+ related
#include <glib/gi18n.h>

// LRC
#include <accountmodel.h> // Old lrc but still used
#include <api/account.h>
#include <api/contact.h>
#include <api/contactmodel.h>
#include <api/conversationmodel.h>
#include <api/lrc.h>
#include <api/newaccountmodel.h>
#include <api/behaviorcontroller.h>
#include "accountcontainer.h"
#include "conversationcontainer.h"

// Ring client
#include "accountview.h"
#include "accountmigrationview.h"
#include "accountcreationwizard.h"
#include "chatview.h"
#include "conversationsview.h"
#include "currentcallview.h"
#include "dialogs.h"
#include "generalsettingsview.h"
#include "incomingcallview.h"
#include "mediasettingsview.h"
#include "models/gtkqtreemodel.h"
#include "ringwelcomeview.h"
#include "utils/accounts.h"
#include "utils/files.h"

static constexpr const char* CALL_VIEW_NAME             = "calls";
static constexpr const char* ACCOUNT_CREATION_WIZARD_VIEW_NAME = "account-creation-wizard";
static constexpr const char* ACCOUNT_MIGRATION_VIEW_NAME       = "account-migration-view";
static constexpr const char* GENERAL_SETTINGS_VIEW_NAME = "general";
static constexpr const char* MEDIA_SETTINGS_VIEW_NAME   = "media";
static constexpr const char* ACCOUNT_SETTINGS_VIEW_NAME = "accounts";

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
    GtkWidget *notebook_contacts;
    GtkWidget *scrolled_window_smartview;
    GtkWidget *treeview_conversations;
    GtkWidget *vbox_left_pane;
    GtkWidget *search_entry;
    GtkWidget *stack_main_view;
    GtkWidget *vbox_call_view;
    GtkWidget *frame_call;
    GtkWidget *welcome_view;
    GtkWidget *button_new_conversation;
    GtkWidget *account_settings_view;
    GtkWidget *media_settings_view;
    GtkWidget *general_settings_view;
    GtkWidget *last_settings_view;
    GtkWidget *radiobutton_general_settings;
    GtkWidget *radiobutton_media_settings;
    GtkWidget *radiobutton_account_settings;
    GtkWidget *account_creation_wizard;
    GtkWidget *account_migration_view;
    GtkWidget *combobox_account_selector;
    GtkWidget *treeview_contact_requests;
    GtkWidget *scrolled_window_contact_requests;
    GtkWidget *image_contact_requests_list;

    // The webkit_chat_container is created once, then reused for all chat views
    GtkWidget *webkit_chat_container;

    QMetaObject::Connection selected_item_changed;
    QMetaObject::Connection selected_call_over;

    gboolean   show_settings;

    /* fullscreen */
    gboolean is_fullscreen;

    GSettings *settings;

    // Lrc containers and signals
    std::unique_ptr<lrc::api::Lrc> lrc_;
    AccountContainer* accountContainer_;
    ConversationContainer* chatViewConversation_;
    lrc::api::profile::Type currentTypeFilter_;

    QMetaObject::Connection showChatViewConnection_;
    QMetaObject::Connection showCallViewConnection_;
    QMetaObject::Connection showIncomingViewConnection_;
    QMetaObject::Connection changeAccountConnection_;
    QMetaObject::Connection newAccountConnection_;
    QMetaObject::Connection rmAccountConnection_;
    QMetaObject::Connection historyClearedConnection_;
    QMetaObject::Connection modelSortedConnection_;
    QMetaObject::Connection filterChangedConnection_;
    QMetaObject::Connection newConversationConnection_;
    QMetaObject::Connection conversationRemovedConnection_;
    QMetaObject::Connection accountStatusChangedConnection_;

};

G_DEFINE_TYPE_WITH_PRIVATE(RingMainWindow, ring_main_window, GTK_TYPE_APPLICATION_WINDOW);

#define RING_MAIN_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RING_MAIN_WINDOW_TYPE, RingMainWindowPrivate))

static void
change_view(RingMainWindow *self, GtkWidget* old, lrc::api::conversation::Info conversation, GType type);
static void
ring_init_lrc(RingMainWindow *win, const std::string& accountId);

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
set_pending_contact_request_tab_icon(RingMainWindow* self)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    if (not priv->accountContainer_ )
        return;

    bool isRingAccount = priv->accountContainer_->info.profileInfo.type == lrc::api::profile::Type::RING;
    gtk_widget_set_visible(priv->scrolled_window_contact_requests, isRingAccount);

    if (not isRingAccount)
        return;

    gtk_image_set_from_resource(GTK_IMAGE(priv->image_contact_requests_list),
        (priv->accountContainer_->info.contactModel->hasPendingRequests())
        ? "/cx/ring/RingGnome/contact_requests_list_with_notification"
        : "/cx/ring/RingGnome/contact_requests_list");
}

static void
hide_view_clicked(RingMainWindow *self)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));
    // clear selection and go to welcome page.
    auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_conversations));
    gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_conversations));
    auto selection_contact_request = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_contact_requests));
    gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_contact_request));
    auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));
    lrc::api::conversation::Info current_item;
    if (IS_CHAT_VIEW(old_view))
        current_item = chat_view_get_conversation(CHAT_VIEW(old_view));
    change_view(self, old_view, current_item, RING_WELCOME_VIEW_TYPE);
}

static void
change_view(RingMainWindow *self, GtkWidget* old, lrc::api::conversation::Info conversation, GType type)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    leave_full_screen(self);
    gtk_container_remove(GTK_CONTAINER(priv->frame_call), old);

    GtkWidget *new_view = nullptr;

    QObject::disconnect(priv->selected_item_changed);
    QObject::disconnect(priv->selected_call_over);

    if (g_type_is_a(INCOMING_CALL_VIEW_TYPE, type)) {
        delete priv->chatViewConversation_;
        priv->chatViewConversation_ = new ConversationContainer(conversation);
        new_view = incoming_call_view_new(get_webkit_chat_container(self), priv->accountContainer_, priv->chatViewConversation_);
    } else if (g_type_is_a(CURRENT_CALL_VIEW_TYPE, type)) {
        delete priv->chatViewConversation_;
        priv->chatViewConversation_ = new ConversationContainer(conversation);
        new_view = current_call_view_new(get_webkit_chat_container(self), priv->accountContainer_, priv->chatViewConversation_);

        try {
            auto contactUri =  priv->chatViewConversation_->info.participants.front();
            auto contactInfo = priv->accountContainer_->info.contactModel->getContact(contactUri);
            chat_view_update_temporary(CHAT_VIEW(gtk_bin_get_child(GTK_BIN(priv->frame_call))),
               contactInfo.profileInfo.type == lrc::api::profile::Type::PENDING
               || contactInfo.profileInfo.type == lrc::api::profile::Type::TEMPORARY);
        } catch(...) { }
        g_signal_connect_swapped(new_view, "video-double-clicked", G_CALLBACK(video_double_clicked), self);
    } else if (g_type_is_a(CHAT_VIEW_TYPE, type)) {
        delete priv->chatViewConversation_;
        priv->chatViewConversation_ = new ConversationContainer(conversation);
        new_view = chat_view_new(get_webkit_chat_container(self), priv->accountContainer_, priv->chatViewConversation_);
        g_signal_connect_swapped(new_view, "hide-view-clicked", G_CALLBACK(hide_view_clicked), self);
    } else {
        // TODO select first conversation?
        new_view = priv->welcome_view;
    }

    gtk_container_add(GTK_CONTAINER(priv->frame_call), new_view);
    gtk_widget_show(new_view);
}

static void
show_combobox_account_selector(RingMainWindow *self, gboolean show)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    // we only want to show the account selector when there is more than 1 enabled
    // account; so every time we want to show it, we should preform this check
    gtk_widget_set_visible(priv->combobox_account_selector,
        show && priv->lrc_->getAccountModel().getAccountList().size() > 1);
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
change_to_account(RingMainWindow *win, const std::string& accountId)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);
    // Go to welcome view
    auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));
    change_view(win, old_view, lrc::api::conversation::Info(), RING_WELCOME_VIEW_TYPE);
    // Change combobox
    auto accounts = priv->lrc_->getAccountModel().getAccountList();
    auto i = 0;
    for (; i < accounts.size(); ++i) {
        auto id = accounts.at(i);
        if (id == accountId) break;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(priv->combobox_account_selector), i);
    // Show conversation panel
    gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->notebook_contacts), 0);
    // Reinit LRC
    ring_init_lrc(win, std::string(accountId));
    // Update the welcome view
    ring_welcome_update_view(RING_WELCOME_VIEW(priv->welcome_view), priv->accountContainer_);
}

static void
ring_init_lrc(RingMainWindow *win, const std::string& accountId)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);
    // Disconnect old signals.
    QObject::disconnect(priv->showChatViewConnection_);
    QObject::disconnect(priv->showIncomingViewConnection_);
    QObject::disconnect(priv->changeAccountConnection_);
    QObject::disconnect(priv->newAccountConnection_);
    QObject::disconnect(priv->rmAccountConnection_);
    QObject::disconnect(priv->showCallViewConnection_);
    QObject::disconnect(priv->modelSortedConnection_);
    QObject::disconnect(priv->historyClearedConnection_);
    QObject::disconnect(priv->filterChangedConnection_);
    QObject::disconnect(priv->newConversationConnection_);
    QObject::disconnect(priv->conversationRemovedConnection_);

    // Get the account selected
    if (priv->accountContainer_) delete priv->accountContainer_;
    priv->accountContainer_ = new AccountContainer(priv->lrc_->getAccountModel().getAccountInfo(accountId));

    // Reinit tree views
    if (priv->treeview_conversations) {
        auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_conversations));
        gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_conversations));
        gtk_widget_destroy(priv->treeview_conversations);
    }
    priv->treeview_conversations = conversations_view_new(priv->accountContainer_);
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_smartview), priv->treeview_conversations);

    if (priv->treeview_contact_requests) {
        auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_contact_requests));
        gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_conversations));
        gtk_widget_destroy(priv->treeview_contact_requests);
    }
    priv->treeview_contact_requests = conversations_view_new(priv->accountContainer_);
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_contact_requests), priv->treeview_contact_requests);

    // Connect to signals from LRC
    priv->historyClearedConnection_ = QObject::connect(
    &*priv->accountContainer_->info.conversationModel,
    &lrc::api::ConversationModel::conversationCleared,
    [win, priv] (const std::string& uid) {
        // Change the view when the history is cleared.
        auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));
        lrc::api::conversation::Info current_item;
        current_item.uid = "-1"; // Because the Searching item has an empty uid
        if (IS_CHAT_VIEW(old_view))
             current_item = chat_view_get_conversation(CHAT_VIEW(old_view));
         else
             // if incoming or call view we don't want to change the view
             return;
         if (current_item.uid == uid) {
             // We are on the conversation cleared.
             // Go to welcome view because user doesn't want this conversation
             // TODO go to first conversation?
             hide_view_clicked(win);
         }
     });

     priv->modelSortedConnection_ = QObject::connect(
     &*priv->accountContainer_->info.conversationModel,
     &lrc::api::ConversationModel::modelSorted,
     [win, priv] () {
         // Synchronize selection when sorted and update pending icon
         auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));
         lrc::api::conversation::Info current_item;
         current_item.uid = "-1";
         if (IS_CHAT_VIEW(old_view))
              current_item = chat_view_get_conversation(CHAT_VIEW(old_view));
         else if (IS_CURRENT_CALL_VIEW(old_view))
               current_item = current_call_view_get_conversation(CURRENT_CALL_VIEW(old_view));
          else if (IS_INCOMING_CALL_VIEW(old_view))
                current_item = incoming_call_view_get_conversation(INCOMING_CALL_VIEW(old_view));
         conversations_view_select_conversation(CONVERSATIONS_VIEW(priv->treeview_conversations), current_item.uid);
         set_pending_contact_request_tab_icon(win);
     });

     priv->filterChangedConnection_ = QObject::connect(
     &*priv->accountContainer_->info.conversationModel,
     &lrc::api::ConversationModel::filterChanged,
     [win, priv] () {
         // Synchronize selection when filter changes
         auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));
         lrc::api::conversation::Info current_item;
         current_item.uid = "-1";
         if (IS_CHAT_VIEW(old_view))
              current_item = chat_view_get_conversation(CHAT_VIEW(old_view));
         else if (IS_CURRENT_CALL_VIEW(old_view))
               current_item = current_call_view_get_conversation(CURRENT_CALL_VIEW(old_view));
          else if (IS_INCOMING_CALL_VIEW(old_view))
                current_item = incoming_call_view_get_conversation(INCOMING_CALL_VIEW(old_view));
         conversations_view_select_conversation(CONVERSATIONS_VIEW(priv->treeview_conversations), current_item.uid);
         // Get if conversation still exists.
         auto& conversationModel = priv->accountContainer_->info.conversationModel;
         auto conversations = conversationModel->allFilteredConversations();
         auto isInConv = std::find_if(
         conversations.begin(), conversations.end(),
         [current_item](const lrc::api::conversation::Info& conversation) {
           return current_item.uid == conversation.uid;
         });
         if (IS_CHAT_VIEW(old_view) && isInConv == conversations.end()) {
             change_view(win, old_view, lrc::api::conversation::Info(), RING_WELCOME_VIEW_TYPE);
         }
     });

     priv->newConversationConnection_ = QObject::connect(
     &*priv->accountContainer_->info.conversationModel,
     &lrc::api::ConversationModel::newConversation,
     [win, priv] (const std::string& uid) {
         gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->notebook_contacts), 0);
         priv->accountContainer_->info.conversationModel->setFilter(lrc::api::profile::Type::RING);
         gtk_entry_set_text(GTK_ENTRY(priv->search_entry), "");
         priv->accountContainer_->info.conversationModel->setFilter("");
         // Select new conversation if contact added
         auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));
         if (IS_RING_WELCOME_VIEW(old_view) ||
         (IS_CHAT_VIEW(old_view) && chat_view_get_temporary(CHAT_VIEW(old_view)))) {
             priv->accountContainer_->info.conversationModel->selectConversation(uid);
             try {
                 auto contactUri =  priv->chatViewConversation_->info.participants.front();
                 auto contactInfo = priv->accountContainer_->info.contactModel->getContact(contactUri);
                 chat_view_update_temporary(CHAT_VIEW(gtk_bin_get_child(GTK_BIN(priv->frame_call))),
                    contactInfo.profileInfo.type == lrc::api::profile::Type::PENDING
                    || contactInfo.profileInfo.type == lrc::api::profile::Type::TEMPORARY);
             } catch(...) { }
         }
     });

     priv->conversationRemovedConnection_ = QObject::connect(
     &*priv->accountContainer_->info.conversationModel,
     &lrc::api::ConversationModel::conversationRemoved,
     [win, priv] (const std::string& uid) {
         // If contact is removed, go to welcome view
         auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));
         lrc::api::conversation::Info current_item;
         if (IS_CHAT_VIEW(old_view))
            current_item = chat_view_get_conversation(CHAT_VIEW(old_view));
         else if (IS_CURRENT_CALL_VIEW(old_view))
             current_item = current_call_view_get_conversation(CURRENT_CALL_VIEW(old_view));
         else if (IS_INCOMING_CALL_VIEW(old_view))
            current_item = incoming_call_view_get_conversation(INCOMING_CALL_VIEW(old_view));
         if (current_item.uid == uid)
             change_view(win, old_view, lrc::api::conversation::Info(), RING_WELCOME_VIEW_TYPE);
     });

    priv->showChatViewConnection_ = QObject::connect(
    &priv->lrc_->getBehaviorController(),
    &lrc::api::BehaviorController::showChatView,
    [win, priv] (const std::string& accountId, lrc::api::conversation::Info origin) {
        if (accountId != priv->accountContainer_->info.id)
            change_to_account(win, accountId);
        // Show chat view if not in call (unless if it's the same conversation)
        auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));
        lrc::api::conversation::Info current_item;
        current_item.uid = "-1";
        if (IS_CHAT_VIEW(old_view))
           current_item = chat_view_get_conversation(CHAT_VIEW(old_view));
        if (current_item.uid != origin.uid)
            change_view(win, old_view, origin, CHAT_VIEW_TYPE);
    });

    priv->showCallViewConnection_ = QObject::connect(
    &priv->lrc_->getBehaviorController(),
    &lrc::api::BehaviorController::showCallView,
    [win, priv] (const std::string& accountId, lrc::api::conversation::Info origin) {
        if (accountId != priv->accountContainer_->info.id)
            change_to_account(win, accountId);
        // Change the view if we want a different view.
        auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));

        lrc::api::conversation::Info current_item;
        if (IS_CURRENT_CALL_VIEW(old_view))
            current_item = current_call_view_get_conversation(CURRENT_CALL_VIEW(old_view));

        if (current_item.uid != origin.uid)
            change_view(win, old_view, origin, CURRENT_CALL_VIEW_TYPE);
    });

    priv->showIncomingViewConnection_ = QObject::connect(
    &priv->lrc_->getBehaviorController(),
    &lrc::api::BehaviorController::showIncomingCallView,
    [win, priv] (const std::string& accountId, lrc::api::conversation::Info origin) {
        if (accountId != priv->accountContainer_->info.id)
            change_to_account(win, accountId);
        // Change the view if we want a different view.
        auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));

        lrc::api::conversation::Info current_item;
        if (IS_INCOMING_CALL_VIEW(old_view))
            current_item = incoming_call_view_get_conversation(INCOMING_CALL_VIEW(old_view));

        if (current_item.uid != origin.uid)
            change_view(win, old_view, origin, INCOMING_CALL_VIEW_TYPE);
    });

    priv->newAccountConnection_ = QObject::connect(
    &priv->lrc_->getAccountModel(),
    &lrc::api::NewAccountModel::accountAdded,
    [win, priv] (const std::string& idAdded) {
        // New account added. go to this account
        auto accounts = priv->lrc_->getAccountModel().getAccountList();
        auto store = gtk_list_store_new (2 /* # of cols */ ,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_UINT);
        auto currentIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(priv->combobox_account_selector));
        GtkTreeIter iter;
        for (const auto& accountId : accounts) {
            const auto& account = priv->lrc_->getAccountModel().getAccountInfo(accountId);
            if (account.enabled) {
                gtk_list_store_append (store, &iter);
                gtk_list_store_set (store, &iter,
                0 /* col # */ , accountId.c_str() /* celldata */,
                1 /* col # */ , account.profileInfo.alias.c_str() /* celldata */,
                -1 /* end */);
            }
        }
        // Redraw combobox
        gtk_combo_box_set_model(
            GTK_COMBO_BOX(priv->combobox_account_selector),
            GTK_TREE_MODEL (store)
        );
        gtk_combo_box_set_active(GTK_COMBO_BOX(priv->combobox_account_selector), currentIdx);
    });

    priv->rmAccountConnection_ = QObject::connect(
    &priv->lrc_->getAccountModel(),
    &lrc::api::NewAccountModel::accountRemoved,
    [win, priv] (const std::string& idRemoved) {
        // Account removed, change account if it's the current account selected.
        auto accounts = priv->lrc_->getAccountModel().getAccountList();
        if (accounts.empty()) {
            show_account_creation_wizard(win);
        } else {
            auto store = gtk_list_store_new (2 /* # of cols */ ,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_UINT);
            GtkTreeIter iter;
            for (const auto& accountId : accounts) {
                const auto& account = priv->lrc_->getAccountModel().getAccountInfo(accountId);
                if (account.enabled) {
                    gtk_list_store_append (store, &iter);
                    gtk_list_store_set (store, &iter,
                    0 /* col # */ , accountId.c_str() /* celldata */,
                    1 /* col # */ , account.profileInfo.alias.c_str() /* celldata */,
                    -1 /* end */);
                }
            }

            gtk_combo_box_set_model(
            GTK_COMBO_BOX(priv->combobox_account_selector),
            GTK_TREE_MODEL (store)
            );
            gtk_combo_box_set_active(GTK_COMBO_BOX(priv->combobox_account_selector), 0);
            // Show conversation panel
            gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->notebook_contacts), 0);
            // Reinit LRC
            ring_init_lrc(win, std::string(accounts.at(0)));
            // Update the welcome view
            ring_welcome_update_view(RING_WELCOME_VIEW(priv->welcome_view), priv->accountContainer_);
        }
    });

    const gchar *text = gtk_entry_get_text(GTK_ENTRY(priv->search_entry));
    priv->currentTypeFilter_ = priv->accountContainer_->info.profileInfo.type;
    priv->accountContainer_->info.conversationModel->setFilter(text);
    priv->accountContainer_->info.conversationModel->setFilter(priv->currentTypeFilter_);
}

static void
account_changed(RingMainWindow *self)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    auto accountComboBox = GTK_COMBO_BOX(priv->combobox_account_selector);
    auto model = gtk_combo_box_get_model(accountComboBox);
    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(accountComboBox, &iter)) {
        gchar *accountId;
        gtk_tree_model_get (model, &iter,
                            0 /* col# */, &accountId /* data */,
                            -1);
        // Reinit view
        auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));
        change_view(self, old_view, lrc::api::conversation::Info(), RING_WELCOME_VIEW_TYPE);
        gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->notebook_contacts), 0);
        // Change account
        ring_init_lrc(self, std::string(accountId));
        ring_welcome_update_view(RING_WELCOME_VIEW(priv->welcome_view), priv->accountContainer_);
        g_free(accountId);
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
settings_clicked(RingMainWindow *win)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(win));
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    /* check which view to show */
    if (!priv->show_settings) {
        /* show the settings */
        priv->show_settings = TRUE;

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
search_entry_text_changed(GtkSearchEntry *search_entry, RingMainWindow *self)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    // Filter model
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(search_entry));
    priv->accountContainer_->info.conversationModel->setFilter(text);
}

static void
search_entry_activated(RingMainWindow *self)
{
    // Select the first conversation of the list
    auto *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    auto& conversationModel = priv->accountContainer_->info.conversationModel;
    auto conversations = conversationModel->allFilteredConversations();
    if (!conversations.empty())
        conversationModel->selectConversation(conversations[0].uid);
}

static gboolean
search_entry_key_released(G_GNUC_UNUSED GtkEntry *search_entry, GdkEventKey *key, RingMainWindow *self)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    // if esc key pressed, clear the regex (keep the text, the user might not want to actually delete it)
    if (key->keyval == GDK_KEY_Escape) {
        priv->accountContainer_->info.conversationModel->setFilter("");
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

    // NOTE deactivated for now, because not implemented in the new models.
    /* make sure that a call is selected* /
    QItemSelectionModel *selection = CallModel::instance().selectionModel();
    QModelIndex idx = selection->currentIndex();
    if (!idx.isValid())
        return GDK_EVENT_PROPAGATE;

    /* make sure that the selected call is in progress * /
    Call *call = CallModel::instance().getCall(idx);
    Call::LifeCycleState state = call->lifeCycleState();
    if (state != Call::LifeCycleState::PROGRESS)
        return GDK_EVENT_PROPAGATE;

    /* filter out cretain MOD masked key presses so that, for example, 'Ctrl+c'
     * does not result in a 'c' being played.
     * we filter Ctrl, Alt, and SUPER/HYPER/META keys * /
    if ( event->state
        & ( GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ))
        return GDK_EVENT_PROPAGATE;

    /* pass the character that was entered to be played by the daemon;
     * the daemon will filter out invalid DTMF characters * /
    guint32 unicode_val = gdk_keyval_to_unicode(event->keyval);
    QString val = QString::fromUcs4(&unicode_val, 1);
    call->playDTMF(val);
    g_debug("attemptingto play DTMF tone during ongoing call: %s", val.toUtf8().constData());

    /* always propogate the key, so we don't steal accelerators/shortcuts */
    return GDK_EVENT_PROPAGATE;
}

static void
tab_changed(G_GNUC_UNUSED GtkNotebook* notebook,
            G_GNUC_UNUSED GtkWidget* page,
            guint page_num,
            RingMainWindow* self)
{
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    auto newType = page_num == 0 ? priv->accountContainer_->info.profileInfo.type : lrc::api::profile::Type::PENDING;
    if (priv->currentTypeFilter_ != newType) {
        priv->currentTypeFilter_ = newType;
        priv->accountContainer_->info.conversationModel->setFilter(priv->currentTypeFilter_);
    }
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
                        GtkCellRenderer* cell,
                        GtkTreeModel* model,
                        GtkTreeIter* iter,
                        gpointer* data)
{
    gchar *alias;
    gchar *text;

    gtk_tree_model_get (model, iter,
                        1 /* col# */, &alias /* data */,
                        -1);

    text = g_markup_printf_escaped(
        "<span fgcolor=\"gray\">%s</span>",
        alias
    );

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(alias);
}


static void
ring_main_window_init(RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);
    gtk_widget_init_template(GTK_WIDGET(win));

    // NOTE: When new models will be fully implemented, we need to move this
    // in rign_client.cpp.
    // Init LRC and the vew
    priv->lrc_ = std::make_unique<lrc::api::Lrc>();
    const auto accountIds = priv->lrc_->getAccountModel().getAccountList();
    if (not accountIds.empty()) {
        ring_init_lrc(win, accountIds.front());
    }

    // Account status changed
    priv->accountStatusChangedConnection_ = QObject::connect(
    &priv->lrc_->getAccountModel(),
    &lrc::api::NewAccountModel::accountStatusChanged,
    [win, priv] (const std::string& accountId) {
        if (not priv->accountContainer_ ) {
            ring_init_lrc(win, accountId);

            if (priv->accountContainer_)
                ring_welcome_update_view(RING_WELCOME_VIEW(priv->welcome_view),
                                         priv->accountContainer_);
        }
    });

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

    g_signal_connect(priv->notebook_contacts, "switch-page", G_CALLBACK(tab_changed), win);

    auto available_accounts_changed = [win, priv] {
        /* if we're hiding the settings it means we're in the migration or wizard view and we don't
         * want to show the combo box */
        if (gtk_widget_get_visible(priv->ring_settings))
            show_combobox_account_selector(win, TRUE);
    };

    /* welcome/default view */
    priv->welcome_view = ring_welcome_view_new(priv->accountContainer_);
    g_object_ref(priv->welcome_view); // increase ref because don't want it to be destroyed when not displayed
    gtk_container_add(GTK_CONTAINER(priv->frame_call), priv->welcome_view);
    gtk_widget_show(priv->welcome_view);

    g_signal_connect_swapped(priv->search_entry, "activate", G_CALLBACK(search_entry_activated), win);
    g_signal_connect_swapped(priv->button_new_conversation, "clicked", G_CALLBACK(search_entry_activated), win);
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

    // setup account selector and select the first account
    auto store = gtk_list_store_new (2 /* # of cols */ ,
                                     G_TYPE_STRING,
                                     G_TYPE_STRING,
                                     G_TYPE_UINT);
    GtkTreeIter iter;
    for (const auto& accountId : accountIds) {
        const auto& accountInfo = priv->lrc_->getAccountModel().getAccountInfo(accountId);
        if (accountInfo.enabled) {
            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter,
            0 /* col # */ , accountId.c_str() /* celldata */,
            1 /* col # */ , accountInfo.profileInfo.alias.c_str() /* celldata */,
            -1 /* end */);
        }
    }

    gtk_combo_box_set_model(
        GTK_COMBO_BOX(priv->combobox_account_selector),
        GTK_TREE_MODEL (store)
    );
    gtk_combo_box_set_active(GTK_COMBO_BOX(priv->combobox_account_selector), 0);

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
    g_signal_connect_swapped(priv->combobox_account_selector, "changed", G_CALLBACK(account_changed), win);

    // initialize the pending contact request icon.
    set_pending_contact_request_tab_icon(win);

    if (not accountIds.empty()) {
        auto& conversationModel = priv->accountContainer_->info.conversationModel;
        auto conversations = conversationModel->allFilteredConversations();
        for (const auto& conversation: conversations) {
            if (!conversation.callId.empty()) {
                priv->accountContainer_->info.conversationModel->selectConversation(conversation.uid);
            }
        }
    }
}

static void
ring_main_window_dispose(GObject *object)
{
    RingMainWindow *self = RING_MAIN_WINDOW(object);
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    delete priv->accountContainer_;
    delete priv->chatViewConversation_;

    QObject::disconnect(priv->selected_item_changed);
    QObject::disconnect(priv->selected_call_over);


    QObject::disconnect(priv->showChatViewConnection_);
    QObject::disconnect(priv->showIncomingViewConnection_);
    QObject::disconnect(priv->historyClearedConnection_);
    QObject::disconnect(priv->modelSortedConnection_);
    QObject::disconnect(priv->filterChangedConnection_);
    QObject::disconnect(priv->newConversationConnection_);
    QObject::disconnect(priv->conversationRemovedConnection_);
    QObject::disconnect(priv->changeAccountConnection_);
    QObject::disconnect(priv->newAccountConnection_);
    QObject::disconnect(priv->rmAccountConnection_);
    QObject::disconnect(priv->showCallViewConnection_);
    QObject::disconnect(priv->modelSortedConnection_);
    QObject::disconnect(priv->accountStatusChangedConnection_);

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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, notebook_contacts);
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
