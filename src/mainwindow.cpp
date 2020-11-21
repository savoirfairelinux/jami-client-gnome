/*
 *  Copyright (C) 2015-2020 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author : Aline Gondim Santos <aline.gondimsantos@savoirfairelinux.com>
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

#include "mainwindow.h"

// GTK+ related
#include <glib/gi18n.h>
// std
#include <algorithm>
#include <sstream>

// Qt
#include <QSize>

// LRC
#include <api/account.h>
#include <api/avmodel.h>
#include <api/pluginmodel.h>
#include <api/behaviorcontroller.h>
#include <api/contact.h>
#include <api/contactmodel.h>
#include <api/conversation.h>
#include <api/conversationmodel.h>
#include <api/datatransfermodel.h>
#include <api/lrc.h>
#include <api/newaccountmodel.h>
#include <api/newcallmodel.h>
#include <api/profile.h>

// Jami Client
#include "config.h"
#include "newaccountsettingsview.h"
#include "accountmigrationview.h"
#include "accountcreationwizard.h"
#include "chatview.h"
#include "conversationsview.h"
#include "currentcallview.h"
#include "dialogs.h"
#include "generalsettingsview.h"
#include "pluginsettingsview.h"
#include "incomingcallview.h"
#include "mediasettingsview.h"
#include "models/gtkqtreemodel.h"
#include "welcomeview.h"
#include "utils/drawing.h"
#include "utils/files.h"
#include "notifier.h"
#include "accountinfopointer.h"
#include "native/pixbufmanipulator.h"
#include "notifier.h"

#if USE_LIBNM
#include <NetworkManager.h>
#endif

//==============================================================================

namespace { namespace details
{
class CppImpl;
}}

struct _MainWindow
{
    GtkApplicationWindow parent;
};

struct _MainWindowClass
{
    GtkApplicationWindowClass parent_class;
};

struct MainWindowPrivate
{
    GtkWidget *menu;
    GtkWidget *image_ring;
    GtkWidget *settings;
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
    GtkWidget *media_settings_view;
    GtkWidget *plugin_settings_view;
    GtkWidget *new_account_settings_view;
    GtkWidget *general_settings_view;
    GtkWidget *last_settings_view;
    GtkWidget *radiobutton_new_account_settings;
    GtkWidget *radiobutton_general_settings;
    GtkWidget *radiobutton_plugin_settings;
    GtkWidget *radiobutton_media_settings;
    GtkWidget *account_creation_wizard;
    GtkWidget *account_migration_view;
    GtkWidget *combobox_account_selector;
    GtkWidget *treeview_contact_requests;
    GtkWidget *scrolled_window_contact_requests;
    GtkWidget *webkit_chat_container; ///< The webkit_chat_container is created once, then reused for all chat views
    GtkWidget *image_contact_requests_list;
    GtkWidget *image_conversations_list;

    GtkWidget *notifier;

    GSettings *window_settings;
    bool useDarkTheme {false};

    details::CppImpl* cpp; ///< Non-UI and C++ only code

    gulong update_download_folder;
    gulong notif_chat_view;
    gulong notif_accept_pending;
    gulong notif_refuse_pending;
    gulong notif_accept_call;
    gulong notif_decline_call;
    gboolean set_top_account_flag = true;
    gboolean is_fullscreen_main_win = false;
    gboolean key_pressed = false;

    GCancellable *cancellable;

    GtkWidget* migratingDialog_;
#if USE_LIBNM
    /* NetworkManager */
    NMClient *nm_client;
#endif
};

G_DEFINE_TYPE_WITH_PRIVATE(MainWindow, main_window, GTK_TYPE_APPLICATION_WINDOW);

#define MAIN_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MAIN_WINDOW_TYPE, MainWindowPrivate))

//==============================================================================

namespace { namespace details
{

static constexpr const char* CALL_VIEW_NAME                    = "calls";
static constexpr const char* ACCOUNT_CREATION_WIZARD_VIEW_NAME = "account-creation-wizard";
static constexpr const char* ACCOUNT_MIGRATION_VIEW_NAME       = "account-migration-view";
static constexpr const char* GENERAL_SETTINGS_VIEW_NAME        = "general";
static constexpr const char* MEDIA_SETTINGS_VIEW_NAME          = "media";
static constexpr const char* NEW_ACCOUNT_SETTINGS_VIEW_NAME    = "account";
static constexpr const char* PLUGIN_SETTINGS_VIEW_NAME          = "plugin";

inline namespace helpers
{

/**
 * set the column value by printing the alias and the state of an account in combobox_account_selector.
 */
static void
print_account_and_state(GtkCellLayout*,
                        GtkCellRenderer* cell,
                        GtkTreeModel* model,
                        GtkTreeIter* iter,
                        gpointer*)
{
    gchar *id;
    gchar *alias;
    gchar *registeredName;
    gchar *uri;
    gchar *text;

    gtk_tree_model_get (model, iter,
                        0 /* col# */, &id /* data */,
                        3 /* col# */, &uri /* data */,
                        4 /* col# */, &alias /* data */,
                        5 /* col# */, &registeredName /* data */,
                        -1);

    if (g_strcmp0("", id) == 0) {
        text = g_markup_printf_escaped(
            "<span font=\"10\">%s</span>",
            _("Add account…")
        );
    } else if (g_strcmp0("", registeredName) == 0) {
        if (g_strcmp0(uri, alias) == 0) {
            text = g_markup_printf_escaped(
                "<span font=\"10\">%s</span>",
                alias
            );
        } else {
            text = g_markup_printf_escaped(
                "<span font=\"10\">%s</span>\n<span font=\"7\">%s</span>",
                alias, uri
            );
        }
    } else {
        if (g_strcmp0(alias, registeredName) == 0) {
            text = g_markup_printf_escaped(
                "<span font=\"10\">%s</span>",
                alias
            );
        } else {
            text = g_markup_printf_escaped(
                "<span font=\"10\">%s</span>\n<span font=\"7\">%s</span>",
                alias, registeredName
            );
        }
    }

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_object_set(G_OBJECT(cell), "height", 17, NULL);
    g_object_set(G_OBJECT(cell), "ypad", 0, NULL);

    g_free(id);
    g_free(alias);
    g_free(registeredName);
    g_free(uri);
    g_free(text);
}

static void
render_account_avatar(GtkCellLayout*,
                      GtkCellRenderer *cell,
                      GtkTreeModel *model,
                      GtkTreeIter *iter,
                      gpointer)
{
    gchar *id;
    gchar* avatar;
    gchar* status;

    gtk_tree_model_get (model, iter,
                        0 /* col# */, &id /* data */,
                        1 /* col# */, &status /* data */,
                        2 /* col# */, &avatar /* data */,
                        -1);

    if (g_strcmp0("", id) == 0) {
        g_free(status);
        g_free(avatar);
        g_free(id);

        GdkPixbuf* icon = gdk_pixbuf_new_from_resource("/net/jami/JamiGnome/add-device", nullptr);
        g_object_set(G_OBJECT(cell), "width", 32, nullptr);
        g_object_set(G_OBJECT(cell), "height", 32, nullptr);
        g_object_set(G_OBJECT(cell), "pixbuf", icon, nullptr);
        return;
    }

    IconStatus iconStatus = IconStatus::INVALID;
    std::string statusStr = status? status : "";
    if (statusStr == "DISCONNECTED") {
        iconStatus = IconStatus::DISCONNECTED;
    } else if (statusStr == "TRYING") {
        iconStatus = IconStatus::TRYING;
    } else if (statusStr == "CONNECTED") {
        iconStatus = IconStatus::CONNECTED;
    }
    auto default_avatar = Interfaces::PixbufManipulator().generateAvatar("", "");
    auto default_scaled = Interfaces::PixbufManipulator().scaleAndFrame(default_avatar.get(), QSize(32, 32), true, iconStatus);
    auto photo = default_scaled;

    std::string photostr = avatar;
    if (!photostr.empty()) {
        QByteArray byteArray(photostr.c_str(), photostr.length());
        QVariant avatar = Interfaces::PixbufManipulator().personPhoto(byteArray);
        auto pixbuf_photo = Interfaces::PixbufManipulator().scaleAndFrame(avatar.value<std::shared_ptr<GdkPixbuf>>().get(), QSize(32, 32), true, iconStatus);
        if (avatar.isValid()) {
            photo = pixbuf_photo;
        }
    }

    g_object_set(G_OBJECT(cell), "width", 32, nullptr);
    g_object_set(G_OBJECT(cell), "height", 32, nullptr);
    g_object_set(G_OBJECT(cell), "pixbuf", photo.get(), nullptr);

    g_free(status);
    g_free(avatar);
    g_free(id);
}

inline static void
foreachLrcAccount(const lrc::api::Lrc& lrc,
                  const std::function<void(const lrc::api::account::Info&)>& func)
{
    auto& account_model = lrc.getAccountModel();
    for (const auto& account_id : account_model.getAccountList()) {
        const auto& account_info = account_model.getAccountInfo(account_id);
            func(account_info);
    }
}

} // namespace helpers

class CppImpl
{
public:
    explicit CppImpl(MainWindow& widget);
    ~CppImpl();

    void init();
    void updateLrc(const std::string& accountId, const std::string& accountIdToFlagFreeable = "");
    void changeView(GType type, lrc::api::conversation::Info conversation = {});
    void enterFullScreen();
    void leaveFullScreen();
    void toggleFullScreen();
    void resetToWelcome();
    void refreshPendingContactRequestTab();
    void onAccountSelectionChange(const std::string& id);
    void enterAccountCreationWizard(bool showControls = false);
    void leaveAccountCreationWizard();
    void enterSettingsView();
    void leaveSettingsView();

    std::string getCurrentUid();
    void forCurrentConversation(const std::function<void(const lrc::api::conversation::Info&)>& func);
    bool showOkCancelDialog(const std::string& title, const std::string& text);

    lrc::api::conversation::Info getCurrentConversation(GtkWidget* frame_call);

    void showAccountSelectorWidget(bool show = true);
    std::size_t refreshAccountSelectorWidget(int selection_row = -1, const std::string& selected = "");

    WebKitChatContainer* webkitChatContainer(bool redraw_webview = false);

    MainWindow* self = nullptr; // The GTK widget itself
    MainWindowPrivate* widgets = nullptr;

    std::unique_ptr<lrc::api::Lrc> lrc_;
    AccountInfoPointer accountInfo_ = nullptr;
    AccountInfoPointer accountInfoForMigration_ = nullptr;
    std::unique_ptr<lrc::api::conversation::Info> chatViewConversation_;
    lrc::api::profile::Type currentTypeFilter_;
    bool show_settings = false;
    bool is_fullscreen = false;
    bool has_cleared_all_history = false;
    guint inhibitionCookie = 0;

    int smartviewPageNum = 0;
    int contactRequestsPageNum = 0;

    QMetaObject::Connection showChatViewConnection_;
    QMetaObject::Connection showLeaveMessageViewConnection_;
    QMetaObject::Connection showCallViewConnection_;
    QMetaObject::Connection newTrustRequestNotification_;
    QMetaObject::Connection closeTrustRequestNotification_;
    QMetaObject::Connection slotNewInteraction_;
    QMetaObject::Connection slotReadInteraction_;
    QMetaObject::Connection newAccountConnection_;
    QMetaObject::Connection rmAccountConnection_;
    QMetaObject::Connection invalidAccountConnection_;
    QMetaObject::Connection historyClearedConnection_;
    QMetaObject::Connection modelSortedConnection_;
    QMetaObject::Connection callChangedConnection_;
    QMetaObject::Connection callStartedConnection_;
    QMetaObject::Connection callEndedConnection_;
    QMetaObject::Connection newIncomingCallConnection_;
    QMetaObject::Connection filterChangedConnection_;
    QMetaObject::Connection newConversationConnection_;
    QMetaObject::Connection conversationRemovedConnection_;
    QMetaObject::Connection accountStatusChangedConnection_;
    QMetaObject::Connection profileUpdatedConnection_;

    std::string eventUid_;
    std::string eventBody_;

    bool isCreatingAccount {false};
    QHash<QString, QMetaObject::Connection> pendingConferences_;
private:
    CppImpl() = delete;
    CppImpl(const CppImpl&) = delete;
    CppImpl& operator=(const CppImpl&) = delete;

    GtkWidget* displayWelcomeView(lrc::api::conversation::Info);
    GtkWidget* displayIncomingView(lrc::api::conversation::Info, bool redraw_webview);
    GtkWidget* displayCurrentCallView(lrc::api::conversation::Info, bool redraw_webview);
    GtkWidget* displayChatView(lrc::api::conversation::Info, bool redraw_webview);

    // Callbacks used as LRC Qt slot
    void slotAccountAddedFromLrc(const std::string& id);
    void slotAccountRemovedFromLrc(const std::string& id);
    void slotInvalidAccountFromLrc(const std::string& id);
    void slotAccountNeedsMigration(const std::string& id);
    void slotAccountStatusChanged(const std::string& id);
    void slotConversationCleared(const std::string& uid);
    void slotModelSorted();
    void slotNewIncomingCall(const std::string& accountId, lrc::api::conversation::Info origin);
    void slotCallStatusChanged(const std::string& accountId, const std::string& callId);
    void slotCallStarted(const std::string& callId);
    void slotCallEnded(const std::string& callId);
    void slotFilterChanged();
    void slotNewConversation(const std::string& uid);
    void slotConversationRemoved(const std::string& uid);
    void slotShowChatView(const std::string& id, lrc::api::conversation::Info origin);
    void slotShowLeaveMessageView(lrc::api::conversation::Info conv);
    void slotShowCallView(const std::string& id, lrc::api::conversation::Info origin);
    void slotNewTrustRequest(const std::string& id, const std::string& contactUri);
    void slotCloseTrustRequest(const std::string& id, const std::string& contactUri);
    void slotNewInteraction(const std::string& accountId, const std::string& conversation,
                                uint64_t interactionId, const lrc::api::interaction::Info& interaction);
    void slotCloseInteraction(const std::string& accountId, const std::string& conversation, uint64_t interactionId);
    void slotProfileUpdated(const std::string& id);
};

inline namespace gtk_callbacks
{

static void
render_rendezvous_mode(GtkCellLayout*,
                      GtkCellRenderer *cell,
                      GtkTreeModel *model,
                      GtkTreeIter *iter,
                      MainWindowPrivate* priv)
{
    g_return_if_fail(priv->cpp->accountInfo_);

    gchar *id;
    gchar* avatar;
    gchar* status;

    gtk_tree_model_get (model, iter,
                        0 /* col# */, &id /* data */,
                        -1);

    if (g_strcmp0("", id) != 0) {
        try {
            auto conf = priv->cpp->accountInfo_->accountModel->getAccountConfig(id);
            if (conf.isRendezVous) {
                GdkPixbuf* icon = gdk_pixbuf_new_from_resource("/net/jami/JamiGnome/groups", nullptr);
                g_object_set(G_OBJECT(cell), "width", 32, nullptr);
                g_object_set(G_OBJECT(cell), "height", 32, nullptr);
                g_object_set(G_OBJECT(cell), "pixbuf", icon, nullptr);
            } else {
                g_object_set(G_OBJECT(cell), "pixbuf", nullptr, nullptr);
            }
        } catch (...) {}
    } else {
        g_object_set(G_OBJECT(cell), "pixbuf", nullptr, nullptr);
    }
    g_free(id);
}

static void
on_video_double_clicked(MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    priv->cpp->toggleFullScreen();
}

static void
on_hide_view_clicked(MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    priv->cpp->resetToWelcome();
}

static gboolean
place_call_event(MainWindow* self)
{
    g_return_val_if_fail(IS_MAIN_WINDOW(self), G_SOURCE_REMOVE);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    if (!priv->cpp->eventUid_.empty())
        priv->cpp->accountInfo_->conversationModel->placeCall(priv->cpp->eventUid_.c_str());

    return G_SOURCE_REMOVE;
}

static void
on_place_call_clicked(G_GNUC_UNUSED GtkWidget*, gchar *uid, MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self) && uid);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    priv->cpp->eventUid_ = uid;
    g_idle_add((GSourceFunc)place_call_event, self);
}

static gboolean
add_conversation_event(MainWindow* self)
{
    g_return_val_if_fail(IS_MAIN_WINDOW(self), G_SOURCE_REMOVE);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    if (!priv->cpp->eventUid_.empty())
        priv->cpp->accountInfo_->conversationModel->makePermanent(priv->cpp->eventUid_.c_str());

    return G_SOURCE_REMOVE;
}

static void
on_add_conversation_clicked(G_GNUC_UNUSED GtkWidget*, gchar *uid, MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self) && uid);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    priv->cpp->eventUid_ = uid;
    g_idle_add((GSourceFunc)add_conversation_event, self);
}

static gboolean
send_text_event(MainWindow* self)
{
    g_return_val_if_fail(IS_MAIN_WINDOW(self), G_SOURCE_REMOVE);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    if (!priv->cpp->eventUid_.empty())
        priv->cpp->accountInfo_->conversationModel->sendMessage(priv->cpp->eventUid_.c_str(),
                                                                priv->cpp->eventBody_.c_str());

    return G_SOURCE_REMOVE;
}

gboolean
on_redraw(GtkWidget*, cairo_t*, MainWindow* self)
{
    g_return_val_if_fail(IS_MAIN_WINDOW(self), G_SOURCE_REMOVE);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    auto color = get_ambient_color(GTK_WIDGET(self));
    bool current_theme = (color.red + color.green + color.blue) / 3 < .5;
    conversations_view_set_theme(CONVERSATIONS_VIEW(priv->treeview_conversations), current_theme);
    if (priv->useDarkTheme != current_theme) {
        welcome_set_theme(WELCOME_VIEW(priv->welcome_view), current_theme);
        welcome_update_view(WELCOME_VIEW(priv->welcome_view));

        gtk_image_set_from_resource(GTK_IMAGE(priv->image_contact_requests_list), current_theme?
            "/net/jami/JamiGnome/contact_requests_list_white" : "/net/jami/JamiGnome/contact_requests_list");
        gtk_image_set_from_resource(GTK_IMAGE(priv->image_conversations_list), current_theme?
            "/net/jami/JamiGnome/conversations_list_white" : "/net/jami/JamiGnome/conversations_list");

        priv->useDarkTheme = current_theme;

        std::stringstream background;
        background << "#" << std::hex << (int)(color.red * 256)
                          << (int)(color.green * 256)
                          << (int)(color.blue * 256);

        auto provider = gtk_css_provider_new();
        std::string background_search_entry = "background: " + background.str() + ";";
        std::string css_style = ".search-entry-style { border: 0; border-radius: 0; } \
        .spinner-style { border: 0; background: white; } \
        .new-conversation-style { border: 0; " + background_search_entry + " transition: all 0.3s ease; border-radius: 0; } \
        .new-conversation-style:hover {  background: " + (priv->useDarkTheme ? "#003b4e" : "#bae5f0") + "; }";
        gtk_css_provider_load_from_data(provider,
            css_style.c_str(),
            -1, nullptr
        );
        gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                                  GTK_STYLE_PROVIDER(provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    return false;
}

static void
on_send_text_clicked(G_GNUC_UNUSED GtkWidget*, gchar* uid, gchar* body, MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self) && uid);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    priv->cpp->eventUid_ = uid;
    priv->cpp->eventBody_ = body;
    g_idle_add((GSourceFunc)send_text_event, self);
}

static gboolean
place_audio_call_event(MainWindow* self)
{
    g_return_val_if_fail(IS_MAIN_WINDOW(self), G_SOURCE_REMOVE);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    if (!priv->cpp->eventUid_.empty())
        priv->cpp->accountInfo_->conversationModel->placeAudioOnlyCall(priv->cpp->eventUid_.c_str());

    return G_SOURCE_REMOVE;
}

static void
on_place_audio_call_clicked(G_GNUC_UNUSED GtkWidget*, gchar *uid, MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self) && uid);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    priv->cpp->eventUid_ = uid;
    g_idle_add((GSourceFunc)place_audio_call_event, self);
}

static void
on_account_creation_completed(MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    priv->cpp->isCreatingAccount = false;
    priv->cpp->leaveAccountCreationWizard();
}

static void
on_account_creation_unlock(MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    priv->cpp->isCreatingAccount = false;
}

static void
on_account_creation_lock(MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    priv->cpp->isCreatingAccount = true;
}

static void
on_account_changed(MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    auto changeTopAccount = priv->set_top_account_flag;
    if (!priv->set_top_account_flag) {
        priv->set_top_account_flag = true;
    }

    auto accountComboBox = GTK_COMBO_BOX(priv->combobox_account_selector);
    auto model = gtk_combo_box_get_model(accountComboBox);

    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(accountComboBox, &iter)) {
        gchar* accountId;
        gtk_tree_model_get(model, &iter, 0 /* col# */, &accountId /* data */, -1);
        if (g_strcmp0("", accountId) == 0) {
            priv->cpp->enterAccountCreationWizard(true);
        } else {
            if (!priv->cpp->isCreatingAccount) priv->cpp->leaveAccountCreationWizard();
            if (priv->cpp->accountInfo_ && changeTopAccount) {
                priv->cpp->accountInfo_->accountModel->setTopAccount(accountId);
            }
            priv->cpp->onAccountSelectionChange(accountId);
            gtk_notebook_set_show_tabs(GTK_NOTEBOOK(priv->notebook_contacts),
                priv->cpp->accountInfo_->contactModel->hasPendingRequests());
        }
        g_free(accountId);
    }
}

static void
on_settings_clicked(MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    if (!priv->cpp->show_settings)
        priv->cpp->enterSettingsView();
    else
        priv->cpp->leaveSettingsView();
}

static void
on_show_media_settings(GtkToggleButton* navbutton, MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    if (gtk_toggle_button_get_active(navbutton)) {
        media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(priv->media_settings_view), TRUE);
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), MEDIA_SETTINGS_VIEW_NAME);
        priv->last_settings_view = priv->media_settings_view;
    } else {
        media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(priv->media_settings_view), FALSE);
    }
}


static void
on_show_new_account_settings(GtkToggleButton* navbutton, MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    if (gtk_toggle_button_get_active(navbutton)) {
        new_account_settings_view_show(NEW_ACCOUNT_SETTINGS_VIEW(priv->new_account_settings_view), TRUE);
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), NEW_ACCOUNT_SETTINGS_VIEW_NAME);
        priv->last_settings_view = priv->new_account_settings_view;
    } else {
        new_account_settings_view_show(NEW_ACCOUNT_SETTINGS_VIEW(priv->new_account_settings_view), FALSE);
    }
}

static void
on_show_general_settings(GtkToggleButton* navbutton, MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    if (gtk_toggle_button_get_active(navbutton)) {
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), GENERAL_SETTINGS_VIEW_NAME);
        priv->last_settings_view = priv->general_settings_view;
    }
}

static void
on_show_plugin_settings(GtkToggleButton* navbutton, MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    if (gtk_toggle_button_get_active(navbutton)) {
        plugin_settings_view_show(PLUGIN_SETTINGS_VIEW(priv->plugin_settings_view), TRUE);
        gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), PLUGIN_SETTINGS_VIEW_NAME);
        priv->last_settings_view = priv->plugin_settings_view;
    } else {
        plugin_settings_view_show(PLUGIN_SETTINGS_VIEW(priv->plugin_settings_view), FALSE);
    }
}

static void
on_search_entry_text_changed(GtkSearchEntry* search_entry, MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    // Filter model
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(search_entry));
    priv->cpp->accountInfo_->conversationModel->setFilter(text);
}

static void
on_search_entry_activated(MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    // Select the first conversation of the list
    auto& conversationModel = priv->cpp->accountInfo_->conversationModel;
    auto conversations = conversationModel->getAllSearchResults();

    const gchar *text = gtk_entry_get_text(GTK_ENTRY(priv->search_entry));

    if (!conversations.empty() && text && !std::string(text).empty())
        conversationModel->selectConversation(conversations[0].uid);
}

static gboolean
on_search_entry_key_released(G_GNUC_UNUSED GtkEntry* search_entry, GdkEventKey* key, MainWindow* self)
{
    g_return_val_if_fail(IS_MAIN_WINDOW(self), GDK_EVENT_PROPAGATE);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    // if esc key pressed, clear the regex (keep the text, the user might not want to actually delete it)
    if (key->keyval == GDK_KEY_Escape) {
        priv->cpp->accountInfo_->conversationModel->setFilter("");
        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean
on_current_call_clicked(GtkWidget *widget, G_GNUC_UNUSED GdkEventButton *event)
{
    // once mouse is clicked, grab the focus
    gtk_widget_set_can_focus (widget, true);
    gtk_widget_grab_focus(widget);
    return GDK_EVENT_PROPAGATE;
}

static gboolean
on_dtmf_pressed(MainWindow* self, GdkEventKey* event, gpointer user_data)
{
    g_return_val_if_fail(IS_MAIN_WINDOW(self), GDK_EVENT_PROPAGATE);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    if(priv->key_pressed && !(event->state & GDK_SHIFT_MASK)){
        return GDK_EVENT_PROPAGATE;
    }

    (void)user_data;

    g_return_val_if_fail(event->type == GDK_KEY_PRESS, GDK_EVENT_PROPAGATE);
    g_return_val_if_fail(priv, GDK_EVENT_PROPAGATE);

    /* we want to react to digit key presses, as long as a GtkEntry is not the
     * input focus
     */
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(self));
    if (GTK_IS_ENTRY(focus))
        return GDK_EVENT_PROPAGATE;

    if (priv->cpp->accountInfo_ &&
        priv->cpp->accountInfo_->profileInfo.type != lrc::api::profile::Type::SIP)
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

    if (current_item.callId.isEmpty())
       return GDK_EVENT_PROPAGATE;

    // pass the character that was entered to be played by the daemon;
    // the daemon will filter out invalid DTMF characters
    guint32 unicode_val = gdk_keyval_to_unicode(event->keyval);
    QString val = QString::fromUcs4(&unicode_val, 1);
    g_debug("attempting to play DTMF tone during ongoing call: %s", qUtf8Printable(val));
    priv->cpp->accountInfo_->callModel->playDTMF(current_item.callId, val);

    // set keyPressed to True
    priv->key_pressed = true;
    // always propagate the key, so we don't steal accelerators/shortcuts
    return GDK_EVENT_PROPAGATE;
}

static gboolean
on_dtmf_released(MainWindow* self, GdkEventKey* event, gpointer user_data)
{
    g_return_val_if_fail(IS_MAIN_WINDOW(self), GDK_EVENT_PROPAGATE);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    priv->key_pressed = false;
    return GDK_EVENT_PROPAGATE;
}

static void
on_tab_changed(GtkNotebook* notebook, GtkWidget* page, guint page_num, MainWindow* self)
{
    (void)notebook;
    (void)page;

    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    auto newType = page_num == 0 ? priv->cpp->accountInfo_->profileInfo.type : lrc::api::profile::Type::PENDING;
    if (priv->cpp->currentTypeFilter_ != newType) {
        priv->cpp->currentTypeFilter_ = newType;
        priv->cpp->accountInfo_->conversationModel->setFilter(priv->cpp->currentTypeFilter_);
    }
}

static gboolean
on_window_state_event(GtkWidget *widget, GdkEventWindowState *event)
{
    g_return_val_if_fail(IS_MAIN_WINDOW(widget), GDK_EVENT_PROPAGATE);
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(widget));

    g_settings_set_boolean(priv->window_settings, "window-maximized", ((event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0));

    return GDK_EVENT_PROPAGATE;
}

static void
on_window_size_changed(GtkWidget *self, G_GNUC_UNUSED GdkRectangle*, G_GNUC_UNUSED gpointer)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    int new_width, new_height;
    gtk_window_get_size(GTK_WINDOW(self), &new_width, &new_height);
    g_settings_set_int(priv->window_settings, "window-width", new_width);
    g_settings_set_int(priv->window_settings, "window-height", new_height);
}

static void
on_search_entry_places_call_changed(GSettings* settings, const gchar* key, MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

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
on_handle_account_migrations(MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    /* If there is an existing migration view, remove it */
    if (priv->account_migration_view) {
        gtk_widget_destroy(priv->account_migration_view);
        priv->account_migration_view = nullptr;
    }

    auto accounts = priv->cpp->lrc_->getAccountModel().getAccountList();
    for (const auto& accountId : accounts) {
        priv->cpp->accountInfoForMigration_ = &priv->cpp->lrc_->getAccountModel().getAccountInfo(accountId);
        if (priv->cpp->accountInfoForMigration_->status == lrc::api::account::Status::ERROR_NEED_MIGRATION) {
            priv->account_migration_view = account_migration_view_new(priv->cpp->accountInfoForMigration_);
            g_signal_connect_swapped(priv->account_migration_view, "account-migration-completed",
                                     G_CALLBACK(on_handle_account_migrations), self);
            g_signal_connect_swapped(priv->account_migration_view, "account-migration-failed",
                                     G_CALLBACK(on_handle_account_migrations), self);

            gtk_widget_hide(priv->settings);
            priv->cpp->showAccountSelectorWidget(false);
            gtk_widget_show(priv->account_migration_view);
            gtk_stack_add_named(
                GTK_STACK(priv->stack_main_view),
                priv->account_migration_view,
                ACCOUNT_MIGRATION_VIEW_NAME);
            gtk_stack_set_visible_child_name(
                GTK_STACK(priv->stack_main_view),
                ACCOUNT_MIGRATION_VIEW_NAME);
            return;
        }
    }

    priv->cpp->accountInfoForMigration_ = nullptr;
    on_account_changed(self);
    gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_main_view), CALL_VIEW_NAME);
    gtk_widget_show(priv->settings);
}

enum class Action {
    SELECT,
    ACCEPT,
    REFUSE
};

static void
action_notification(gchar* title, MainWindow* self, Action action)
{
    g_return_if_fail(IS_MAIN_WINDOW(self) && title);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    if (!priv->cpp->accountInfo_) {
        g_warning("Notification clicked but accountInfo_ currently empty");
        return;
    }

    std::string titleStr = title;
    auto firstMarker = titleStr.find(":");
    if (firstMarker == std::string::npos) return;
    auto secondMarker = titleStr.find(":", firstMarker + 1);
    if (secondMarker == std::string::npos) return;
    auto thirdMarker = titleStr.find(":", secondMarker + 1);

    auto id = titleStr.substr(0, firstMarker);
    auto type = titleStr.substr(firstMarker + 1, secondMarker - firstMarker - 1);
    auto information = titleStr.substr(secondMarker + 1, thirdMarker - secondMarker - 1);

    if (action == Action::SELECT) {
        // Select conversation
        if (priv->cpp->show_settings) {
            priv->cpp->leaveSettingsView();
        }

        if (priv->cpp->accountInfo_->id.toStdString() != id) {
            priv->cpp->updateLrc(id);
            priv->cpp->refreshAccountSelectorWidget(-1, id);
        }

        if (type == "interaction") {
            priv->cpp->accountInfo_->conversationModel->selectConversation(information.c_str());
            conversations_view_select_conversation(CONVERSATIONS_VIEW(priv->treeview_conversations), information);
        } else if (type == "request") {
            for (const auto& conversation : priv->cpp->accountInfo_->conversationModel->getFilteredConversations(lrc::api::profile::Type::PENDING)) {
                auto contactRequestsPageNum = gtk_notebook_page_num(GTK_NOTEBOOK(priv->notebook_contacts),
                                                               priv->scrolled_window_contact_requests);
                gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->notebook_contacts), contactRequestsPageNum);
                if (!conversation.participants.empty() && conversation.participants.front().toStdString() == information) {
                    priv->cpp->accountInfo_->conversationModel->selectConversation(conversation.uid);
                }
                conversations_view_select_conversation(CONVERSATIONS_VIEW(priv->treeview_conversations), conversation.uid.toStdString());
            }
        }
    } else {
        // accept or refuse notifiation
        try {
            auto& accountInfo = priv->cpp->lrc_->getAccountModel().getAccountInfo(id.c_str());
            for (const auto& conversation : accountInfo.conversationModel->getFilteredConversations(lrc::api::profile::Type::PENDING)) {
                if (!conversation.participants.empty() && conversation.participants.front().toStdString() == information) {
                    if (action == Action::ACCEPT) {
                        accountInfo.conversationModel->makePermanent(conversation.uid);
                    } else {
                        accountInfo.conversationModel->removeConversation(conversation.uid);
                    }
                }
            }
        } catch (const std::out_of_range& e) {
            g_warning("Can't get account %s: %s", id.c_str(), e.what());
        }
    }

}

static void
on_notification_chat_clicked(G_GNUC_UNUSED GtkWidget* notifier,
                             gchar *title, MainWindow* self)
{
    action_notification(title, self, Action::SELECT);
}

static void
on_notification_accept_pending(GtkWidget*, gchar *title, MainWindow* self)
{
    action_notification(title, self, Action::ACCEPT);
}

static void
on_notification_refuse_pending(GtkWidget*, gchar *title, MainWindow* self)
{
    action_notification(title, self, Action::REFUSE);
}

static void
on_notification_accept_call(GtkWidget*, gchar *title, MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self) && title);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    if (!priv->cpp->accountInfo_) {
        g_warning("Notification clicked but accountInfo_ currently empty");
        return;
    }

    std::string titleStr = title;
    auto firstMarker = titleStr.find(":");
    if (firstMarker == std::string::npos) return;
    auto secondMarker = titleStr.find(":", firstMarker + 1);
    if (secondMarker == std::string::npos) return;

    auto id = titleStr.substr(0, firstMarker);
    auto type = titleStr.substr(firstMarker + 1, secondMarker - firstMarker - 1);
    auto information = titleStr.substr(secondMarker + 1);

    if (priv->cpp->show_settings) {
        priv->cpp->leaveSettingsView();
    }

    if (priv->cpp->accountInfo_->id.toStdString() != id) {
        priv->cpp->updateLrc(id);
        priv->cpp->refreshAccountSelectorWidget(-1, id);
    }

    try {
        auto& accountInfo = priv->cpp->lrc_->getAccountModel().getAccountInfo(id.c_str());
        accountInfo.callModel->accept(information.c_str());
    } catch (const std::out_of_range& e) {
        g_warning("Can't get account %s: %s", id.c_str(), e.what());
    }
}

static void
on_notification_decline_call(GtkWidget*, gchar *title, MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self) && title);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    if (!priv->cpp->accountInfo_) {
        g_warning("Notification clicked but accountInfo_ currently empty");
        return;
    }

    std::string titleStr = title;
    auto firstMarker = titleStr.find(":");
    if (firstMarker == std::string::npos) return;
    auto secondMarker = titleStr.find(":", firstMarker + 1);
    if (secondMarker == std::string::npos) return;

    auto accountId = titleStr.substr(0, firstMarker);
    auto type = titleStr.substr(firstMarker + 1, secondMarker - firstMarker - 1);
    auto callId = titleStr.substr(secondMarker + 1);

    try {
        auto& accountInfo = priv->cpp->lrc_->getAccountModel().getAccountInfo(accountId.c_str());
        accountInfo.callModel->hangUp(callId.c_str());
        hide_notification(NOTIFIER(priv->notifier),
                          accountId + ":call:" + callId);
    } catch (const std::out_of_range& e) {
        g_warning("Can't get account %s: %s", accountId.c_str(), e.what());
    }
}

static void
on_incoming_call_view_decline_call(GtkWidget*, gchar* accountId, gchar* callId,
                                   MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self) && accountId && callId);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    if (!priv->cpp->accountInfo_) {
        g_warning("Notification clicked but accountInfo_ currently empty");
        return;
    }
    try {
        std::string aId(accountId);
        std::string cId(callId);
        hide_notification(NOTIFIER(priv->notifier), aId + ":call:" + cId);
    } catch (const std::out_of_range& e) {
        g_warning("Can't get account %s: %s", accountId, e.what());
    }
}

} // namespace gtk_callbacks

CppImpl::CppImpl(MainWindow& widget)
    : self {&widget}
    , widgets {MAIN_WINDOW_GET_PRIVATE(&widget)}
{
    lrc_ = std::make_unique<lrc::api::Lrc>([this](){
        widgets->migratingDialog_ = gtk_message_dialog_new(
            GTK_WINDOW(self), GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_INFO, GTK_BUTTONS_NONE, nullptr);
        gtk_window_set_title(GTK_WINDOW(widgets->migratingDialog_), _("Jami - Migration needed"));

        auto* content_area = gtk_dialog_get_content_area(GTK_DIALOG(widgets->migratingDialog_));
        GError *error = nullptr;

        GdkPixbuf* logo_jami = gdk_pixbuf_new_from_resource_at_scale("/net/jami/JamiGnome/jami-logo-blue",
                                                                    -1, 128, TRUE, &error);
        if (!logo_jami) {
            g_debug("Could not load logo: %s", error->message);
            g_clear_error(&error);
            return;
        }
        auto* image = gtk_image_new_from_pixbuf(logo_jami);
        auto* label = gtk_label_new(_("Migration in progress... please do not close this window."));
        gtk_container_add(GTK_CONTAINER(content_area), image);
        gtk_container_add(GTK_CONTAINER(content_area), label);
        gtk_widget_set_margin_left(content_area, 32);
        gtk_widget_set_margin_right(content_area, 32);

        gtk_widget_show_all(widgets->migratingDialog_);

        gtk_dialog_run(GTK_DIALOG(widgets->migratingDialog_));
    },
    [this](){
        gtk_widget_destroy(widgets->migratingDialog_);
    });
}

static void
on_clear_all_history_clicked(MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    g_return_if_fail(priv && priv->cpp);

    for (const auto &account_id : priv->cpp->lrc_->getAccountModel().getAccountList()) {
        try {
            auto &accountInfo = priv->cpp->lrc_->getAccountModel().getAccountInfo(account_id);
            accountInfo.conversationModel->clearAllHistory();
        } catch (const std::out_of_range &e) {
            g_warning("Can't get account %s: %s", qUtf8Printable(account_id), e.what());
        }
    }

    priv->cpp->has_cleared_all_history = true;
}

static void
update_data_transfer(lrc::api::DataTransferModel& model, GSettings* settings)
{
    char* download_directory_value;
    auto* download_directory_variant = g_settings_get_value(settings, "download-folder");
    g_variant_get(download_directory_variant, "&s", &download_directory_value);
    std::string download_dir = download_directory_value;
    if (!download_dir.empty() && download_dir.back() != '/')
        download_dir += "/";
    model.downloadDirectory = QString::fromStdString(download_dir);
}

static void
update_download_folder(MainWindow* self)
{
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    g_return_if_fail(priv);

    update_data_transfer(priv->cpp->lrc_->getDataTransferModel(), priv->window_settings);
}

#if USE_LIBNM

static void
log_connection_info(NMActiveConnection *connection)
{
    if (connection) {
        g_debug("primary network connection: %s, default: %s",
                nm_active_connection_get_uuid(connection),
                nm_active_connection_get_default(connection) ? "yes" : "no");
    } else {
        g_warning("no primary network connection detected, check network settings");
    }
}

static void
primary_connection_changed(NMClient *nm,  GParamSpec*, MainWindow* self)
{
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    auto connection = nm_client_get_primary_connection(nm);
    log_connection_info(connection);
    priv->cpp->lrc_->connectivityChanged();
}

static void
nm_client_cb(G_GNUC_UNUSED GObject *source_object, GAsyncResult *result,  MainWindow* self)
{
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));

    GError* error = nullptr;
    if (auto nm_client = nm_client_new_finish(result, &error)) {
        priv->nm_client = nm_client;
        g_debug("NetworkManager client initialized, version: %s\ndaemon running: %s\nnnetworking enabled: %s",
                nm_client_get_version(nm_client),
                nm_client_get_nm_running(nm_client) ? "yes" : "no",
                nm_client_networking_get_enabled(nm_client) ? "yes" : "no");

        auto connection = nm_client_get_primary_connection(nm_client);
        log_connection_info(connection);
        g_signal_connect(nm_client, "notify::active-connections", G_CALLBACK(primary_connection_changed), self);

    } else {
        g_warning("error initializing NetworkManager client: %s", error->message);
        g_clear_error(&error);
    }
}

#endif /* USE_LIBNM */

void
CppImpl::init()
{
    widgets->cancellable = g_cancellable_new();
#if USE_LIBNM
     // monitor the network using libnm to notify the daemon about connectivity changes
     nm_client_new_async(widgets->cancellable, (GAsyncReadyCallback)nm_client_cb, self);
#endif

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
        on_handle_account_migrations(self);
        for (const auto& id : accountIds) {
            const auto& accountInfo = lrc_->getAccountModel().getAccountInfo(id);
            if (accountInfo.enabled) {
                activeAccountId = id;
                break;
            }
        }
    }

    if (!activeAccountId.isEmpty()) {
        updateLrc(activeAccountId.toStdString());
    } else if(!accountIds.isEmpty()) {
        // If all accounts are disabled, show the first account
        updateLrc(accountIds.front().toStdString());
    } else {
        // No account: create empty widgets
        widgets->treeview_conversations = conversations_view_new(accountInfo_);
        gtk_container_add(GTK_CONTAINER(widgets->scrolled_window_smartview), widgets->treeview_conversations);
        widgets->treeview_contact_requests = conversations_view_new(accountInfo_);
        gtk_container_add(GTK_CONTAINER(widgets->scrolled_window_contact_requests), widgets->treeview_contact_requests);
    }

    accountStatusChangedConnection_ = QObject::connect(&lrc_->getAccountModel(),
                                                       &lrc::api::NewAccountModel::accountStatusChanged,
                                                       [this](const QString& id) { slotAccountStatusChanged(id.toStdString()); });
    profileUpdatedConnection_ = QObject::connect(&lrc_->getAccountModel(),
                                                 &lrc::api::NewAccountModel::profileUpdated,
                                                 [this](const QString& id) { slotProfileUpdated(id.toStdString()); });
    newAccountConnection_ = QObject::connect(&lrc_->getAccountModel(),
                                             &lrc::api::NewAccountModel::accountAdded,
                                             [this](const QString& id) { slotAccountAddedFromLrc(id.toStdString()); });
    rmAccountConnection_ = QObject::connect(&lrc_->getAccountModel(),
                                            &lrc::api::NewAccountModel::accountRemoved,
                                            [this](const QString& id){ slotAccountRemovedFromLrc(id.toStdString()); });
    invalidAccountConnection_ = QObject::connect(&lrc_->getAccountModel(),
                                                 &lrc::api::NewAccountModel::invalidAccountDetected,
                                                 [this](const QString& id){ slotInvalidAccountFromLrc(id.toStdString()); });

    /* bind to window size settings */
    widgets->window_settings = g_settings_new_full(get_settings_schema(), nullptr, nullptr);
    auto width = g_settings_get_int(widgets->window_settings, "window-width");
    auto height = g_settings_get_int(widgets->window_settings, "window-height");
    gtk_window_set_default_size(GTK_WINDOW(self), width, height);
    g_signal_connect(self, "size-allocate", G_CALLBACK(on_window_size_changed), nullptr);
    g_signal_connect(self, "window-state-event", G_CALLBACK(on_window_state_event), nullptr);

    if (g_settings_get_boolean(widgets->window_settings, "window-maximized"))
        gtk_window_maximize(GTK_WINDOW(self));

    update_data_transfer(lrc_->getDataTransferModel(), widgets->window_settings);

    /* search-entry-places-call setting */
    on_search_entry_places_call_changed(widgets->window_settings, "search-entry-places-call", self);
    g_signal_connect(widgets->window_settings, "changed::search-entry-places-call",
                     G_CALLBACK(on_search_entry_places_call_changed), self);

    /* set window icon */
    GError *error = NULL;
    GdkPixbuf* icon = gdk_pixbuf_new_from_resource("/net/jami/JamiGnome/jami-symbol-blue", &error);
    if (icon == NULL) {
        g_debug("Could not load icon: %s", error->message);
        g_clear_error(&error);
    } else
        gtk_window_set_icon(GTK_WINDOW(self), icon);

    /* set menu icon */
    GdkPixbuf* image_ring = gdk_pixbuf_new_from_resource_at_scale("/net/jami/JamiGnome/jami-symbol-blue",
                                                                  -1, 16, TRUE, &error);
    if (image_ring == NULL) {
        g_debug("Could not load icon: %s", error->message);
        g_clear_error(&error);
    } else
        gtk_image_set_from_pixbuf(GTK_IMAGE(widgets->image_ring), image_ring);

    /* ring menu */
    GtkBuilder *builder = gtk_builder_new_from_resource("/net/jami/JamiGnome/gearsmenu.ui");
    GMenuModel *menu = G_MENU_MODEL(gtk_builder_get_object(builder, "menu"));
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(widgets->menu), menu);
    g_object_unref(builder);

    /* settings icon */
    gtk_image_set_from_icon_name(GTK_IMAGE(widgets->image_settings), "emblem-system-symbolic",
                                 GTK_ICON_SIZE_SMALL_TOOLBAR);

    /* connect settings button signal */
    g_signal_connect_swapped(widgets->settings, "clicked",
                             G_CALLBACK(on_settings_clicked), self);

    /* add the call view to the main stack */
    gtk_stack_add_named(GTK_STACK(widgets->stack_main_view), widgets->vbox_call_view,
                        CALL_VIEW_NAME);

    widgets->media_settings_view = media_settings_view_new(lrc_->getAVModel());
    gtk_stack_add_named(GTK_STACK(widgets->stack_main_view), widgets->media_settings_view,
                        MEDIA_SETTINGS_VIEW_NAME);

    widgets->plugin_settings_view = plugin_settings_view_new(lrc_->getPluginModel());
    gtk_stack_add_named(GTK_STACK(widgets->stack_main_view), widgets->plugin_settings_view,
                        PLUGIN_SETTINGS_VIEW_NAME);

    if (not accountIds.empty()) {
        widgets->new_account_settings_view = new_account_settings_view_new(accountInfo_, lrc_->getAVModel());
        gtk_stack_add_named(GTK_STACK(widgets->stack_main_view), widgets->new_account_settings_view,
                            NEW_ACCOUNT_SETTINGS_VIEW_NAME);
    }

    widgets->general_settings_view = general_settings_view_new(GTK_WIDGET(self), lrc_->getAVModel(), lrc_->getDataTransferModel());
    widgets->update_download_folder = g_signal_connect_swapped(
        widgets->general_settings_view,
        "update-download-folder",
        G_CALLBACK(update_download_folder),
        self
    );
    gtk_stack_add_named(GTK_STACK(widgets->stack_main_view), widgets->general_settings_view,
                        GENERAL_SETTINGS_VIEW_NAME);
    g_signal_connect_swapped(widgets->general_settings_view, "clear-all-history", G_CALLBACK(on_clear_all_history_clicked), self);


    /* make the account settings will be showed as the active one (or general if no accounts) */
    if (not accountIds.empty()) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets->radiobutton_new_account_settings), TRUE);
        widgets->last_settings_view = widgets->new_account_settings_view;
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets->radiobutton_general_settings), TRUE);
        widgets->last_settings_view = widgets->general_settings_view;
    }

    /* connect the settings button signals to switch settings views */
    g_signal_connect(widgets->radiobutton_media_settings, "toggled", G_CALLBACK(on_show_media_settings), self);
    g_signal_connect(widgets->radiobutton_general_settings, "toggled", G_CALLBACK(on_show_general_settings), self);
    g_signal_connect(widgets->radiobutton_new_account_settings, "toggled", G_CALLBACK(on_show_new_account_settings), self);
    g_signal_connect(widgets->radiobutton_plugin_settings, "toggled", G_CALLBACK(on_show_plugin_settings), self);
    g_signal_connect(widgets->notebook_contacts, "switch-page", G_CALLBACK(on_tab_changed), self);

    /* welcome/default view */
    widgets->welcome_view = welcome_view_new(accountInfo_);
    g_object_ref(widgets->welcome_view); // increase ref because don't want it to be destroyed when not displayed
    gtk_container_add(GTK_CONTAINER(widgets->frame_call), widgets->welcome_view);
    gtk_widget_show(widgets->welcome_view);

    g_signal_connect_swapped(widgets->search_entry, "activate", G_CALLBACK(on_search_entry_activated), self);
    g_signal_connect_swapped(widgets->button_new_conversation, "clicked", G_CALLBACK(on_search_entry_activated), self);
    g_signal_connect(widgets->search_entry, "search-changed", G_CALLBACK(on_search_entry_text_changed), self);
    g_signal_connect(widgets->search_entry, "key-release-event", G_CALLBACK(on_search_entry_key_released), self);
    g_signal_connect(widgets->search_entry, "draw", G_CALLBACK(on_redraw), self);


    /* react to digit key press/release events */
    g_signal_connect(self, "key-press-event", G_CALLBACK(on_dtmf_pressed), nullptr);
    g_signal_connect(self, "key-release-event", G_CALLBACK(on_dtmf_released), nullptr);

    /* set the search entry placeholder text */
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->search_entry),
                                   C_("Please try to make the translation 50 chars or less so that it fits into the layout",
                                      "Find or start a conversation"));

    /* init chat webkit container so that it starts loading before the first time we need it*/
    (void)webkitChatContainer();

    // set up account selector
    if (!activeAccountId.isEmpty()) {
        // if there is a non-disabled account, select the first such account
        refreshAccountSelectorWidget(-1, activeAccountId.toStdString());
    } else if(!accountIds.isEmpty()) {
        // all accounts are disabled, select the first account
        refreshAccountSelectorWidget(0);
    }

    auto* renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(widgets->combobox_account_selector), renderer, true);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(widgets->combobox_account_selector),
                                       renderer,
                                       (GtkCellLayoutDataFunc )render_account_avatar,
                                       widgets, nullptr);


    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(widgets->combobox_account_selector), renderer, true);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(widgets->combobox_account_selector),
                                       renderer,
                                       (GtkCellLayoutDataFunc )print_account_and_state,
                                       widgets, nullptr);

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(widgets->combobox_account_selector), renderer, true);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(widgets->combobox_account_selector),
                                       renderer,
                                       (GtkCellLayoutDataFunc )render_rendezvous_mode,
                                       widgets, nullptr);

    // we closing any view opened to avoid confusion (especially between SIP and  protocols).
    g_signal_connect_swapped(widgets->combobox_account_selector, "changed", G_CALLBACK(on_account_changed), self);

    // initialize the pending contact request icon.
    refreshPendingContactRequestTab();

    if (accountInfo_) {
        auto& conversationModel = accountInfo_->conversationModel;
        auto conversations = conversationModel->allFilteredConversations();
        for (const auto& conversation: conversations) {
            if (!conversation.callId.isEmpty()) {
                accountInfo_->conversationModel->selectConversation(conversation.uid);
            }
        }
    }
    // delete obsolete history
    if (not accountIds.empty()) {
        auto days = g_settings_get_int(widgets->window_settings, "history-limit");
        for (auto& accountId : accountIds) {
            auto& accountInfo = lrc_->getAccountModel().getAccountInfo(accountId);
            accountInfo.conversationModel->deleteObsoleteHistory(days);
        }
    }

    // No account? Show wizard
    auto accounts = lrc_->getAccountModel().getAccountList();
    if (accounts.empty()) {
        enterAccountCreationWizard();
    }

    widgets->notif_chat_view = g_signal_connect(widgets->notifier, "showChatView",
                                                     G_CALLBACK(on_notification_chat_clicked), self);
    widgets->notif_accept_pending = g_signal_connect(widgets->notifier, "acceptPending",
                                                     G_CALLBACK(on_notification_accept_pending), self);
    widgets->notif_refuse_pending = g_signal_connect(widgets->notifier, "refusePending",
                                                    G_CALLBACK(on_notification_refuse_pending), self);
    widgets->notif_accept_call = g_signal_connect(widgets->notifier, "acceptCall",
                                                  G_CALLBACK(on_notification_accept_call), self);
    widgets->notif_decline_call = g_signal_connect(widgets->notifier, "declineCall",
                                                   G_CALLBACK(on_notification_decline_call), self);
}

CppImpl::~CppImpl()
{
    QObject::disconnect(showLeaveMessageViewConnection_);
    QObject::disconnect(showChatViewConnection_);
    QObject::disconnect(historyClearedConnection_);
    QObject::disconnect(modelSortedConnection_);
    QObject::disconnect(callChangedConnection_);
    QObject::disconnect(callStartedConnection_);
    QObject::disconnect(callEndedConnection_);
    QObject::disconnect(newIncomingCallConnection_);
    QObject::disconnect(filterChangedConnection_);
    QObject::disconnect(newConversationConnection_);
    QObject::disconnect(conversationRemovedConnection_);
    QObject::disconnect(newAccountConnection_);
    QObject::disconnect(rmAccountConnection_);
    QObject::disconnect(invalidAccountConnection_);
    QObject::disconnect(showCallViewConnection_);
    QObject::disconnect(newTrustRequestNotification_);
    QObject::disconnect(closeTrustRequestNotification_);
    QObject::disconnect(slotNewInteraction_);
    QObject::disconnect(slotReadInteraction_);
    QObject::disconnect(accountStatusChangedConnection_);
    QObject::disconnect(profileUpdatedConnection_);

    g_clear_object(&widgets->welcome_view);
    g_clear_object(&widgets->webkit_chat_container);
}

void
CppImpl::changeView(GType type, lrc::api::conversation::Info conversation)
{
    leaveFullScreen();
    auto* old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
    bool redraw_webview = (g_type_is_a(INCOMING_CALL_VIEW_TYPE, type) && !IS_INCOMING_CALL_VIEW(old_view))
        || (g_type_is_a(CURRENT_CALL_VIEW_TYPE, type) && !IS_CURRENT_CALL_VIEW(old_view))
        || (g_type_is_a(CHAT_VIEW_TYPE, type) && !IS_CHAT_VIEW(old_view));
    gtk_container_remove(GTK_CONTAINER(widgets->frame_call),
                         old_view);

    GtkWidget* new_view;
    if (g_type_is_a(INCOMING_CALL_VIEW_TYPE, type)) {
        new_view = displayIncomingView(conversation, redraw_webview);
    } else if (g_type_is_a(CURRENT_CALL_VIEW_TYPE, type)) {
        new_view = displayCurrentCallView(conversation, redraw_webview);
    } else if (g_type_is_a(CHAT_VIEW_TYPE, type)) {
        new_view = displayChatView(conversation, redraw_webview);
    } else {
        new_view = displayWelcomeView(conversation);
    }

    gtk_container_add(GTK_CONTAINER(widgets->frame_call), new_view);
    gtk_widget_show(new_view);

    if (conversation.uid != "" && type != WELCOME_VIEW_TYPE)
        conversations_view_select_conversation(
            CONVERSATIONS_VIEW(widgets->treeview_conversations),
            conversation.uid.toStdString());

    // grab focus for handup button for current call view
    if (g_type_is_a(CURRENT_CALL_VIEW_TYPE, type)){
        current_call_view_handup_focus(new_view);
    }
}

GtkWidget*
CppImpl::displayWelcomeView(lrc::api::conversation::Info conversation)
{
    (void) conversation;

    // TODO select first conversation?

    chatViewConversation_.reset(nullptr);
    refreshPendingContactRequestTab();

    return widgets->welcome_view;
}

GtkWidget*
CppImpl::displayIncomingView(lrc::api::conversation::Info conversation, bool redraw_webview)
{
    chatViewConversation_.reset(new lrc::api::conversation::Info(conversation));
    GtkWidget* incoming_call_view =
        incoming_call_view_new(webkitChatContainer(redraw_webview),
                               lrc_->getAVModel(), accountInfo_,
                               chatViewConversation_.get());
    g_signal_connect(incoming_call_view, "call-hungup",
                     G_CALLBACK(on_incoming_call_view_decline_call), self);
    return incoming_call_view;
}

GtkWidget*
CppImpl::displayCurrentCallView(lrc::api::conversation::Info conversation, bool redraw_webview)
{
    chatViewConversation_.reset(new lrc::api::conversation::Info(conversation));
    auto* new_view = current_call_view_new(webkitChatContainer(redraw_webview),
                                           accountInfo_,
                                           chatViewConversation_.get(),
                                           lrc_->getAVModel(), *lrc_.get()); // TODO improve. Only LRC is needed

    try {
        auto contactUri = chatViewConversation_->participants.front();
        auto contactInfo = accountInfo_->contactModel->getContact(contactUri);
        if (auto chat_view = current_call_view_get_chat_view(CURRENT_CALL_VIEW(new_view))) {
            chat_view_update_temporary(CHAT_VIEW(chat_view));
        }
    } catch(...) { }

    g_signal_connect_swapped(new_view, "video-double-clicked",
                             G_CALLBACK(on_video_double_clicked), self);
    g_signal_connect(new_view, "button-press-event", G_CALLBACK(on_current_call_clicked), nullptr);
    return new_view;
}

GtkWidget*
CppImpl::displayChatView(lrc::api::conversation::Info conversation, bool redraw_webview)
{
    chatViewConversation_.reset(new lrc::api::conversation::Info(conversation));
    auto* new_view = chat_view_new(webkitChatContainer(redraw_webview), accountInfo_, chatViewConversation_.get(), lrc_->getAVModel());
    g_signal_connect_swapped(new_view, "hide-view-clicked", G_CALLBACK(on_hide_view_clicked), self);
    g_signal_connect(new_view, "add-conversation-clicked", G_CALLBACK(on_add_conversation_clicked), self);
    g_signal_connect(new_view, "place-audio-call-clicked", G_CALLBACK(on_place_audio_call_clicked), self);
    g_signal_connect(new_view, "place-call-clicked", G_CALLBACK(on_place_call_clicked), self);
    g_signal_connect(new_view, "send-text-clicked", G_CALLBACK(on_send_text_clicked), self);
    try {
        auto contactUri = chatViewConversation_->participants.front();
        auto contactInfo = accountInfo_->contactModel->getContact(contactUri);
        auto isPending = contactInfo.profileInfo.type == lrc::api::profile::Type::PENDING;
        if (isPending) {
            auto notifId = accountInfo_->id + ":request:" + contactUri;
            hide_notification(NOTIFIER(widgets->notifier), notifId.toStdString());
        }
    } catch(...) { }

    return new_view;
}

WebKitChatContainer*
CppImpl::webkitChatContainer(bool redraw_webview)
{
    if (widgets->webkit_chat_container && redraw_webview)
    {
        // The WebkitChatContainer doesn't like to be reparented
        // Sometimes, the view disappears. So, as a workaround,
        // we recreate the webview when changing the type of the window
        gtk_widget_destroy(widgets->webkit_chat_container);
        widgets->webkit_chat_container = nullptr;
    }
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
        if (!widgets->is_fullscreen_main_win) gtk_window_unfullscreen(GTK_WINDOW(self));
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
    changeView(WELCOME_VIEW_TYPE, current_item);
}

void
CppImpl::refreshPendingContactRequestTab()
{
    if (!accountInfo_)
        return;

    auto hasPendingRequests = accountInfo_->contactModel->hasPendingRequests();
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
    gtk_widget_set_visible(widgets->combobox_account_selector, show);
}

/// Update the account GtkComboBox from LRC data and select the given entry.
/// The widget visibility is changed depending on number of account found.
/// /note Default selection_row is -1, meaning no selection.
/// /note Default selected is "", meaning no forcing
std::size_t
CppImpl::refreshAccountSelectorWidget(int selection_row, const std::string& selected)
{
    auto store = gtk_list_store_new(6 /* # of cols */ ,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_UINT);
    GtkTreeIter iter;
    std::size_t enabled_accounts = 0;
    std::size_t idx = 0;
    foreachLrcAccount(*lrc_, [&] (const auto& acc_info) {
            ++enabled_accounts;
            if (!selected.empty() && selected == acc_info.id.toStdString()) {
                selection_row = idx;
            }
            gtk_list_store_append(store, &iter);
            std::string status = "";
            switch (acc_info.status) {
                case lrc::api::account::Status::ERROR_NEED_MIGRATION:
                    status = "NEEDS MIGRATION";
                    break;
                case lrc::api::account::Status::INVALID:
                case lrc::api::account::Status::UNREGISTERED:
                    status = "DISCONNECTED";
                    break;
                case lrc::api::account::Status::INITIALIZING:
                case lrc::api::account::Status::TRYING:
                    status = "TRYING";
                    break;
                case lrc::api::account::Status::REGISTERED:
                    status = "CONNECTED";
                    break;
            }
            gtk_list_store_set(store, &iter,
                               0 /* col # */ , qUtf8Printable(acc_info.id) /* celldata */,
                               1 /* col # */ , status.c_str() /* celldata */,
                               2 /* col # */ , qUtf8Printable(acc_info.profileInfo.avatar) /* celldata */,
                               3 /* col # */ , qUtf8Printable(acc_info.profileInfo.uri) /* celldata */,
                               4 /* col # */ , qUtf8Printable(acc_info.profileInfo.alias) /* celldata */,
                               5 /* col # */ , qUtf8Printable(acc_info.registeredName) /* celldata */,
                               -1 /* end */);
            ++idx;
        });

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       0 /* col # */ , "" /* celldata */,
                       1 /* col # */ , "" /* celldata */,
                       2 /* col # */ , "" /* celldata */,
                       3 /* col # */ , "" /* celldata */,
                       4 /* col # */ , "" /* celldata */,
                       5 /* col # */ , "" /* celldata */,
                       -1 /* end */);

    gtk_combo_box_set_model(
        GTK_COMBO_BOX(widgets->combobox_account_selector),
        GTK_TREE_MODEL(store)
    );
    widgets->set_top_account_flag = false;
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_account_selector), selection_row);

    return enabled_accounts;
}

void
CppImpl::enterAccountCreationWizard(bool showControls)
{
    if (!widgets->account_creation_wizard) {
        widgets->account_creation_wizard = account_creation_wizard_new(lrc_->getAVModel(), lrc_->getAccountModel());
        g_object_add_weak_pointer(G_OBJECT(widgets->account_creation_wizard),
                                  reinterpret_cast<gpointer*>(&widgets->account_creation_wizard));
        g_signal_connect_swapped(widgets->account_creation_wizard, "account-creation-lock",
                                 G_CALLBACK(on_account_creation_lock), self);
        g_signal_connect_swapped(widgets->account_creation_wizard, "account-creation-unlock",
                                 G_CALLBACK(on_account_creation_unlock), self);
        g_signal_connect_swapped(widgets->account_creation_wizard, "account-creation-completed",
                                 G_CALLBACK(on_account_creation_completed), self);

        gtk_stack_add_named(GTK_STACK(widgets->stack_main_view),
                            widgets->account_creation_wizard,
                            ACCOUNT_CREATION_WIZARD_VIEW_NAME);
    }

    /* hide settings button until account creation is complete */
    gtk_widget_hide(widgets->hbox_settings);
    gtk_widget_hide(widgets->settings);
    showAccountSelectorWidget(showControls);

    gtk_widget_show(widgets->account_creation_wizard);
    gtk_stack_set_visible_child(GTK_STACK(widgets->stack_main_view), widgets->account_creation_wizard);
}

void
CppImpl::leaveAccountCreationWizard()
{
    auto old_view = gtk_stack_get_visible_child(GTK_STACK(widgets->stack_main_view));
    if(IS_ACCOUNT_MIGRATION_VIEW(old_view)) return;
    if (show_settings) {
        gtk_stack_set_visible_child(GTK_STACK(widgets->stack_main_view), widgets->last_settings_view);
        gtk_widget_show(widgets->hbox_settings);
    } else {
        gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack_main_view), CALL_VIEW_NAME);
    }

    /* destroy the wizard */
    if (widgets->account_creation_wizard) {
        gtk_container_remove(GTK_CONTAINER(widgets->stack_main_view), widgets->account_creation_wizard);
        gtk_widget_destroy(widgets->account_creation_wizard);
        widgets->account_creation_wizard = nullptr;
    }

    /* show the settings button */
    gtk_widget_show(widgets->settings);

    /* show the account selector */
    showAccountSelectorWidget();
}

void
CppImpl::onAccountSelectionChange(const std::string& id)
{
    auto oldId = accountInfo_? accountInfo_->id.toStdString() : "";

    if (id != oldId) {
        // Go to welcome view
        changeView(WELCOME_VIEW_TYPE);
        // Show conversation panel
        gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets->notebook_contacts), 0);
        // Reinit LRC
        updateLrc(id);
        // Update the welcome view
        welcome_update_view(WELCOME_VIEW(widgets->welcome_view));
        // Show pending contacts tab if necessary
        refreshPendingContactRequestTab();
        // Update account settings
        if (widgets->new_account_settings_view)
            new_account_settings_view_update(NEW_ACCOUNT_SETTINGS_VIEW(widgets->new_account_settings_view), true);
    }
}

void
CppImpl::enterSettingsView()
{
    /* show the settings */
    show_settings = true;

    /* show settings */
    gtk_image_set_from_icon_name(GTK_IMAGE(widgets->image_settings), "go-previous-symbolic",
                                 GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_set_tooltip_text(GTK_WIDGET(widgets->settings), _("Leave settings page"));

    gtk_widget_show(widgets->hbox_settings);

    /* make sure to start preview if we're showing the video settings */
    if (widgets->last_settings_view == widgets->media_settings_view)
        media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(widgets->media_settings_view), TRUE);

    /* make sure avatar widget is restored properly if we're showing general settings */
    if (widgets->last_settings_view == widgets->new_account_settings_view)
        new_account_settings_view_show(NEW_ACCOUNT_SETTINGS_VIEW(widgets->new_account_settings_view),
                                       TRUE);
    /* make sure to start preview if we're showing the video settings */
    if (widgets->last_settings_view == widgets->plugin_settings_view)
        plugin_settings_view_show(PLUGIN_SETTINGS_VIEW(widgets->plugin_settings_view), TRUE);

    gtk_stack_set_visible_child(GTK_STACK(widgets->stack_main_view), widgets->last_settings_view);
}

void
CppImpl::leaveSettingsView()
{
    /* hide the settings */
    show_settings = false;

    /* show calls */
    gtk_image_set_from_icon_name(GTK_IMAGE(widgets->image_settings), "emblem-system-symbolic",
                                 GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_set_tooltip_text(GTK_WIDGET(widgets->settings), _("Settings"));

    gtk_widget_hide(widgets->hbox_settings);

    /* make sure video preview is stopped, in case it was started */
    media_settings_view_show_preview(MEDIA_SETTINGS_VIEW(widgets->media_settings_view), FALSE);
    new_account_settings_view_show(NEW_ACCOUNT_SETTINGS_VIEW(widgets->new_account_settings_view),
                                   FALSE);
    plugin_settings_view_show(PLUGIN_SETTINGS_VIEW(widgets->plugin_settings_view), FALSE);

    gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack_main_view), CALL_VIEW_NAME);

    /* return to the welcome view if has_cleared_all_history. The reason is you can have been in a chatview before you
     * opened the settings view and did a clear all history. So without the code below, you'll see the chatview with
     * obsolete messages. It will also ensure to refresh last interaction printed in the conversations list.
     */
    if (has_cleared_all_history) {
        onAccountSelectionChange(accountInfo_->id.toStdString());
        resetToWelcome();
        has_cleared_all_history = false;
    }
}

std::string
CppImpl::getCurrentUid()
{
    const auto &treeview = gtk_notebook_get_current_page(
        GTK_NOTEBOOK(widgets->notebook_contacts)) == contactRequestsPageNum
            ? widgets->treeview_contact_requests
            : widgets->treeview_conversations;
    return conversations_view_get_current_selected(CONVERSATIONS_VIEW(treeview));
}

void
CppImpl::forCurrentConversation(const std::function<void(const lrc::api::conversation::Info&)>& func)
{
    const auto current = getCurrentUid();
    if (current.empty()) return;
    try {
        auto conversation = accountInfo_->conversationModel->getConversationForUID(current.c_str());
        if (conversation.participants.empty()) return;
        func(conversation);
    } catch (...) {
        g_warning("Can't retrieve conversation %s", current.c_str());
    }
}

bool
CppImpl::showOkCancelDialog(const std::string &title, const std::string &text)
{
    auto *confirm_dialog = gtk_message_dialog_new(
        GTK_WINDOW(self), GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
        "%s", text.c_str());
    gtk_window_set_title(GTK_WINDOW(confirm_dialog), title.c_str());
    gtk_dialog_set_default_response(GTK_DIALOG(confirm_dialog),
                                    GTK_RESPONSE_CANCEL);
    gtk_widget_show_all(confirm_dialog);

    auto res = gtk_dialog_run(GTK_DIALOG(confirm_dialog));

    gtk_widget_destroy(confirm_dialog);
    return res == GTK_RESPONSE_OK;
}

void
CppImpl::updateLrc(const std::string& id, const std::string& accountIdToFlagFreeable)
{
    // Disconnect old signals.
    QObject::disconnect(showLeaveMessageViewConnection_);
    QObject::disconnect(showChatViewConnection_);
    QObject::disconnect(showCallViewConnection_);
    QObject::disconnect(newTrustRequestNotification_);
    QObject::disconnect(closeTrustRequestNotification_);
    QObject::disconnect(slotNewInteraction_);
    QObject::disconnect(slotReadInteraction_);
    QObject::disconnect(modelSortedConnection_);
    QObject::disconnect(callChangedConnection_);
    QObject::disconnect(callStartedConnection_);
    QObject::disconnect(callEndedConnection_);
    QObject::disconnect(newIncomingCallConnection_);
    QObject::disconnect(historyClearedConnection_);
    QObject::disconnect(filterChangedConnection_);
    QObject::disconnect(newConversationConnection_);
    QObject::disconnect(conversationRemovedConnection_);

    if (!accountIdToFlagFreeable.empty()) {
        g_debug("Account %s flagged for removal. Mark it freeable.", accountIdToFlagFreeable.c_str());
        try {
            lrc_->getAccountModel().flagFreeable(accountIdToFlagFreeable.c_str());
        } catch (std::exception& e) {
            g_debug("Error while flagging %s for removal: '%s'.", accountIdToFlagFreeable.c_str(), e.what());
            g_debug("This is most likely an internal error, please report this bug.");
        } catch (...) {
            g_debug("Unexpected failure while flagging %s for removal.", accountIdToFlagFreeable.c_str());
        }
    }

    // Get the account selected
    if (!id.empty())
        accountInfo_ = &lrc_->getAccountModel().getAccountInfo(id.c_str());
    else
        accountInfo_ = nullptr;

    // Reinit tree views
    if (widgets->treeview_conversations) {
        auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(widgets->treeview_conversations));
        gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_conversations));
        gtk_widget_destroy(widgets->treeview_conversations);
    }
    widgets->treeview_conversations = conversations_view_new(accountInfo_);
    gtk_container_add(GTK_CONTAINER(widgets->scrolled_window_smartview), widgets->treeview_conversations);

    if (widgets->treeview_contact_requests) {
        auto selection_conversations = gtk_tree_view_get_selection(GTK_TREE_VIEW(widgets->treeview_contact_requests));
        gtk_tree_selection_unselect_all(GTK_TREE_SELECTION(selection_conversations));
        gtk_widget_destroy(widgets->treeview_contact_requests);
    }
    widgets->treeview_contact_requests = conversations_view_new(accountInfo_);
    gtk_container_add(GTK_CONTAINER(widgets->scrolled_window_contact_requests), widgets->treeview_contact_requests);

    if (!accountInfo_) return;

    // Connect to signals from LRC
    historyClearedConnection_ = QObject::connect(&*accountInfo_->conversationModel,
                                                 &lrc::api::ConversationModel::conversationCleared,
                                                 [this] (const QString& id) { slotConversationCleared(id.toStdString()); });

    modelSortedConnection_ = QObject::connect(&*accountInfo_->conversationModel,
                                              &lrc::api::ConversationModel::modelSorted,
                                              [this] { slotModelSorted(); });

    callChangedConnection_ = QObject::connect(&lrc_->getBehaviorController(),
                                              &lrc::api::BehaviorController::callStatusChanged,
                                              [this] (const QString& accountId, const QString& callId) { slotCallStatusChanged(accountId.toStdString(), callId.toStdString()); });

    callStartedConnection_ = QObject::connect(&*accountInfo_->callModel,
                                              &lrc::api::NewCallModel::callStarted,
                                              [this] (const QString& callId) { slotCallStarted(callId.toStdString()); });

    callEndedConnection_ = QObject::connect(&*accountInfo_->callModel,
                                              &lrc::api::NewCallModel::callEnded,
                                              [this] (const QString& callId) { slotCallEnded(callId.toStdString()); });

    newIncomingCallConnection_ = QObject::connect(&lrc_->getBehaviorController(),
                                                  &lrc::api::BehaviorController::showIncomingCallView,
                                                  [this] (const QString& accountId, lrc::api::conversation::Info origin) {
                                                    slotNewIncomingCall(accountId.toStdString(), origin); });

    filterChangedConnection_ = QObject::connect(&*accountInfo_->conversationModel,
                                                &lrc::api::ConversationModel::filterChanged,
                                                [this] { slotFilterChanged(); });

    newConversationConnection_ = QObject::connect(&*accountInfo_->conversationModel,
                                                  &lrc::api::ConversationModel::newConversation,
                                                  [this] (const QString& id) { slotNewConversation(id.toStdString()); });

    conversationRemovedConnection_ = QObject::connect(&*accountInfo_->conversationModel,
                                                      &lrc::api::ConversationModel::conversationRemoved,
                                                      [this] (const QString& id) { slotConversationRemoved(id.toStdString()); });

    showChatViewConnection_ = QObject::connect(&lrc_->getBehaviorController(),
                                               &lrc::api::BehaviorController::showChatView,
                                               [this] (const QString& id, lrc::api::conversation::Info origin) { slotShowChatView(id.toStdString(), origin); });

    showLeaveMessageViewConnection_ = QObject::connect(&lrc_->getBehaviorController(),
                                               &lrc::api::BehaviorController::showLeaveMessageView,
                                               [this] (const QString&, lrc::api::conversation::Info conv) { slotShowLeaveMessageView(conv); });

    showCallViewConnection_ = QObject::connect(&lrc_->getBehaviorController(),
                                               &lrc::api::BehaviorController::showCallView,
                                               [this] (const QString& id, lrc::api::conversation::Info origin) { slotShowCallView(id.toStdString(), origin); });

    newTrustRequestNotification_ = QObject::connect(&lrc_->getBehaviorController(),
                                                    &lrc::api::BehaviorController::newTrustRequest,
                                                    [this] (const QString& id, const QString& contactUri) { slotNewTrustRequest(id.toStdString(), contactUri.toStdString()); });

    closeTrustRequestNotification_ = QObject::connect(&lrc_->getBehaviorController(),
                                                      &lrc::api::BehaviorController::trustRequestTreated,
                                                      [this] (const QString& id, const QString& contactUri) { slotCloseTrustRequest(id.toStdString(), contactUri.toStdString()); });

    slotNewInteraction_ = QObject::connect(&lrc_->getBehaviorController(),
                                           &lrc::api::BehaviorController::newUnreadInteraction,
                                           [this] (const QString& accountId, const QString& conversation,
                                                  uint64_t interactionId, const lrc::api::interaction::Info& interaction)
                                                  { slotNewInteraction(accountId.toStdString(), conversation.toStdString(), interactionId, interaction); });

    slotReadInteraction_ = QObject::connect(&lrc_->getBehaviorController(),
                                            &lrc::api::BehaviorController::newReadInteraction,
                                            [this] (const QString& accountId, const QString& conversation, uint64_t interactionId)
                                                   { slotCloseInteraction(accountId.toStdString(), conversation.toStdString(), interactionId); });

    const gchar *text = gtk_entry_get_text(GTK_ENTRY(widgets->search_entry));
    currentTypeFilter_ = accountInfo_->profileInfo.type;
    accountInfo_->conversationModel->setFilter(text);
    accountInfo_->conversationModel->setFilter(currentTypeFilter_);
}

void
CppImpl::slotAccountAddedFromLrc(const std::string& id)
{
    auto currentIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(widgets->combobox_account_selector));
    if (currentIdx == -1)
        currentIdx = 0; // If no account selected, select the first account

    try {
        auto& account_model = lrc_->getAccountModel();

        const auto& account_info = account_model.getAccountInfo(id.c_str());
        auto old_view = gtk_stack_get_visible_child(GTK_STACK(widgets->stack_main_view));
        if(IS_ACCOUNT_CREATION_WIZARD(old_view)) {
            // TODO finalize (set avatar + register name)
            account_creation_wizard_account_added(ACCOUNT_CREATION_WIZARD(old_view), id);
        }
        if (!accountInfo_) {
            updateLrc(id);
            welcome_update_view(WELCOME_VIEW(widgets->welcome_view));
        }
        if (!gtk_stack_get_child_by_name(GTK_STACK(widgets->stack_main_view), NEW_ACCOUNT_SETTINGS_VIEW_NAME)) {
            widgets->new_account_settings_view = new_account_settings_view_new(accountInfo_, lrc_->getAVModel());
            gtk_stack_add_named(GTK_STACK(widgets->stack_main_view), widgets->new_account_settings_view,
                                NEW_ACCOUNT_SETTINGS_VIEW_NAME);
        }
        refreshAccountSelectorWidget(currentIdx, id);
        if (account_info.profileInfo.type == lrc::api::profile::Type::SIP) {
            enterSettingsView();
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets->radiobutton_new_account_settings), true);
        }
    } catch (...) {
        refreshAccountSelectorWidget(currentIdx, id);
    }
}

void
CppImpl::slotAccountRemovedFromLrc(const std::string& id)
{
    /* Before doing anything, we need to update the struct pointers
       and tell the LRC it can free the old structures. */
    updateLrc("", id);

    auto accounts = lrc_->getAccountModel().getAccountList();
    if (accounts.empty()) {
        leaveSettingsView();
        enterAccountCreationWizard();
        return;
    }

    /* Update Account selector. This will trigger an update of the LRC
       and of all elements of the main view, we don't need to do it
       manually here. */
    refreshAccountSelectorWidget(0);
}

void
CppImpl::slotInvalidAccountFromLrc(const std::string& id)
{
    auto old_view = gtk_stack_get_visible_child(GTK_STACK(widgets->stack_main_view));
    if(IS_ACCOUNT_CREATION_WIZARD(old_view)) {
        account_creation_show_error_view(ACCOUNT_CREATION_WIZARD(old_view), id);
    }
}

void
CppImpl::slotAccountNeedsMigration(const std::string& id)
{
    accountInfoForMigration_ = &lrc_->getAccountModel().getAccountInfo(id.c_str());
    if (accountInfoForMigration_->status == lrc::api::account::Status::ERROR_NEED_MIGRATION) {
        leaveSettingsView();
        widgets->account_migration_view = account_migration_view_new(accountInfoForMigration_);
        g_signal_connect_swapped(widgets->account_migration_view, "account-migration-completed",
                                    G_CALLBACK(on_handle_account_migrations), self);
        g_signal_connect_swapped(widgets->account_migration_view, "account-migration-failed",
                                    G_CALLBACK(on_handle_account_migrations), self);

        gtk_widget_hide(widgets->settings);
        showAccountSelectorWidget(false);
        gtk_widget_show(widgets->account_migration_view);
        gtk_stack_add_named(
            GTK_STACK(widgets->stack_main_view),
            widgets->account_migration_view,
            ACCOUNT_MIGRATION_VIEW_NAME);
        gtk_stack_set_visible_child_name(
            GTK_STACK(widgets->stack_main_view),
            ACCOUNT_MIGRATION_VIEW_NAME);
    }
}

lrc::api::conversation::Info
CppImpl::getCurrentConversation(GtkWidget* frame_call)
{
    lrc::api::conversation::Info current_item;
    current_item.uid = "-1";
    if (IS_CHAT_VIEW(frame_call)) {
        current_item = chat_view_get_conversation(CHAT_VIEW(frame_call));
    } else if (IS_CURRENT_CALL_VIEW(frame_call)) {
        current_item = current_call_view_get_conversation(CURRENT_CALL_VIEW(frame_call));
    } else if (IS_INCOMING_CALL_VIEW(frame_call)) {
        current_item = incoming_call_view_get_conversation(INCOMING_CALL_VIEW(frame_call));
    }

    return current_item;
}

void
CppImpl::slotAccountStatusChanged(const std::string& id)
{
    if (!accountInfo_) {
        updateLrc(id);
        welcome_update_view(WELCOME_VIEW(widgets->welcome_view));
        return;
    }

    new_account_settings_view_update(NEW_ACCOUNT_SETTINGS_VIEW(widgets->new_account_settings_view), false);
    auto currentIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(widgets->combobox_account_selector));
    if (currentIdx == -1)
        currentIdx = 0; // If no account selected, select the first account
    // NOTE: Because the currentIdx can change (accounts can be re-ordered), force to select the accountInfo_->id
    refreshAccountSelectorWidget(currentIdx, accountInfo_->id.toStdString());

    if (accountInfo_->status == lrc::api::account::Status::ERROR_NEED_MIGRATION) {
        slotAccountNeedsMigration(id);
        return;
    }

    auto* frame_call = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
    conversations_view_select_conversation(CONVERSATIONS_VIEW(widgets->treeview_conversations),
                                           getCurrentConversation(frame_call).uid.toStdString());

    if (IS_CHAT_VIEW(frame_call)) {
        chat_view_update_temporary(CHAT_VIEW(frame_call));
    }
}

void
CppImpl::slotConversationCleared(const std::string& uid)
{
    // Change the view when the history is cleared.
    auto* old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
    g_return_if_fail(IS_CHAT_VIEW(old_view));

    lrc::api::conversation::Info current_item = getCurrentConversation(old_view);

    if (current_item.uid.toStdString() == uid) {
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
    auto* frame_call = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
    conversations_view_select_conversation(CONVERSATIONS_VIEW(widgets->treeview_conversations),
                                           getCurrentConversation(frame_call).uid.toStdString());
    refreshPendingContactRequestTab();
}

void
CppImpl::slotCallStatusChanged(const std::string& accountId, const std::string& callId)
{
    try {
        auto& accountInfo = lrc_->getAccountModel().getAccountInfo(accountId.c_str());
        auto call = accountInfo.callModel->getCall(callId.c_str());
        auto peer = call.peerUri.remove("ring:");
        QString avatar = "", name = "", uri = "";
        std::string notifId = "";
        std::string conversation = "";
        try {
            auto contactInfo = accountInfo.contactModel->getContact(peer);
            uri = contactInfo.profileInfo.uri;
            avatar = contactInfo.profileInfo.avatar;
            name = accountInfo.contactModel->bestNameForContact(peer);
            if (name.isEmpty()) {
                name = accountInfo.contactModel->bestIdForContact(peer);
            }
            notifId = accountId + ":call:" + callId;
        } catch (...) {
            g_warning("Can't get contact for account %s. Don't show notification", accountId.c_str());
            return;
        }

        conversation = get_notification_conversation(NOTIFIER(widgets->notifier), notifId);

        if (call.status == lrc::api::call::Status::IN_PROGRESS) {
            // Call answered and in progress; close the notification
            hide_notification(NOTIFIER(widgets->notifier), notifId);
        } else if (call.status == lrc::api::call::Status::ENDED) {
            // Call ended; close the notification
            if (hide_notification(NOTIFIER(widgets->notifier), notifId)
                && call.startTime.time_since_epoch().count() == 0) {
                // This was a missed call; show a missed call notification
                name.remove('\r');
                auto body = _("Missed call from ") + name.toStdString();
                show_notification(NOTIFIER(widgets->notifier),
                                avatar.toStdString(), uri.toStdString(), name.toStdString(),
                                accountId + ":interaction:" + conversation + ":0",
                                _("Missed call"), body, NotificationType::CHAT);
            }
        }
    } catch (const std::exception& e) {
        g_warning("Can't get call %s for this account.", callId.c_str());
    }
}

void
CppImpl::slotCallStarted(const std::string& /*callId*/)
{
    if (lrc::api::Lrc::activeCalls().size() == 1) {
        GtkApplication* app = gtk_window_get_application(GTK_WINDOW(self));
        if (app) {
            inhibitionCookie = gtk_application_inhibit(
                app, GTK_WINDOW(self),
                static_cast<GtkApplicationInhibitFlags>(GTK_APPLICATION_INHIBIT_SUSPEND|GTK_APPLICATION_INHIBIT_IDLE),
                _("There are active calls"));
            g_debug("Inhibition was activated.");
        }
    }
}

void
CppImpl::slotCallEnded(const std::string& /*callId*/)
{
    if (lrc::api::Lrc::activeCalls().empty()) {
        GtkApplication* app = gtk_window_get_application(GTK_WINDOW(self));
        if (app) {
            gtk_application_uninhibit(app, inhibitionCookie);
            g_debug("Inhibition was deactivated.");
        }
    }
}

void
CppImpl::slotNewIncomingCall(const std::string& accountId, lrc::api::conversation::Info origin)
{
    if (g_settings_get_boolean(widgets->window_settings, "bring-window-to-front")) {
        g_settings_set_boolean(widgets->window_settings, "show-main-window", TRUE);
    }
    auto callId = origin.callId;
    try {
        auto& accountInfo = lrc_->getAccountModel().getAccountInfo(accountId.c_str());
        auto call = accountInfo.callModel->getCall(callId);
        // workaround for https://git.jami.net/savoirfairelinux/ring-lrc/issues/433
        if (call.status == lrc::api::call::Status::INCOMING_RINGING) {
            auto peer = call.peerUri.remove("ring:");
            auto& contactModel = accountInfo.contactModel;
            QString avatar = "", name = "", uri = "";
            std::string notifId = "";
            try {
                auto contactInfo = contactModel->getContact(peer);
                uri = contactInfo.profileInfo.uri;
                avatar = contactInfo.profileInfo.avatar;
                name = contactInfo.profileInfo.alias;
                if (name.isEmpty()) {
                    name = contactInfo.registeredName;
                    if (name.isEmpty()) {
                        name = contactInfo.profileInfo.uri;
                    }
                }
                notifId = accountInfo.id.toStdString() + ":call:" + callId.toStdString();
            } catch (...) {
                g_warning("Can't get contact for account %s. Don't show notification", qUtf8Printable(accountInfo.id));
                return;
            }

            if (g_settings_get_boolean(widgets->window_settings, "enable-call-notifications")
                && !accountInfo.confProperties.isRendezVous
                && !accountInfo.confProperties.autoAnswer
                && !has_notification(NOTIFIER(widgets->notifier), notifId)) {
                name.remove('\r');
                auto body = name.toStdString() + _(" is calling you!");
                show_notification(NOTIFIER(widgets->notifier),
                                avatar.toStdString(), uri.toStdString(), name.toStdString(),
                                notifId, _("Incoming call"), body, NotificationType::CALL,
                                origin.uid.toStdString());
            }
        }
    } catch (const std::exception& e) {
        g_warning("Can't get call %s for this account.", callId.toStdString().c_str());
    }

    if (not accountInfo_ or accountInfo_->id.toStdString() != accountId)
        return;

    // call changeView even if we are already in an incoming call view, since
    // the incoming call view holds a copy of the conversation info which has
    // the be updated
    changeView(INCOMING_CALL_VIEW_TYPE, origin);
}

void
CppImpl::slotFilterChanged()
{
    // Synchronize selection when filter changes
    auto* old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
    auto current_item = getCurrentConversation(old_view);
    conversations_view_select_conversation(CONVERSATIONS_VIEW(widgets->treeview_conversations), current_item.uid.toStdString());

    // Get if conversation still exists.
    auto& conversationModel = accountInfo_->conversationModel;
    auto conversations = conversationModel->allFilteredConversations();
    auto conversation = std::find_if(
        conversations.begin(), conversations.end(),
        [current_item](const lrc::api::conversation::Info& conversation) {
            return current_item.uid == conversation.uid;
        });

    if (conversation == conversations.end())
        changeView(WELCOME_VIEW_TYPE);
}

void
CppImpl::slotNewConversation(const std::string& uid)
{
    gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets->notebook_contacts), 0);
    accountInfo_->conversationModel->setFilter(lrc::api::profile::Type::RING);
    gtk_entry_set_text(GTK_ENTRY(widgets->search_entry), "");
    accountInfo_->conversationModel->setFilter("");
    // Select new conversation if contact added
    auto* old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
    if (IS_WELCOME_VIEW(old_view)) {
        accountInfo_->conversationModel->selectConversation(uid.c_str());
        if (chatViewConversation_) {
            try {
                auto contactUri =  chatViewConversation_->participants.front();
                auto contactInfo = accountInfo_->contactModel->getContact(contactUri);
                chat_view_update_temporary(CHAT_VIEW(gtk_bin_get_child(GTK_BIN(widgets->frame_call))));
            } catch(...) { }
        }
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
    if (current_item.uid.toStdString() == uid)
        changeView(WELCOME_VIEW_TYPE);
}

void
CppImpl::slotShowChatView(const std::string& id, lrc::api::conversation::Info origin)
{
    auto* old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
    if (IS_INCOMING_CALL_VIEW(old_view) && is_showing_let_a_message_view(INCOMING_CALL_VIEW(old_view), origin)) {
        return;
    }
    if (accountInfo_->id.toStdString() != id)
        return;
    // Show chat view if not in call (unless if it's the same conversation)
    lrc::api::conversation::Info current_item;
    current_item.uid = "-1";
    if (IS_CHAT_VIEW(old_view))
        current_item = chat_view_get_conversation(CHAT_VIEW(old_view));
    // Do not show a conversation without any participants
    if (origin.participants.empty()) return;
    auto firstContactUri = origin.participants.front();
    auto contactInfo = accountInfo_->contactModel->getContact(firstContactUri);
    // change view if necessary or just update temporary
    if (current_item.uid != origin.uid) {
        changeView(CHAT_VIEW_TYPE, origin);
    } else {
        chat_view_update_temporary(CHAT_VIEW(old_view));
    }
}

void
CppImpl::slotShowLeaveMessageView(lrc::api::conversation::Info conv)
{
    auto* current_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
    if (IS_INCOMING_CALL_VIEW(current_view)
        && accountInfo_->profileInfo.type != lrc::api::profile::Type::SIP)
        incoming_call_view_let_a_message(INCOMING_CALL_VIEW(current_view), conv);
}

void
CppImpl::slotShowCallView(const std::string& id, lrc::api::conversation::Info origin)
{
    if (accountInfo_->id.toStdString() != id)
        return;
    // Change the view if we want a different view.
    auto* old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));

    lrc::api::conversation::Info current_item;
    if (IS_CURRENT_CALL_VIEW(old_view))
        current_item = current_call_view_get_conversation(CURRENT_CALL_VIEW(old_view));

    if (IS_CURRENT_CALL_VIEW(old_view) && origin.uid == current_item.uid) {
        auto rendered = QString::fromStdString(current_call_view_get_rendered_call(CURRENT_CALL_VIEW(old_view)));
        auto sameCall = origin.confId.isEmpty() ? origin.callId == rendered : origin.confId == rendered;
        if (sameCall)
            return;
    }

    changeView(CURRENT_CALL_VIEW_TYPE, origin);
}

void
CppImpl::slotNewTrustRequest(const std::string& id, const std::string& contactUri)
{
    try {
        auto& accountInfo = lrc_->getAccountModel().getAccountInfo(id.c_str());
        auto notifId = accountInfo.id.toStdString() + ":request:" + contactUri;
        QString avatar = "", name = "", uri = "";
        auto& contactModel = accountInfo.contactModel;
        try {
            auto contactInfo = contactModel->getContact(contactUri.c_str());
            uri = contactInfo.profileInfo.uri;
            avatar = contactInfo.profileInfo.avatar;
            name = contactInfo.profileInfo.alias;
            if (name.isEmpty()) {
                name = contactInfo.registeredName;
                if (name.isEmpty()) {
                    name = contactInfo.profileInfo.uri;
                }
            }
        } catch (...) {
            g_warning("Can't get contact for account %s. Don't show notification", qUtf8Printable(accountInfo.id));
            return;
        }
        if (g_settings_get_boolean(widgets->window_settings, "enable-pending-notifications")) {
            name.remove('\r');
            auto body = _("New request from ") + name.toStdString();
            show_notification(NOTIFIER(widgets->notifier),
                              avatar.toStdString(), uri.toStdString(), name.toStdString(),
                              notifId, _("Trust request"), body, NotificationType::REQUEST);
        }
    } catch (...) {
        g_warning("Can't get account %s", id.c_str());
    }
}

void
CppImpl::slotCloseTrustRequest(const std::string& id, const std::string& contactUri)
{
    try {
        auto notifId = id + ":request:" + contactUri;
        hide_notification(NOTIFIER(widgets->notifier), notifId);
    } catch (...) {
        g_warning("Can't get account %s", id.c_str());
    }
}

void
CppImpl::slotNewInteraction(const std::string& accountId, const std::string& conversation,
                            uint64_t interactionId, const lrc::api::interaction::Info& interaction)
{
    if (chatViewConversation_ && chatViewConversation_->uid == QString::fromStdString(conversation)) {
        auto *old_view = gtk_bin_get_child(GTK_BIN(widgets->frame_call));
        if (IS_CURRENT_CALL_VIEW(old_view)) {
            current_call_view_show_chat(CURRENT_CALL_VIEW(old_view));
        }
        if (gtk_window_is_active(GTK_WINDOW(self))) {
            return;
        }
    }
    try {
        auto& accountInfo = lrc_->getAccountModel().getAccountInfo(QString::fromStdString(accountId));
        auto notifId = accountInfo.id.toStdString() + ":interaction:" + conversation + ":" + std::to_string(interactionId);
        auto& contactModel = accountInfo.contactModel;
        auto& conversationModel = accountInfo.conversationModel;
        for (const auto& conv : conversationModel->allFilteredConversations())
        {
            if (conv.uid.toStdString() == conversation) {
                if (conv.participants.empty()) return;
                QString avatar = "", name = "", uri = "";
                try {
                    auto contactInfo = contactModel->getContact(conv.participants.front());
                    uri = contactInfo.profileInfo.uri;
                    avatar = contactInfo.profileInfo.avatar;
                    name = contactInfo.profileInfo.alias;
                    if (name.isEmpty()) {
                        name = contactInfo.registeredName;
                        if (name.isEmpty()) {
                            name = contactInfo.profileInfo.uri;
                        }
                    }
                } catch (...) {
                    g_warning("Can't get contact for account %s. Don't show notification", qUtf8Printable(accountInfo.id));
                    return;
                }

                if (g_settings_get_boolean(widgets->window_settings, "enable-chat-notifications")) {
                    name.remove('\r');
                    auto body = name + ": " + interaction.body;
                    show_notification(NOTIFIER(widgets->notifier),
                                      avatar.toStdString(), uri.toStdString(), name.toStdString(),
                                      notifId, _("New message"), body.toStdString(), NotificationType::CHAT);
                }
            }
        }
    } catch (...) {
        g_warning("Can't get account %s", accountId.c_str());
    }
}

void
CppImpl::slotCloseInteraction(const std::string& accountId, const std::string& conversation, uint64_t interactionId)
{
    if (!gtk_window_is_active(GTK_WINDOW(self))
        || (chatViewConversation_ && chatViewConversation_->uid != QString::fromStdString(conversation))) {
            return;
    }
    try {
        auto& accountInfo = lrc_->getAccountModel().getAccountInfo(QString::fromStdString(accountId));
        auto notifId = accountInfo.id.toStdString() + ":interaction:" + conversation + ":" + std::to_string(interactionId);
        hide_notification(NOTIFIER(widgets->notifier), notifId);
    } catch (...) {
        g_warning("Can't get account %s", accountId.c_str());
    }
}

void
CppImpl::slotProfileUpdated(const std::string& id)
{
    auto currentIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(widgets->combobox_account_selector));
    if (currentIdx == -1)
        currentIdx = 0; // If no account selected, select the first account
   refreshAccountSelectorWidget(currentIdx, id);
}

}} // namespace <anonymous>::details

void
main_window_reset(MainWindow* self)
{
    g_return_if_fail(IS_MAIN_WINDOW(self));
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    if (priv->cpp->show_settings)
        priv->cpp->leaveSettingsView();
}

bool
main_window_can_close(MainWindow* self)
{
    g_return_val_if_fail(IS_MAIN_WINDOW(self), true);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(self));
    if (!lrc::api::Lrc::activeCalls().empty()) {
        auto res = priv->cpp->showOkCancelDialog(
            _("Stop current call?"),
            _("A call is currently ongoing. Do you want to close the window and stop all current calls?"));
        if (res) lrc::api::NewCallModel::hangupCallsAndConferences();
        return res;
    }
    return true;
}

void
main_window_display_account_list(MainWindow *win)
{
    g_return_if_fail(IS_MAIN_WINDOW(win));
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(win));
    gtk_combo_box_popup(GTK_COMBO_BOX(priv->combobox_account_selector));
}

void
main_window_search(MainWindow *win)
{
    g_return_if_fail(IS_MAIN_WINDOW(win));
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(win));
    gtk_widget_grab_focus(GTK_WIDGET(priv->search_entry));
}

void
main_window_conversations_list(MainWindow *win)
{
    g_return_if_fail(IS_MAIN_WINDOW(win));
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(win));
    auto smartViewPageNum = gtk_notebook_page_num(GTK_NOTEBOOK(priv->notebook_contacts),
                                                               priv->scrolled_window_smartview);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->notebook_contacts), smartViewPageNum);
    gtk_widget_grab_focus(GTK_WIDGET(priv->treeview_conversations));
}

void
main_window_requests_list(MainWindow *win)
{
    g_return_if_fail(IS_MAIN_WINDOW(win));
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(win));
    if (!priv->cpp->accountInfo_->contactModel->hasPendingRequests()) return;
    auto contactRequestsPageNum = gtk_notebook_page_num(GTK_NOTEBOOK(priv->notebook_contacts),
                                                               priv->scrolled_window_contact_requests);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->notebook_contacts), contactRequestsPageNum);
    gtk_widget_grab_focus(GTK_WIDGET(priv->treeview_contact_requests));
}

void
main_window_audio_call(MainWindow *win)
{
    g_return_if_fail(IS_MAIN_WINDOW(win));
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(win));
    g_return_if_fail(priv && priv->cpp);

    priv->cpp->forCurrentConversation([&](const auto& conversation){
        priv->cpp->accountInfo_->conversationModel->placeAudioOnlyCall(conversation.uid);
    });
}

void
main_window_clear_history(MainWindow *win)
{
    g_return_if_fail(IS_MAIN_WINDOW(win));
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(win));
    g_return_if_fail(priv && priv->cpp && priv->cpp->accountInfo_);

    priv->cpp->forCurrentConversation([&](const auto &conversation) {
        auto res = priv->cpp->showOkCancelDialog(
            _("Clear history"),
            _("Do you really want to clear the history of this conversation?"));
        if (!res) return;
        priv->cpp->accountInfo_->conversationModel->clearHistory(conversation.uid);
    });
}

void
main_window_remove_conversation(MainWindow *win)
{
    g_return_if_fail(IS_MAIN_WINDOW(win));
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(win));
    g_return_if_fail(priv && priv->cpp && priv->cpp->accountInfo_);

    priv->cpp->forCurrentConversation([&](const auto& conversation){
        auto res = priv->cpp->showOkCancelDialog(
            _("Remove conversation"),
            _("Do you really want to remove this conversation?"));
        if (!res) return;
        priv->cpp->accountInfo_->conversationModel->removeConversation(conversation.uid);
    });
}

void
main_window_block_contact(MainWindow *win)
{
    g_return_if_fail(IS_MAIN_WINDOW(win));
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(win));
    g_return_if_fail(priv && priv->cpp && priv->cpp->accountInfo_);

    priv->cpp->forCurrentConversation([&](const auto& conversation){
        auto res = priv->cpp->showOkCancelDialog(
            _("Block contact"),
            _("Do you really want to block this contact?"));
        if (!res) return;
        priv->cpp->accountInfo_->conversationModel->removeConversation(conversation.uid, true);
    });
}

void
main_window_unblock_contact(MainWindow *win)
{
    g_return_if_fail(IS_MAIN_WINDOW(win));
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(win));
    g_return_if_fail(priv && priv->cpp && priv->cpp->accountInfo_);

    priv->cpp->forCurrentConversation([&](const auto& conversation){
        auto& uri = conversation.participants[0];

        auto contactInfo = priv->cpp->accountInfo_->contactModel->getContact(uri);
        if (!contactInfo.isBanned) return;
        priv->cpp->accountInfo_->contactModel->addContact(contactInfo);
    });
}

void
main_window_copy_contact(MainWindow *win)
{
    g_return_if_fail(IS_MAIN_WINDOW(win));
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(win));
    g_return_if_fail(priv && priv->cpp && priv->cpp->accountInfo_);

    priv->cpp->forCurrentConversation([&](const auto& conversation){
        auto& contact = priv->cpp->accountInfo_->contactModel->getContact(conversation.participants.front());
        auto bestName = contact.registeredName.isEmpty() ? contact.profileInfo.uri : contact.registeredName;
        auto text = (gchar *)qUtf8Printable(bestName);
        GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        gtk_clipboard_set_text(clip, text, -1);
        clip = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
        gtk_clipboard_set_text(clip, text, -1);
    });
}

void
main_window_add_contact(MainWindow *win)
{
    g_return_if_fail(IS_MAIN_WINDOW(win));
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(win));
    g_return_if_fail(priv && priv->cpp);

    priv->cpp->forCurrentConversation([&](const auto &conversation) {
        priv->cpp->accountInfo_->conversationModel->makePermanent(conversation.uid);
    });
}

void
main_window_accept_call(MainWindow *win)
{
    g_return_if_fail(IS_MAIN_WINDOW(win));
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(win));
    g_return_if_fail(priv && priv->cpp && priv->cpp->accountInfo_);

    // Select the first conversation of the list
    auto current = priv->cpp->getCurrentUid();
    if (current.empty()) return;
    try {
        auto conversation = priv->cpp->accountInfo_->conversationModel->getConversationForUID(current.c_str());
        if (conversation.participants.empty()) return;
        auto contactUri = conversation.participants.at(0);
        auto contact = priv->cpp->accountInfo_->contactModel->getContact(contactUri);
        // If the contact is pending, we should accept its request
        if (contact.profileInfo.type == lrc::api::profile::Type::PENDING)
            priv->cpp->accountInfo_->conversationModel->makePermanent(conversation.uid);
        // Accept call
        priv->cpp->accountInfo_->callModel->accept(conversation.callId);
    } catch (...) {
        g_warning("Can't retrieve conversation %s", current.c_str());
    }
}

void
main_window_decline_call(MainWindow *win)
{
    g_return_if_fail(IS_MAIN_WINDOW(win));
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(win));
    g_return_if_fail(priv && priv->cpp);

    priv->cpp->forCurrentConversation([&](const auto &conversation) {
        priv->cpp->accountInfo_->callModel->hangUp(conversation.callId);
    });
}

void
main_window_toggle_fullscreen(MainWindow *win)
{
    g_return_if_fail(IS_MAIN_WINDOW(win));
    auto *priv = MAIN_WINDOW_GET_PRIVATE(MAIN_WINDOW(win));
    g_return_if_fail(priv && priv->cpp);
    if (priv->cpp->is_fullscreen) return;

    if (priv->is_fullscreen_main_win) {
        gtk_window_unfullscreen(GTK_WINDOW(win));
    } else {
        gtk_window_fullscreen(GTK_WINDOW(win));
    }

    priv->is_fullscreen_main_win = !priv->is_fullscreen_main_win;
}

//==============================================================================

static void
main_window_init(MainWindow *win)
{
    auto* priv = MAIN_WINDOW_GET_PRIVATE(win);
    gtk_widget_init_template(GTK_WIDGET(win));

#if USE_LIBNM
    priv->nm_client = nullptr;
#endif

    // CppImpl ctor
    priv->cpp = new details::CppImpl {*win};
    priv->notifier = notifier_new();
    priv->cpp->init();
}

static void
main_window_dispose(GObject *object)
{
    auto* self = MAIN_WINDOW(object);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(self);

    if (priv->cpp) {
        delete priv->cpp;
        priv->cpp = nullptr;
    }

    // cancel any pending cancellable operations
    if (priv->cancellable) {
        g_cancellable_cancel(priv->cancellable);
        g_object_unref(priv->cancellable);
        priv->cancellable = nullptr;
    }

#if USE_LIBNM
    // clear NetworkManager client if it was used
    g_clear_object(&priv->nm_client);
#endif

    if (priv->update_download_folder) {
        g_signal_handler_disconnect(priv->general_settings_view, priv->update_download_folder);
        priv->update_download_folder = 0;
    }

    if (priv->notifier) {
        g_signal_handler_disconnect(priv->notifier, priv->notif_chat_view);
        priv->notif_chat_view = 0;
        g_signal_handler_disconnect(priv->notifier, priv->notif_accept_pending);
        priv->notif_accept_pending = 0;
        g_signal_handler_disconnect(priv->notifier, priv->notif_refuse_pending);
        priv->notif_refuse_pending = 0;
        g_signal_handler_disconnect(priv->notifier, priv->notif_accept_call);
        priv->notif_accept_call = 0;
        g_signal_handler_disconnect(priv->notifier, priv->notif_decline_call);
        priv->notif_decline_call = 0;

        gtk_widget_destroy(priv->notifier);
        priv->notifier = nullptr;
    }

    G_OBJECT_CLASS(main_window_parent_class)->dispose(object);
}

static void
main_window_finalize(GObject *object)
{
    auto* self = MAIN_WINDOW(object);
    auto* priv = MAIN_WINDOW_GET_PRIVATE(self);

    g_clear_object(&priv->settings);

    G_OBJECT_CLASS(main_window_parent_class)->finalize(object);
}

static void
main_window_class_init(MainWindowClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = main_window_finalize;
    G_OBJECT_CLASS(klass)->dispose = main_window_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/net/jami/JamiGnome/mainwindow.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, vbox_left_pane);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, notebook_contacts);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, scrolled_window_smartview);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, menu);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, image_ring);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, image_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, hbox_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, search_entry);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, stack_main_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, vbox_call_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, frame_call);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, button_new_conversation  );
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, radiobutton_general_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, radiobutton_plugin_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, radiobutton_media_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, radiobutton_new_account_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, combobox_account_selector);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, scrolled_window_contact_requests);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, image_contact_requests_list);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MainWindow, image_conversations_list);
}

GtkWidget *
main_window_new(GtkApplication *app)
{
    gpointer win = g_object_new(MAIN_WINDOW_TYPE, "application", app, NULL);
    gtk_window_set_title(GTK_WINDOW(win), JAMI_CLIENT_NAME);
    return (GtkWidget *)win;
}
