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
#include <callmodel.h>
#include <call.h>
#include <accountmodel.h>
#include <recentmodel.h>
#include <availableaccountmodel.h>
#include <contactrequest.h>
#include <pendingcontactrequestmodel.h>
#include <api/contactmodel.h>
#include <api/conversationmodel.h>
#include <api/lrc.h>
#include <api/newaccountmodel.h>
#include "accountcontainer.h"
#include "conversationcontainer.h"

// Ring client
#include "models/gtkqtreemodel.h"
#include "incomingcallview.h"
#include "currentcallview.h"
#include "accountview.h"
#include "dialogs.h"
#include "mediasettingsview.h"
#include "generalsettingsview.h"
#include "utils/accounts.h"
#include "ringwelcomeview.h"
#include "accountmigrationview.h"
#include "accountcreationwizard.h"
#include "conversationsview.h"
#include "chatview.h"
#include "utils/files.h"
#include "contactrequestcontentview.h"

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

    std::unique_ptr<lrc::api::Lrc> lrc_;
    AccountContainer* accountContainer_;
    ConversationContainer* chatViewConversation_;
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
change_view(RingMainWindow *self, GtkWidget* old, lrc::api::conversation::Info conversation, GType type)
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
        delete priv->chatViewConversation_;
        priv->chatViewConversation_ = new ConversationContainer(conversation);
        new_view = chat_view_new(get_webkit_chat_container(self), priv->accountContainer_, priv->chatViewConversation_);
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
    priv->accountContainer_->info.conversationModel->setFilter(text);
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

    // TODO compare conversations

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

    /* TODO priv->accountContainer_.info.conversationModel->setFilter(CONTACT)*/
    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    sync_all_view_selection(selection,
                            GTK_TREE_VIEW(priv->treeview_contact_requests));
}

static void
contact_request_selection_changed(GtkTreeSelection* selection, RingMainWindow* self)
{
    if (gtk_q_tree_model_ignore_selection_change(selection)) return;

    /* TODO priv->accountContainer_.info.conversationModel->setFilter(PENDING)*/

    auto priv = RING_MAIN_WINDOW_GET_PRIVATE(self);
    sync_all_view_selection(selection,
                            GTK_TREE_VIEW(priv->treeview_conversations));
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
    // TODO replace accountId by alias
    gchar *accountId;
    gchar *text;

    gtk_tree_model_get (model, iter,
                        0 /* col# */, &accountId /* data */,
                        -1);

    text = g_markup_printf_escaped(
        "<span fgcolor=\"gray\">%s</span>",
        accountId
    );

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(accountId);
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
    priv->lrc_ = std::unique_ptr<lrc::api::Lrc>(new lrc::api::Lrc());
    const auto accountIds = priv->lrc_->getAccountModel().getAccountList();
    if (accountIds.empty()) {
        qDebug() << "ring_main_window_init: empty account list";
        return;
    }
    priv->accountContainer_ = new AccountContainer(priv->lrc_->getAccountModel().getAccountInfo(accountIds.front()));

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

    /* populate the NEW notebook */
    priv->treeview_conversations = conversations_view_new(priv->accountContainer_/*, CONTACT */);
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window_smartview), priv->treeview_conversations);

    auto available_accounts_changed = [win, priv] {
        /* if we're hiding the settings it means we're in the migration or wizard view and we don't
         * want to show the combo box */
        if (gtk_widget_get_visible(priv->ring_settings))
            show_combobox_account_selector(win, TRUE);
    };

    // TODO connect to accountRemoved / accountAdded / changed
    QObject::connect(&AvailableAccountModel::instance(), &QAbstractItemModel::rowsRemoved, available_accounts_changed);
    QObject::connect(&AvailableAccountModel::instance(), &QAbstractItemModel::rowsInserted, available_accounts_changed);
    QObject::connect(AvailableAccountModel::instance().selectionModel(), &QItemSelectionModel::currentChanged,
    [win] (const QModelIndex& current, const QModelIndex& previous) {
        auto account = current.data(static_cast<int>(Account::Role::Object)).value<Account*>();
        current_account_changed(win, account);
    });

    priv->treeview_contact_requests = conversations_view_new(priv->accountContainer_/*, PENDING */);
    // TODO remove pending view
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

    /* TODO make sure the incoming call is the selected call in the CallModel */
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
    // TODO get list from string
    auto store = gtk_list_store_new (1 /* # of cols */ ,
                                     G_TYPE_STRING,
                                     G_TYPE_UINT);
    GtkTreeIter iter;

    // TODO listend for add/remove account
    for (const auto& accountId : accountIds) {
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
         0 /* col # */ , accountId.c_str() /* celldata */,
         -1 /* end */);
    }

    gtk_combo_box_set_model(
        GTK_COMBO_BOX(priv->combobox_account_selector),
        GTK_TREE_MODEL (store)
    );

    /* set visibility */
    show_combobox_account_selector(win, TRUE);
    // TODO
    gtk_combo_box_set_active(GTK_COMBO_BOX(priv->combobox_account_selector), 0);

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
    QObject::connect(&*priv->accountContainer_->info.conversationModel,
    &lrc::api::ConversationModel::showChatView,
    [win, priv] (lrc::api::conversation::Info origin) {
        // Change the view if we want a different view.
        auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));

        lrc::api::conversation::Info current_item;
        if (IS_CHAT_VIEW(old_view))
            current_item = chat_view_get_conversation(CHAT_VIEW(old_view));

        if (current_item.uid != origin.uid) {
            change_view(win, old_view, origin, CHAT_VIEW_TYPE);
        }
    });

    QObject::connect(&*priv->accountContainer_->info.conversationModel,
    &lrc::api::ConversationModel::modelUpdated,
    [win, priv] () {
        // Change the view if we want a different view.
        auto old_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));

        lrc::api::conversation::Info current_item;
        if (IS_CHAT_VIEW(old_view))
            current_item = chat_view_get_conversation(CHAT_VIEW(old_view));

        if (current_item.participants.empty()) {
            change_view(win, old_view, current_item, RING_WELCOME_VIEW_TYPE);
        } else if (!current_item.isUsed) {
            // if current conversation is temporary, change if it's not a contact
            auto uri = current_item.participants.front();
            try {
                // else the contact was added
                priv->accountContainer_->info.contactModel->getContact(uri);
                chat_view_update_temporary(CHAT_VIEW(old_view));
                gtk_entry_set_text(GTK_ENTRY(priv->search_entry), "");
            } catch (const std::out_of_range&) {
                change_view(win, old_view, current_item, RING_WELCOME_VIEW_TYPE);
            }
        } else {
            // change view if contact was removed
            auto uri = current_item.participants.front();
            try {
                priv->accountContainer_->info.contactModel->getContact(uri);
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

    delete priv->accountContainer_;
    delete priv->chatViewConversation_;

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
