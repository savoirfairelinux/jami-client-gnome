/*
 *  Copyright (C) 2015-2018 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
#include <api/profile.h>
#include <api/contactmodel.h>
#include <api/conversation.h>
#include <api/conversationmodel.h>
#include <api/lrc.h>
#include <api/newaccountmodel.h>
#include <api/newcallmodel.h>
#include <api/behaviorcontroller.h>
#include "accountcontainer.h"
#include <media/textrecording.h>
#include <media/recordingmodel.h>
#include <media/text.h>

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
#include "ringnotify.h"

//==============================================================================

namespace { namespace details
{
class CppImpl;
}}

struct _RingMainWindow
{
    GtkApplicationWindow parent;
};

struct _RingMainWindowClass
{
    GtkApplicationWindowClass parent_class;
};

struct RingMainWindowPrivate
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
    GtkWidget *webkit_chat_container; ///< The webkit_chat_container is created once, then reused for all chat views

    GSettings *settings;

    details::CppImpl* cpp; ///< Non-UI and C++ only code
};

G_DEFINE_TYPE_WITH_PRIVATE(RingMainWindow, ring_main_window, GTK_TYPE_APPLICATION_WINDOW);

#define RING_MAIN_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RING_MAIN_WINDOW_TYPE, RingMainWindowPrivate))

//==============================================================================

namespace { namespace details
{

static constexpr const char* CALL_VIEW_NAME                    = "calls";
static constexpr const char* ACCOUNT_CREATION_WIZARD_VIEW_NAME = "account-creation-wizard";
static constexpr const char* ACCOUNT_MIGRATION_VIEW_NAME       = "account-migration-view";
static constexpr const char* GENERAL_SETTINGS_VIEW_NAME        = "general";
static constexpr const char* MEDIA_SETTINGS_VIEW_NAME          = "media";
static constexpr const char* ACCOUNT_SETTINGS_VIEW_NAME        = "accounts";

inline namespace helpers
{

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
    (void)cell_layout;
    (void)data;

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

inline static void
foreachEnabledLrcAccount(const lrc::api::Lrc& lrc,
                         const std::function<void(const lrc::api::account::Info&)>& func)
{
    auto& account_model = lrc.getAccountModel();
    for (const auto& account_id : account_model.getAccountList()) {
        const auto& account_info = account_model.getAccountInfo(account_id);
        if (account_info.enabled)
            func(account_info);
    }
}

} // namespace helpers

class CppImpl
{
public:
    explicit CppImpl(RingMainWindow& widget);
    ~CppImpl();

    void init();
    void updateLrc(const std::string& accountId);
    void changeView(GType type, lrc::api::conversation::Info conversation = {});
    void enterFullScreen();
    void leaveFullScreen();
    void toggleFullScreen();
    void resetToWelcome();
    void refreshPendingContactRequestTab();
    void changeAccountSelection(const std::string& id);
    void onAccountSelectionChange(const std::string& id);
    void enterAccountCreationWizard();
    void leaveAccountCreationWizard();
    void enterSettingsView();
    void leaveSettingsView();

    void showAccountSelectorWidget(bool show = true);
    std::size_t refreshAccountSelectorWidget(int selection_row = -1, bool show = true);

    WebKitChatContainer* webkitChatContainer() const;

    RingMainWindow* self = nullptr; // The GTK widget itself
    RingMainWindowPrivate* widgets = nullptr;

    std::unique_ptr<lrc::api::Lrc> lrc_;
    std::unique_ptr<AccountContainer> accountContainer_;
    std::unique_ptr<lrc::api::conversation::Info> chatViewConversation_;
    lrc::api::profile::Type currentTypeFilter_;
    bool show_settings = false;
    bool is_fullscreen = false;
    bool has_cleared_all_history = false;

    int smartviewPageNum = 0;
    int contactRequestsPageNum = 0;

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
    QMetaObject::Connection chatNotification_;

private:
    CppImpl() = delete;
    CppImpl(const CppImpl&) = delete;
    CppImpl& operator=(const CppImpl&) = delete;

    GtkWidget* displayWelcomeView(lrc::api::conversation::Info);
    GtkWidget* displayIncomingView(lrc::api::conversation::Info);
    GtkWidget* displayCurrentCallView(lrc::api::conversation::Info);
    GtkWidget* displayChatView(lrc::api::conversation::Info);
    void chatNotifications();

    // Callbacks used as LRC Qt slot
    void slotAccountAddedFromLrc(const std::string& id);
    void slotAccountRemovedFromLrc(const std::string& id);
    void slotAccountStatusChanged(const std::string& id);
    void slotConversationCleared(const std::string& uid);
    void slotModelSorted();
    void slotFilterChanged();
    void slotNewConversation(const std::string& uid);
    void slotConversationRemoved(const std::string& uid);
    void slotShowChatView(const std::string& id, lrc::api::conversation::Info origin);
    void slotShowCallView(const std::string& id, lrc::api::conversation::Info origin);
    void slotShowIncomingCallView(const std::string& id, lrc::api::conversation::Info origin);
};

inline namespace gtk_callbacks
{

static gboolean
on_save_accounts_timeout(GtkWidget* working_dialog)
{
    /* save changes to accounts */
    AccountModel::instance().save();

    if (working_dialog)
        gtk_widget_destroy(working_dialog);

    return G_SOURCE_REMOVE;
}

static void
on_video_double_clicked(RingMainWindow* self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));
    priv->cpp->toggleFullScreen();
}

static void
on_hide_view_clicked(RingMainWindow* self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));
    priv->cpp->resetToWelcome();
}

static void
on_account_creation_completed(RingMainWindow* self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));
    priv->cpp->leaveAccountCreationWizard();
}

static void
on_account_changed(RingMainWindow* self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    auto accountComboBox = GTK_COMBO_BOX(priv->combobox_account_selector);
    auto model = gtk_combo_box_get_model(accountComboBox);

    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(accountComboBox, &iter)) {
        gchar* accountId;
        gtk_tree_model_get(model, &iter, 0 /* col# */, &accountId /* data */, -1);
        priv->cpp->onAccountSelectionChange(accountId);
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(priv->notebook_contacts),
                                   priv->cpp->accountContainer_->info.contactModel->hasPendingRequests());
        g_free(accountId);
    }
}

static void
on_settings_clicked(RingMainWindow* self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    if (!priv->cpp->show_settings)
        priv->cpp->enterSettingsView();
    else
        priv->cpp->leaveSettingsView();
}

static void
on_show_media_settings(GtkToggleButton* navbutton, RingMainWindow* self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    if (gtk_toggle_button_get_active(navbutton)) {
        media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(priv->media_settings_view), TRUE);
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), MEDIA_SETTINGS_VIEW_NAME);
        priv->last_settings_view = priv->media_settings_view;
    } else {
        media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(priv->media_settings_view), FALSE);
    }
}

static void
on_show_account_settings(GtkToggleButton* navbutton, RingMainWindow* self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    if (gtk_toggle_button_get_active(navbutton)) {
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), ACCOUNT_SETTINGS_VIEW_NAME);
        priv->last_settings_view = priv->account_settings_view;
    }
}

static void
on_show_general_settings(GtkToggleButton* navbutton, RingMainWindow* self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    if (gtk_toggle_button_get_active(navbutton)) {
        general_settings_view_show_profile(GENERAL_SETTINGS_VIEW(priv->general_settings_view), TRUE);
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), GENERAL_SETTINGS_VIEW_NAME);
        priv->last_settings_view = priv->general_settings_view;
    } else {
        general_settings_view_show_profile(GENERAL_SETTINGS_VIEW(priv->general_settings_view), FALSE);
    }
}

static void
on_search_entry_text_changed(GtkSearchEntry* search_entry, RingMainWindow* self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    // Filter model
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(search_entry));
    priv->cpp->accountContainer_->info.conversationModel->setFilter(text);
}

static void
on_search_entry_activated(RingMainWindow* self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    // Select the first conversation of the list
    auto& conversationModel = priv->cpp->accountContainer_->info.conversationModel;
    auto conversations = conversationModel->allFilteredConversations();
    if (!conversations.empty())
        conversationModel->selectConversation(conversations[0].uid);
}

static gboolean
on_search_entry_key_released(G_GNUC_UNUSED GtkEntry* search_entry, GdkEventKey* key, RingMainWindow* self)
{
    g_return_val_if_fail(IS_RING_MAIN_WINDOW(self), GDK_EVENT_PROPAGATE);
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    // if esc key pressed, clear the regex (keep the text, the user might not want to actually delete it)
    if (key->keyval == GDK_KEY_Escape) {
        priv->cpp->accountContainer_->info.conversationModel->setFilter("");
        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean
on_dtmf_pressed(RingMainWindow* self, GdkEventKey* event, gpointer user_data)
{
    (void)user_data;

    g_return_val_if_fail(IS_RING_MAIN_WINDOW(self), GDK_EVENT_PROPAGATE);
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    g_return_val_if_fail(event->type == GDK_KEY_PRESS, GDK_EVENT_PROPAGATE);
    g_return_val_if_fail(priv, GDK_EVENT_PROPAGATE);

    /* we want to react to digit key presses, as long as a GtkEntry is not the
     * input focus
     */
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(self));
    if (GTK_IS_ENTRY(focus))
        return GDK_EVENT_PROPAGATE;

    if (priv->cpp->accountContainer_ &&
        priv->cpp->accountContainer_->info.profileInfo.type != lrc::api::profile::Type::SIP)
        return GDK_EVENT_PROPAGATE;

    /* filter out cretain MOD masked key presses so that, for example, 'Ctrl+c'
     * does not result in a 'c' being played.
     * we filter Ctrl, Alt, and SUPER/HYPER/META keys */
    if ( event->state
       & ( GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ))
       return GDK_EVENT_PROPAGATE;

    // Get current conversation
    auto current_view = gtk_bin_get_child(GTK_BIN(priv->frame_call));
    lrc::api::conversation::Info current_item;
    if (IS_CURRENT_CALL_VIEW(current_view))
       current_item = current_call_view_get_conversation(CURRENT_CALL_VIEW(current_view));
    else
       return GDK_EVENT_PROPAGATE;

    if (current_item.callId.empty())
       return GDK_EVENT_PROPAGATE;

    // pass the character that was entered to be played by the daemon;
    // the daemon will filter out invalid DTMF characters
    guint32 unicode_val = gdk_keyval_to_unicode(event->keyval);
    QString val = QString::fromUcs4(&unicode_val, 1);
    g_debug("attemptingto play DTMF tone during ongoing call: %s", val.toUtf8().constData());
    priv->cpp->accountContainer_->info.callModel->playDTMF(current_item.callId, val.toStdString());
    // always propogate the key, so we don't steal accelerators/shortcuts
    return GDK_EVENT_PROPAGATE;
}

static void
on_tab_changed(GtkNotebook* notebook, GtkWidget* page, guint page_num, RingMainWindow* self)
{
    (void)notebook;
    (void)page;

    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    auto newType = page_num == 0 ? priv->cpp->accountContainer_->info.profileInfo.type : lrc::api::profile::Type::PENDING;
    if (priv->cpp->currentTypeFilter_ != newType) {
        priv->cpp->currentTypeFilter_ = newType;
        priv->cpp->accountContainer_->info.conversationModel->setFilter(priv->cpp->currentTypeFilter_);
    }
}

static gboolean
on_window_size_changed(GtkWidget* self, GdkEventConfigure* event, gpointer user_data)
{
    (void)user_data;

    g_return_val_if_fail(IS_RING_MAIN_WINDOW(self), GDK_EVENT_PROPAGATE);
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    g_settings_set_int(priv->settings, "window-width", event->width);
    g_settings_set_int(priv->settings, "window-height", event->height);

    return GDK_EVENT_PROPAGATE;
}

static void
on_search_entry_places_call_changed(GSettings* settings, const gchar* key, RingMainWindow* self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    if (g_settings_get_boolean(settings, key)) {
        gtk_widget_set_tooltip_text(priv->button_new_conversation,
                                    C_("button next to search entry will place a new call",
                                       "place call"));
    } else {
        gtk_widget_set_tooltip_text(priv->button_new_conversation,
                                    C_("button next to search entry will open chat", "open chat"));
    }
}

static void
on_handle_account_migrations(RingMainWindow* self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));

    /* If there is an existing migration view, remove it */
    if (priv->account_migration_view) {
        gtk_widget_destroy(priv->account_migration_view);
        priv->account_migration_view = nullptr;
    }

    auto accounts = AccountModel::instance().accountsToMigrate();
    if (!accounts.isEmpty()) {
        auto* account = accounts.first();

        priv->account_migration_view = account_migration_view_new(account);
        g_signal_connect_swapped(priv->account_migration_view, "account-migration-completed",
                                 G_CALLBACK(on_handle_account_migrations), self);
        g_signal_connect_swapped(priv->account_migration_view, "account-migration-failed",
                                 G_CALLBACK(on_handle_account_migrations), self);

        gtk_widget_hide(priv->ring_settings);
        priv->cpp->showAccountSelectorWidget(false);
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
    } else {
        gtk_widget_show(priv->ring_settings);
        priv->cpp->showAccountSelectorWidget();
    }
}

} // namespace gtk_callbacks

CppImpl::CppImpl(RingMainWindow& widget)
    : self {&widget}
    , widgets {RING_MAIN_WINDOW_GET_PRIVATE(&widget)}
    , lrc_ {std::make_unique<lrc::api::Lrc>()}
{}

void
CppImpl::chatNotifications()
{
    chatNotification_ = QObject::connect(
        &Media::RecordingModel::instance(),
        &Media::RecordingModel::newTextMessage,
        [this] (Media::TextRecording* t, ContactMethod* cm) {
            if ((chatViewConversation_
                && chatViewConversation_->participants[0] == cm->uri().toStdString())
                || not g_settings_get_boolean(widgets->settings, "enable-chat-notifications"))
                    return;

            ring_notify_message(cm, t);
        }
    );
}

static gboolean
on_clear_all_history_foreach(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer self)
{
    g_return_val_if_fail(IS_RING_MAIN_WINDOW(self), TRUE);

    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));
    const gchar* account_id;

    gtk_tree_model_get(model, iter, 0 /* col# */, &account_id /* data */, -1);

    auto& accountInfo = priv->cpp->lrc_->getAccountModel().getAccountInfo(account_id);
    accountInfo.conversationModel->clearAllHistory();

    return FALSE;
}

static void
on_clear_all_history_clicked(RingMainWindow* self)
{
    g_return_if_fail(IS_RING_MAIN_WINDOW(self));
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(RING_MAIN_WINDOW(self));
    auto accountComboBox = GTK_COMBO_BOX(priv->combobox_account_selector);
    auto model = gtk_combo_box_get_model(accountComboBox);

    gtk_tree_model_foreach (model, on_clear_all_history_foreach, self);

    priv->cpp->has_cleared_all_history = true;
}

void
CppImpl::init()
{
    // Remember the tabs page number for easier selection later
    smartviewPageNum = gtk_notebook_page_num(GTK_NOTEBOOK(widgets->notebook_contacts),
                                             widgets->scrolled_window_smartview);
    contactRequestsPageNum = gtk_notebook_page_num(GTK_NOTEBOOK(widgets->notebook_contacts),
                                                   widgets->scrolled_window_contact_requests);
    g_assert(smartviewPageNum != contactRequestsPageNum);

    // NOTE: When new models will be fully implemented, we need to move this
    // in rign_client.cpp->
    // Init LRC and the vew
    const auto accountIds = lrc_->getAccountModel().getAccountList();
    decltype(accountIds)::value_type activeAccountId; // non-empty if a enabled account is found below

    if (not accountIds.empty()) {
        for (const auto& id : accountIds) {
            const auto& accountInfo = lrc_->getAccountModel().getAccountInfo(id);
            if (accountInfo.enabled) {
                activeAccountId = id;
                break;
            }
        }
    }

    if (!activeAccountId.empty()) {
        updateLrc(activeAccountId);
    } else {
        // No enabled account: create empty widgets
        widgets->treeview_conversations = conversations_view_new(nullptr);
        gtk_container_add(GTK_CONTAINER(widgets->scrolled_window_smartview), widgets->treeview_conversations);
        widgets->treeview_contact_requests = conversations_view_new(nullptr);
        gtk_container_add(GTK_CONTAINER(widgets->scrolled_window_contact_requests), widgets->treeview_contact_requests);
    }

    accountStatusChangedConnection_ = QObject::connect(&lrc_->getAccountModel(),
                                                       &lrc::api::NewAccountModel::accountStatusChanged,
                                                       [this](const std::string& id){ slotAccountStatusChanged(id); });
    newAccountConnection_ = QObject::connect(&lrc_->getAccountModel(),
                                             &lrc::api::NewAccountModel::accountAdded,
                                             [this](const std::string& id){ slotAccountAddedFromLrc(id); });
    rmAccountConnection_ = QObject::connect(&lrc_->getAccountModel(),
                                            &lrc::api::NewAccountModel::accountRemoved,
                                            [this](const std::string& id){ slotAccountRemovedFromLrc(id); });

    /* bind to window size settings */
    widgets->settings = g_settings_new_full(get_ring_schema(), nullptr, nullptr);
    auto width = g_settings_get_int(widgets->settings, "window-width");
    auto height = g_settings_get_int(widgets->settings, "window-height");
    gtk_window_set_default_size(GTK_WINDOW(self), width, height);
    g_signal_connect(self, "configure-event", G_CALLBACK(on_window_size_changed), nullptr);

    /* search-entry-places-call setting */
    on_search_entry_places_call_changed(widgets->settings, "search-entry-places-call", self);
    g_signal_connect(widgets->settings, "changed::search-entry-places-call",
                     G_CALLBACK(on_search_entry_places_call_changed), self);

    /* set window icon */
    GError *error = NULL;
    GdkPixbuf* icon = gdk_pixbuf_new_from_resource("/cx/ring/RingGnome/ring-symbol-blue", &error);
    if (icon == NULL) {
        g_debug("Could not load icon: %s", error->message);
        g_clear_error(&error);
    } else
        gtk_window_set_icon(GTK_WINDOW(self), icon);

    /* set menu icon */
    GdkPixbuf* image_ring = gdk_pixbuf_new_from_resource_at_scale("/cx/ring/RingGnome/ring-symbol-blue",
                                                                  -1, 16, TRUE, &error);
    if (image_ring == NULL) {
        g_debug("Could not load icon: %s", error->message);
        g_clear_error(&error);
    } else
        gtk_image_set_from_pixbuf(GTK_IMAGE(widgets->image_ring), image_ring);

    /* ring menu */
    GtkBuilder *builder = gtk_builder_new_from_resource("/cx/ring/RingGnome/ringgearsmenu.ui");
    GMenuModel *menu = G_MENU_MODEL(gtk_builder_get_object(builder, "menu"));
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(widgets->ring_menu), menu);
    g_object_unref(builder);

    /* settings icon */
    gtk_image_set_from_icon_name(GTK_IMAGE(widgets->image_settings), "emblem-system-symbolic",
                                 GTK_ICON_SIZE_SMALL_TOOLBAR);

    /* connect settings button signal */
    g_signal_connect_swapped(widgets->ring_settings, "clicked",
                             G_CALLBACK(on_settings_clicked), self);

    /* add the call view to the main stack */
    gtk_stack_add_named(GTK_STACK(widgets->stack_main_view), widgets->vbox_call_view,
                        CALL_VIEW_NAME);

    /* init the settings views */
    widgets->account_settings_view = account_view_new();
    gtk_stack_add_named(GTK_STACK(widgets->stack_main_view), widgets->account_settings_view,
                        ACCOUNT_SETTINGS_VIEW_NAME);

    widgets->media_settings_view = media_settings_view_new();
    gtk_stack_add_named(GTK_STACK(widgets->stack_main_view), widgets->media_settings_view,
                        MEDIA_SETTINGS_VIEW_NAME);

    widgets->general_settings_view = general_settings_view_new();
    gtk_stack_add_named(GTK_STACK(widgets->stack_main_view), widgets->general_settings_view,
                        GENERAL_SETTINGS_VIEW_NAME);
    g_signal_connect_swapped(widgets->general_settings_view, "clear-all-history", G_CALLBACK(on_clear_all_history_clicked), self);


    /* make the setting we will show first the active one */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets->radiobutton_general_settings), TRUE);
    widgets->last_settings_view = widgets->general_settings_view;

    /* connect the settings button signals to switch settings views */
    g_signal_connect(widgets->radiobutton_media_settings, "toggled", G_CALLBACK(on_show_media_settings), self);
    g_signal_connect(widgets->radiobutton_account_settings, "toggled", G_CALLBACK(on_show_account_settings), self);
    g_signal_connect(widgets->radiobutton_general_settings, "toggled", G_CALLBACK(on_show_general_settings), self);
    g_signal_connect(widgets->notebook_contacts, "switch-page", G_CALLBACK(on_tab_changed), self);

    /* welcome/default view */
    widgets->welcome_view = ring_welcome_view_new(accountContainer_.get());
    g_object_ref(widgets->welcome_view); // increase ref because don't want it to be destroyed when not displayed
    gtk_container_add(GTK_CONTAINER(widgets->frame_call), widgets->welcome_view);
    gtk_widget_show(widgets->welcome_view);

    g_signal_connect_swapped(widgets->search_entry, "activate", G_CALLBACK(on_search_entry_activated), self);
    g_signal_connect_swapped(widgets->button_new_conversation, "clicked", G_CALLBACK(on_search_entry_activated), self);
    g_signal_connect(widgets->search_entry, "search-changed", G_CALLBACK(on_search_entry_text_changed), self);
    g_signal_connect(widgets->search_entry, "key-release-event", G_CALLBACK(on_search_entry_key_released), self);

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
    g_signal_connect(self, "key-press-event", G_CALLBACK(on_dtmf_pressed), nullptr);

    /* set the search entry placeholder text */
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->search_entry),
                                   C_("Please try to make the translation 50 chars or less so that it fits into the layout",
                                      "Search contacts or enter number"));

    /* init chat webkit container so that it starts loading before the first time we need it*/
    (void)webkitChatContainer();

    if (has_ring_account()) {
        /* user has ring account, so show the call view right away */
        gtk_stack_set_visible_child(GTK_STACK(widgets->stack_main_view), widgets->vbox_call_view);

        on_handle_account_migrations(self);
    } else {
        /* user has to create the ring account */
        enterAccountCreationWizard();
    }

    // setup account selector and select the first account
    refreshAccountSelectorWidget(0);

    /* layout */
    auto *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(widgets->combobox_account_selector), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(widgets->combobox_account_selector), renderer, "text", 0, NULL);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(widgets->combobox_account_selector),
                                       renderer,
                                       (GtkCellLayoutDataFunc)print_account_and_state,
                                       nullptr, nullptr);

    // we closing any view opened to avoid confusion (especially between SIP and Ring protocols).
    g_signal_connect_swapped(widgets->combobox_account_selector, "changed", G_CALLBACK(on_account_changed), self);

    // initialize the pending contact request icon.
    refreshPendingContactRequestTab();

    if (accountContainer_) {
        auto& conversationModel = accountContainer_->info.conversationModel;
        auto conversations = conversationModel->allFilteredConversations();
        for (const auto& conversation: conversations) {
            if (!conversation.callId.empty()) {
                accountContainer_->info.conversationModel->selectConversation(conversation.uid);
            }
        }
    }

    // setup chat notification
    chatNotifications();

    // delete obsolete history
    if (not accountIds.empty()) {
        auto days = g_settings_get_int(widgets->settings, "history-limit");
        for (auto& accountId : accountIds) {
            auto& accountInfo = lrc_->getAccountModel().getAccountInfo(accountId);
            accountInfo.conversationModel->deleteObsoleteHistory(days);
        }
    }

}

CppImpl::~CppImpl()
{
    QObject::disconnect(showChatViewConnection_);
    QObject::disconnect(showIncomingViewConnection_);
    QObject::disconnect(historyClearedConnection_);
    QObject::disconnect(modelSortedConnection_);
    QObject::disconnect(filterChangedConnection_);
    QObject::disconnect(newConversationConnection_);
    QObject::disconnect(conversationRemovedConnection_);
    QObject::disconnect(changeAccountConnection_);
    QObject::disconnect(newAccountConnection_);
    QObject::disconnect(rmAccountConnection_);
    QObject::disconnect(showCallViewConnection_);
    QObject::disconnect(modelSortedConnection_);
    QObject::disconnect(accountStatusChangedConnection_);
    QObject::disconnect(chatNotification_);

    g_clear_object(&widgets->welcome_view);
    g_clear_object(&widgets->webkit_chat_container);
}

void
CppImpl::changeView(GType type, lrc::api::conversation::Info conversation)
{
    leaveFullScreen();
    gtk_container_remove(GTK_CONTAINER(widgets->frame_call),
                         gtk_bin_get_child(GTK_BIN(widgets->frame_call)));

    GtkWidget* new_view;
    if (g_type_is_a(INCOMING_CALL_VIEW_TYPE, type)) {
        new_view = displayIncomingView(conversation);
    } else if (g_type_is_a(CURRENT_CALL_VIEW_TYPE, type)) {
        new_view = displayCurrentCallView(conversation);
    } else if (g_type_is_a(CHAT_VIEW_TYPE, type)) {
        new_view = displayChatView(conversation);
    } else {
        new_view = displayWelcomeView(conversation);
    }

    gtk_container_add(GTK_CONTAINER(widgets->frame_call), new_view);
    gtk_widget_show(new_view);
}

GtkWidget*
CppImpl::displayWelcomeView(lrc::api::conversation::Info conversation)
{
    (void) conversation;

    // TODO select first conversation?

    chatViewConversation_.reset(nullptr);
    refreshPendingContactRequestTab();

    return widgets->welcome_view;;
}

GtkWidget*
CppImpl::displayIncomingView(lrc::api::conversation::Info conversation)
{
    chatViewConversation_.reset(new lrc::api::conversation::Info(conversation));
    return incoming_call_view_new(webkitChatContainer(), accountContainer_.get(), chatViewConversation_.get());
}

GtkWidget*
CppImpl::displayCurrentCallView(lrc::api::conversation::Info conversation)
{
    chatViewConversation_.reset(new lrc::api::conversation::Info(conversation));
    auto* new_view = current_call_view_new(webkitChatContainer(),
                                           accountContainer_.get(), chatViewConversation_.get());

    try {
        auto contactUri = chatViewConversation_->participants.front();
        auto contactInfo = accountContainer_->info.contactModel->getContact(contactUri);
        if (auto chat_view = current_call_view_get_chat_view(CURRENT_CALL_VIEW(new_view))) {
            chat_view_update_temporary(CHAT_VIEW(chat_view),
                                       contactInfo.profileInfo.type == lrc::api::profile::Type::PENDING
                                       || contactInfo.profileInfo.type == lrc::api::profile::Type::TEMPORARY);
        }
    } catch(...) { }

    g_signal_connect_swapped(new_view, "video-double-clicked",
                             G_CALLBACK(on_video_double_clicked), self);
    return new_view;
}

GtkWidget*
CppImpl::displayChatView(lrc::api::conversation::Info conversation)
{
    chatViewConversation_.reset(new lrc::api::conversation::Info(conversation));
    auto* new_view = chat_view_new(webkitChatContainer(), accountContainer_.get(), chatViewConversation_.get());
    g_signal_connect_swapped(new_view, "hide-view-clicked", G_CALLBACK(on_hide_view_clicked), self);
    return new_view;
}

WebKitChatContainer*
CppImpl::webkitChatContainer() const
{
    if (!widgets->webkit_chat_container) {
        widgets->webkit_chat_container = webkit_chat_container_new();

        // We don't want it to be deleted, ever.
        g_object_ref(widgets->webkit_chat_container);
    }
    return WEBKIT_CHAT_CONTAINER(widgets->webkit_chat_container);
}

void
CppImpl::enterFullScreen()
{
    if (!is_fullscreen) {
        gtk_widget_hide(widgets->vbox_left_pane);
        gtk_window_fullscreen(GTK_WINDOW(self));
        is_fullscreen = true;
    }
}

void
CppImpl::leaveFullScreen()
{
    if (is_fullscreen) {
        gtk_widget_show(widgets->vbox_left_pane);
        gtk_window_unfullscreen(GTK_WINDOW(self));
        is_fullscreen = false;
    }
}

void
CppImpl::toggleFullScreen()
{
    if (is_fullscreen)
        leaveFullScreen();
    else
        enterFullScreen();
}

/// Clear selection and go to welcome page.
void
CppImpl::resetToWelcome()
{
    auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(widgets->treeview_conversations));
    gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_conversations));
    auto selection_contact_request = gtk_tree_view_get_selection(GTK_TREE_VIEW(widgets->treeview_contact_requests));
    gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_contact_request));
    auto* old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
    lrc::api::conversation::Info current_item;
    if (IS_CHAT_VIEW(old_view))
        current_item = chat_view_get_conversation(CHAT_VIEW(old_view));
    changeView(RING_WELCOME_VIEW_TYPE, current_item);
}

void
CppImpl::refreshPendingContactRequestTab()
{
    if (not accountContainer_)
        return;

    auto hasPendingRequests = accountContainer_->info.contactModel->hasPendingRequests();
    gtk_widget_set_visible(widgets->scrolled_window_contact_requests, hasPendingRequests);
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(widgets->notebook_contacts), hasPendingRequests);

    // show conversation page if PendingRequests list is empty
    if (not hasPendingRequests) {
        auto current_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(widgets->notebook_contacts));
        if (current_page == contactRequestsPageNum)
            gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets->notebook_contacts), smartviewPageNum);
    }
}

void
CppImpl::showAccountSelectorWidget(bool show)
{
    // we only want to show the account selector when there is more than 1 enabled
    // account; so every time we want to show it, we should preform this check
    std::size_t enabled_accounts = 0;
    foreachEnabledLrcAccount(*lrc_, [&] (const auto&) { ++enabled_accounts; });
    gtk_widget_set_visible(widgets->combobox_account_selector, show && enabled_accounts > 1);
}

/// Update the account GtkComboBox from LRC data and select the given entry.
/// The widget visibility is changed depending on number of account found.
/// /note Default selection_row is -1, meaning no selection.
/// /note Default visibility mode is true, meaning showing the widget.
std::size_t
CppImpl::refreshAccountSelectorWidget(int selection_row, bool show)
{
    auto store = gtk_list_store_new(2 /* # of cols */ ,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_UINT);
    GtkTreeIter iter;
    std::size_t enabled_accounts = 0;
    foreachEnabledLrcAccount(*lrc_, [&] (const auto& acc_info) {
            ++enabled_accounts;
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                               0 /* col # */ , acc_info.id.c_str() /* celldata */,
                               1 /* col # */ , acc_info.profileInfo.alias.c_str() /* celldata */,
                               -1 /* end */);
        });

    gtk_combo_box_set_model(
        GTK_COMBO_BOX(widgets->combobox_account_selector),
        GTK_TREE_MODEL (store)
    );
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_account_selector), selection_row);
    gtk_widget_set_visible(widgets->combobox_account_selector, show && enabled_accounts > 1);

    return enabled_accounts;
}

void
CppImpl::enterAccountCreationWizard()
{
    if (!widgets->account_creation_wizard) {
        widgets->account_creation_wizard = account_creation_wizard_new(false);
        g_object_add_weak_pointer(G_OBJECT(widgets->account_creation_wizard),
                                  reinterpret_cast<gpointer*>(&widgets->account_creation_wizard));
        g_signal_connect_swapped(widgets->account_creation_wizard, "account-creation-completed",
                                 G_CALLBACK(on_account_creation_completed), self);

        gtk_stack_add_named(GTK_STACK(widgets->stack_main_view),
                            widgets->account_creation_wizard,
                            ACCOUNT_CREATION_WIZARD_VIEW_NAME);
    }

    /* hide settings button until account creation is complete */
    gtk_widget_hide(widgets->ring_settings);
    showAccountSelectorWidget(false);

    gtk_widget_show(widgets->account_creation_wizard);
    gtk_stack_set_visible_child(GTK_STACK(widgets->stack_main_view), widgets->account_creation_wizard);
}

void
CppImpl::leaveAccountCreationWizard()
{
    gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack_main_view), CALL_VIEW_NAME);

    /* destroy the wizard */
    if (widgets->account_creation_wizard) {
        gtk_container_remove(GTK_CONTAINER(widgets->stack_main_view), widgets->account_creation_wizard);
        gtk_widget_destroy(widgets->account_creation_wizard);
        widgets->account_creation_wizard = nullptr;
    }

    /* show the settings button */
    gtk_widget_show(widgets->ring_settings);

    /* show the account selector */
    showAccountSelectorWidget();
}

/// Change the selection of the account ComboBox by account Id.
/// Find in displayed accounts with one corresponding to the given id, then select it
void
CppImpl::changeAccountSelection(const std::string& id)
{
    // already selected?
    if (id == accountContainer_->info.id)
        return;

    if (auto* model = gtk_combo_box_get_model(GTK_COMBO_BOX(widgets->combobox_account_selector))) {
        GtkTreeIter iter;
        auto valid = gtk_tree_model_get_iter_first(model, &iter);
        while (valid) {
            const gchar* account_id;
            gtk_tree_model_get(model, &iter, 0 /* col# */, &account_id /* data */, -1);
            if (id == account_id) {
                gtk_combo_box_set_active_iter(GTK_COMBO_BOX(widgets->combobox_account_selector), &iter);
                return;
            }
            valid = gtk_tree_model_iter_next(model, &iter);
        }

        g_debug("BUGS: account not listed: %s", id.c_str());
    }
}

void
CppImpl::onAccountSelectionChange(const std::string& id)
{
    // Go to welcome view
    changeView(RING_WELCOME_VIEW_TYPE);
    // Show conversation panel
    gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets->notebook_contacts), 0);
    // Reinit LRC
    updateLrc(id);
    // Update the welcome view
    ring_welcome_update_view(RING_WELCOME_VIEW(widgets->welcome_view), accountContainer_.get());
}

void
CppImpl::enterSettingsView()
{
    /* show the settings */
    show_settings = true;

    /* show settings */
    gtk_image_set_from_icon_name(GTK_IMAGE(widgets->image_settings), "emblem-ok-symbolic",
                                 GTK_ICON_SIZE_SMALL_TOOLBAR);

    gtk_widget_show(widgets->hbox_settings);

    /* make sure to start preview if we're showing the video settings */
    if (widgets->last_settings_view == widgets->media_settings_view)
        media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(widgets->media_settings_view), TRUE);

    /* make sure to show the profile if we're showing the general settings */
    if (widgets->last_settings_view == widgets->general_settings_view)
        general_settings_view_show_profile(GENERAL_SETTINGS_VIEW(widgets->general_settings_view),
                                           TRUE);

    gtk_stack_set_visible_child(GTK_STACK(widgets->stack_main_view), widgets->last_settings_view);
}

void
CppImpl::leaveSettingsView()
{
    /* hide the settings */
    show_settings = false;

    /* show working dialog in case save operation takes time */
    auto working = ring_dialog_working(GTK_WIDGET(self), nullptr);
    gtk_window_present(GTK_WINDOW(working));

    /* now save after the time it takes to transition back to the call view (400ms)
     * the save doesn't happen before the "working" dialog is presented
     * the timeout function should destroy the "working" dialog when done saving
     */
    g_timeout_add_full(G_PRIORITY_DEFAULT, 400, GSourceFunc(on_save_accounts_timeout), working,
                       nullptr);

    /* show calls */
    gtk_image_set_from_icon_name(GTK_IMAGE(widgets->image_settings), "emblem-system-symbolic",
                                 GTK_ICON_SIZE_SMALL_TOOLBAR);

    gtk_widget_hide(widgets->hbox_settings);

    /* make sure video preview is stopped, in case it was started */
    media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(widgets->media_settings_view), FALSE);
    general_settings_view_show_profile(GENERAL_SETTINGS_VIEW(widgets->general_settings_view),
                                       FALSE);

    gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack_main_view), CALL_VIEW_NAME);

    /* return to the welcome view if has_cleared_all_history. The reason is you can have been in a chatview before you
     * opened the settings view and did a clear all history. So without the code below, you'll see the chatview with
     * obsolete messages. It will also ensure to refresh last interaction printed in the conversations list.
     */
    if (has_cleared_all_history) {
        onAccountSelectionChange(accountContainer_->info.id);
        resetToWelcome();
        has_cleared_all_history = false;
    }
}

void
CppImpl::updateLrc(const std::string& id)
{
    // Disconnect old signals.
    QObject::disconnect(showChatViewConnection_);
    QObject::disconnect(showIncomingViewConnection_);
    QObject::disconnect(changeAccountConnection_);
    QObject::disconnect(showCallViewConnection_);
    QObject::disconnect(modelSortedConnection_);
    QObject::disconnect(historyClearedConnection_);
    QObject::disconnect(filterChangedConnection_);
    QObject::disconnect(newConversationConnection_);
    QObject::disconnect(conversationRemovedConnection_);

    // Get the account selected
    if (!id.empty())
        accountContainer_.reset(new AccountContainer(lrc_->getAccountModel().getAccountInfo(id)));
    else
        accountContainer_.reset();

    // Reinit tree views
    if (widgets->treeview_conversations) {
        auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(widgets->treeview_conversations));
        gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_conversations));
        gtk_widget_destroy(widgets->treeview_conversations);
    }
    widgets->treeview_conversations = conversations_view_new(accountContainer_.get());
    gtk_container_add(GTK_CONTAINER(widgets->scrolled_window_smartview), widgets->treeview_conversations);

    if (widgets->treeview_contact_requests) {
        auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(widgets->treeview_contact_requests));
        gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_conversations));
        gtk_widget_destroy(widgets->treeview_contact_requests);
    }
    widgets->treeview_contact_requests = conversations_view_new(accountContainer_.get());
    gtk_container_add(GTK_CONTAINER(widgets->scrolled_window_contact_requests), widgets->treeview_contact_requests);

    if (!accountContainer_) return;

    // Connect to signals from LRC
    historyClearedConnection_ = QObject::connect(&*accountContainer_->info.conversationModel,
                                                 &lrc::api::ConversationModel::conversationCleared,
                                                 [this] (const std::string& id) { slotConversationCleared(id); });

    modelSortedConnection_ = QObject::connect(&*accountContainer_->info.conversationModel,
                                              &lrc::api::ConversationModel::modelSorted,
                                              [this] { slotModelSorted(); });

    filterChangedConnection_ = QObject::connect(&*accountContainer_->info.conversationModel,
                                                &lrc::api::ConversationModel::filterChanged,
                                                [this] { slotFilterChanged(); });

    newConversationConnection_ = QObject::connect(&*accountContainer_->info.conversationModel,
                                                  &lrc::api::ConversationModel::newConversation,
                                                  [this] (const std::string& id) { slotNewConversation(id); });

    conversationRemovedConnection_ = QObject::connect(&*accountContainer_->info.conversationModel,
                                                      &lrc::api::ConversationModel::conversationRemoved,
                                                      [this] (const std::string& id) { slotConversationRemoved(id); });

    showChatViewConnection_ = QObject::connect(&lrc_->getBehaviorController(),
                                               &lrc::api::BehaviorController::showChatView,
                                               [this] (const std::string& id, lrc::api::conversation::Info origin) { slotShowChatView(id, origin); });

    showCallViewConnection_ = QObject::connect(&lrc_->getBehaviorController(),
                                               &lrc::api::BehaviorController::showCallView,
                                               [this] (const std::string& id, lrc::api::conversation::Info origin) { slotShowCallView(id, origin); });

    showIncomingViewConnection_ = QObject::connect(&lrc_->getBehaviorController(),
                                                   &lrc::api::BehaviorController::showIncomingCallView,
                                                   [this] (const std::string& id, lrc::api::conversation::Info origin) { slotShowIncomingCallView(id, origin); });

    const gchar *text = gtk_entry_get_text(GTK_ENTRY(widgets->search_entry));
    currentTypeFilter_ = accountContainer_->info.profileInfo.type;
    accountContainer_->info.conversationModel->setFilter(text);
    accountContainer_->info.conversationModel->setFilter(currentTypeFilter_);
}

void
CppImpl::slotAccountAddedFromLrc(const std::string& id)
{
    (void)id;
    auto currentIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(widgets->combobox_account_selector));
    if (currentIdx == -1)
        currentIdx = 0; // If no account selected, select the first account
    refreshAccountSelectorWidget(currentIdx);
}

void
CppImpl::slotAccountRemovedFromLrc(const std::string& id)
{
    (void)id;

    auto accounts = lrc_->getAccountModel().getAccountList();
    if (accounts.empty()) {
        enterAccountCreationWizard();
        return;
    }

    // Update Account selector
    refreshAccountSelectorWidget(0);
    // Show conversation panel
    gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets->notebook_contacts), 0);
    // Reinit LRC
    updateLrc(std::string(accounts.at(0)));
    // Update the welcome view
    ring_welcome_update_view(RING_WELCOME_VIEW(widgets->welcome_view), accountContainer_.get());
}

void
CppImpl::slotAccountStatusChanged(const std::string& id)
{
    if (not accountContainer_ ) {
        updateLrc(id);
        ring_welcome_update_view(RING_WELCOME_VIEW(widgets->welcome_view),
                                 accountContainer_.get());
        return;
    }

    auto selectionIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(widgets->combobox_account_selector));

    // keep current selected account only if account status is enabled
    try {
        if (!lrc_->getAccountModel().getAccountInfo(id).enabled)
            selectionIdx = 0;
    } catch (...) {}

    auto enabledAccounts = refreshAccountSelectorWidget(selectionIdx);

    // if no account. reset the view.
    if (enabledAccounts == 0) {
        updateLrc("");
        changeView(RING_WELCOME_VIEW_TYPE);
        ring_welcome_update_view(RING_WELCOME_VIEW(widgets->welcome_view), nullptr);
    }
}

void
CppImpl::slotConversationCleared(const std::string& uid)
{
    // Change the view when the history is cleared.
    auto* old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
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
        resetToWelcome();
    }
}

void
CppImpl::slotModelSorted()
{
    // Synchronize selection when sorted and update pending icon
    auto* old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
    lrc::api::conversation::Info current_item;
    current_item.uid = "-1";
    if (IS_CHAT_VIEW(old_view))
        current_item = chat_view_get_conversation(CHAT_VIEW(old_view));
    else if (IS_CURRENT_CALL_VIEW(old_view))
        current_item = current_call_view_get_conversation(CURRENT_CALL_VIEW(old_view));
    else if (IS_INCOMING_CALL_VIEW(old_view))
        current_item = incoming_call_view_get_conversation(INCOMING_CALL_VIEW(old_view));
    conversations_view_select_conversation(CONVERSATIONS_VIEW(widgets->treeview_conversations), current_item.uid);
    refreshPendingContactRequestTab();
}

void
CppImpl::slotFilterChanged()
{
    // Synchronize selection when filter changes
    auto* old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
    lrc::api::conversation::Info current_item;
    current_item.uid = "-1";
    if (IS_CHAT_VIEW(old_view))
        current_item = chat_view_get_conversation(CHAT_VIEW(old_view));
    else if (IS_CURRENT_CALL_VIEW(old_view))
        current_item = current_call_view_get_conversation(CURRENT_CALL_VIEW(old_view));
    else if (IS_INCOMING_CALL_VIEW(old_view))
        current_item = incoming_call_view_get_conversation(INCOMING_CALL_VIEW(old_view));
    conversations_view_select_conversation(CONVERSATIONS_VIEW(widgets->treeview_conversations), current_item.uid);
    // Get if conversation still exists.
    auto& conversationModel = accountContainer_->info.conversationModel;
    auto conversations = conversationModel->allFilteredConversations();
    auto isInConv = std::find_if(
        conversations.begin(), conversations.end(),
        [current_item](const lrc::api::conversation::Info& conversation) {
            return current_item.uid == conversation.uid;
        });
    if (IS_CHAT_VIEW(old_view) && isInConv == conversations.end()) {
        changeView(RING_WELCOME_VIEW_TYPE);
    }
}

void
CppImpl::slotNewConversation(const std::string& uid)
{
    gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets->notebook_contacts), 0);
    accountContainer_->info.conversationModel->setFilter(lrc::api::profile::Type::RING);
    gtk_entry_set_text(GTK_ENTRY(widgets->search_entry), "");
    accountContainer_->info.conversationModel->setFilter("");
    // Select new conversation if contact added
    auto* old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
    if (IS_RING_WELCOME_VIEW(old_view) || (IS_CHAT_VIEW(old_view) && chat_view_get_temporary(CHAT_VIEW(old_view)))) {
        accountContainer_->info.conversationModel->selectConversation(uid);
        try {
            auto contactUri =  chatViewConversation_->participants.front();
            auto contactInfo = accountContainer_->info.contactModel->getContact(contactUri);
            chat_view_update_temporary(CHAT_VIEW(gtk_bin_get_child(GTK_BIN(widgets->frame_call))),
                                       contactInfo.profileInfo.type == lrc::api::profile::Type::PENDING
                                       || contactInfo.profileInfo.type == lrc::api::profile::Type::TEMPORARY);
        } catch(...) { }
    }
}

void
CppImpl::slotConversationRemoved(const std::string& uid)
{
    // If contact is removed, go to welcome view
    auto* old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
    lrc::api::conversation::Info current_item;
    if (IS_CHAT_VIEW(old_view))
        current_item = chat_view_get_conversation(CHAT_VIEW(old_view));
    else if (IS_CURRENT_CALL_VIEW(old_view))
        current_item = current_call_view_get_conversation(CURRENT_CALL_VIEW(old_view));
    else if (IS_INCOMING_CALL_VIEW(old_view))
        current_item = incoming_call_view_get_conversation(INCOMING_CALL_VIEW(old_view));
    if (current_item.uid == uid)
        changeView(RING_WELCOME_VIEW_TYPE);
}

void
CppImpl::slotShowChatView(const std::string& id, lrc::api::conversation::Info origin)
{
    changeAccountSelection(id);
    // Show chat view if not in call (unless if it's the same conversation)
    auto* old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
    lrc::api::conversation::Info current_item;
    current_item.uid = "-1";
    if (IS_CHAT_VIEW(old_view))
        current_item = chat_view_get_conversation(CHAT_VIEW(old_view));
    if (current_item.uid != origin.uid) {
        if (origin.participants.empty()) return;
        auto firstContactUri = origin.participants.front();
        auto contactInfo = accountContainer_->info.contactModel->getContact(firstContactUri);
        if (contactInfo.profileInfo.type == lrc::api::profile::Type::PENDING && current_item.uid != "-1") return;
        changeView(CHAT_VIEW_TYPE, origin);
    }
}

void
CppImpl::slotShowCallView(const std::string& id, lrc::api::conversation::Info origin)
{
    changeAccountSelection(id);
    // Change the view if we want a different view.
    auto* old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));

    lrc::api::conversation::Info current_item;
    if (IS_CURRENT_CALL_VIEW(old_view))
        current_item = current_call_view_get_conversation(CURRENT_CALL_VIEW(old_view));

    if (current_item.uid != origin.uid)
        changeView(CURRENT_CALL_VIEW_TYPE, origin);
}

void
CppImpl::slotShowIncomingCallView(const std::string& id, lrc::api::conversation::Info origin)
{
    changeAccountSelection(id);
    // Change the view if we want a different view.
    auto* old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));

    lrc::api::conversation::Info current_item;
    if (IS_INCOMING_CALL_VIEW(old_view))
        current_item = incoming_call_view_get_conversation(INCOMING_CALL_VIEW(old_view));

    if (current_item.uid != origin.uid)
        changeView(INCOMING_CALL_VIEW_TYPE, origin);
}

}} // namespace <anonymous>::details

//==============================================================================

static void
ring_main_window_init(RingMainWindow *win)
{
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(win);
    gtk_widget_init_template(GTK_WIDGET(win));

    // CppImpl ctor
    priv->cpp = new details::CppImpl {*win};
    priv->cpp->init();

}

static void
ring_main_window_dispose(GObject *object)
{
    auto* self = RING_MAIN_WINDOW(object);
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

    delete priv->cpp;
    priv->cpp = nullptr;

    G_OBJECT_CLASS(ring_main_window_parent_class)->dispose(object);
}

static void
ring_main_window_finalize(GObject *object)
{
    auto* self = RING_MAIN_WINDOW(object);
    auto* priv = RING_MAIN_WINDOW_GET_PRIVATE(self);

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
}

GtkWidget *
ring_main_window_new(GtkApplication *app)
{
    gpointer win = g_object_new(RING_MAIN_WINDOW_TYPE, "application", app, NULL);
    return (GtkWidget *)win;
}
