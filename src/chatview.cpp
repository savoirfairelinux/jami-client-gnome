/*
 *  Copyright (C) 2016-2021 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Alexandre Viau <alexandre.viau@savoirfairelinux.com>
 *  Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
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

#include "chatview.h"

// std
#include <algorithm>
#include <fstream>
#include <sstream>

// GTK
#include <glib/gi18n.h>
#include <clutter-gtk/clutter-gtk.h>

// Qt
#include <QSize>

// LRC
#include <api/call.h>
#include <api/contactmodel.h>
#include <api/conversationmodel.h>
#include <api/contact.h>
#include <api/lrc.h>
#include <api/newcallmodel.h>
#include <api/avmodel.h>

// Client
#include "marshals.h"
#include "utils/files.h"
#include "native/pixbufmanipulator.h"
#include "video/video_widget.h"

/* size of avatar */
static constexpr int AVATAR_WIDTH  = 150; /* px */
static constexpr int AVATAR_HEIGHT = 150; /* px */

enum class RecordAction {
    RECORD,
    STOP,
    SEND
};

struct CppImpl {
    struct Interaction {
        std::string conv;
        uint64_t id;
        lrc::api::interaction::Info info;
    };
    std::vector<Interaction> interactionsBuffer_;
    lrc::api::AVModel* avModel_;
    RecordAction current_action_ {RecordAction::RECORD};

    // store current recording location
    std::string saveFileName_;
};

struct _ChatView
{
    GtkBox parent;
};

struct _ChatViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _ChatViewPrivate ChatViewPrivate;

struct _ChatViewPrivate
{
    GtkWidget *box_webkit_chat_container;
    GtkWidget *webkit_chat_container;

    GSettings *settings;

    lrc::api::conversation::Info* conversation_;
    AccountInfoPointer const * accountInfo_;

    QMetaObject::Connection new_interaction_connection;
    QMetaObject::Connection composing_changed_connection;
    QMetaObject::Connection interaction_removed;
    QMetaObject::Connection update_interaction_connection;
    QMetaObject::Connection update_add_to_conversations;
    QMetaObject::Connection local_renderer_connection;

    gulong webkit_ready;
    gulong webkit_send_text;
    gulong webkit_drag_drop;

    bool ready_ {false};
    bool readyToRecord_ {false};
    bool useDarkTheme {false};
    bool startRecorderWhenReady {false};
    std::string background;
    CppImpl* cpp;

    bool video_started_by_settings;
    GtkWidget* video_widget;
    GtkWidget* record_popover;
    GtkWidget* button_retry;
    GtkWidget* button_main_action;
    GtkWidget* label_time;
    bool is_video_record {false};
    guint timer_duration = 0;
    uint32_t duration = 0;

};

G_DEFINE_TYPE_WITH_PRIVATE(ChatView, chat_view, GTK_TYPE_BOX);

#define CHAT_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CHAT_VIEW_TYPE, ChatViewPrivate))

enum {
    NEW_MESSAGES_DISPLAYED,
    HIDE_VIEW_CLICKED,
    PLACE_CALL_CLICKED,
    PLACE_AUDIO_CALL_CLICKED,
    ADD_CONVERSATION_CLICKED,
    SEND_TEXT_CLICKED,
    LAST_SIGNAL
};

static guint chat_view_signals[LAST_SIGNAL] = { 0 };

static void init_video_widget(ChatView* self);
static void on_main_action_clicked(ChatView *self);
static void reset_recorder(ChatView *self);

static void
chat_view_dispose(GObject *object)
{
    ChatView *view;
    ChatViewPrivate *priv;

    view = CHAT_VIEW(object);
    priv = CHAT_VIEW_GET_PRIVATE(view);

    QObject::disconnect(priv->new_interaction_connection);
    QObject::disconnect(priv->composing_changed_connection);
    QObject::disconnect(priv->update_interaction_connection);
    QObject::disconnect(priv->interaction_removed);
    QObject::disconnect(priv->update_add_to_conversations);
    QObject::disconnect(priv->local_renderer_connection);

    /* Destroying the box will also destroy its children, and we wouldn't
     * want that. So we remove the webkit_chat_container from the box. */
    if (priv->webkit_chat_container) {
        /* disconnect for webkit signals */
        g_signal_handler_disconnect(priv->webkit_chat_container, priv->webkit_ready);
        priv->webkit_ready = 0;
        g_signal_handler_disconnect(priv->webkit_chat_container, priv->webkit_send_text);
        priv->webkit_send_text = 0;
        g_signal_handler_disconnect(priv->webkit_chat_container, priv->webkit_drag_drop);
        priv->webkit_drag_drop = 0;

        gtk_container_remove(
            GTK_CONTAINER(priv->box_webkit_chat_container),
            GTK_WIDGET(priv->webkit_chat_container)
        );
        priv->webkit_chat_container = nullptr;
    }

    if (priv->video_widget) {
        gtk_widget_destroy(priv->video_widget);
    }

    if (priv->record_popover) {
        gtk_widget_destroy(priv->record_popover);
    }

    G_OBJECT_CLASS(chat_view_parent_class)->dispose(object);
}

void
chat_view_set_dark_mode(ChatView *self, gboolean darkMode, const std::string& background)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);
    priv->useDarkTheme = darkMode;
    priv->background = background;
    if (!priv->ready_) return;
    webkit_chat_set_dark_mode(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container), darkMode, background);
}

gboolean
on_redraw(GtkWidget*, cairo_t*, ChatView* self)
{
    g_return_val_if_fail(IS_CHAT_VIEW(self), G_SOURCE_REMOVE);
    auto* priv = CHAT_VIEW_GET_PRIVATE(CHAT_VIEW(self));

    auto color = get_ambient_color(gtk_widget_get_toplevel(GTK_WIDGET(self)));
    bool current_theme = (color.red + color.green + color.blue) / 3 < .5;
    if (priv->useDarkTheme != current_theme) {
        std::stringstream background;
        background << "#" << std::hex << (int)(color.red * 256)
                          << (int)(color.green * 256)
                          << (int)(color.blue * 256);
        chat_view_set_dark_mode(self, current_theme, background.str());
    }
    return false;
}

static void
hide_chat_view(G_GNUC_UNUSED GtkWidget *widget, ChatView *self)
{
    g_signal_emit(G_OBJECT(self), chat_view_signals[HIDE_VIEW_CLICKED], 0);
}

static void
display_links_toggled(ChatView *self)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);
    if (priv->webkit_chat_container) {
        webkit_chat_container_set_display_links(
            WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
            g_settings_get_boolean(priv->settings, "enable-display-links")
        );
    }
}

static void
placecall_clicked(ChatView *self)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);
    if (!priv->conversation_) return;
    g_signal_emit(G_OBJECT(self), chat_view_signals[PLACE_CALL_CLICKED], 0, qUtf8Printable(priv->conversation_->uid));
}

static void
place_audio_call_clicked(ChatView *self)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);
    if (!priv->conversation_) return;
    g_signal_emit(G_OBJECT(self), chat_view_signals[PLACE_AUDIO_CALL_CLICKED], 0, qUtf8Printable(priv->conversation_->uid));
}

static void
add_to_conversations_clicked(ChatView *self)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);
    if (!priv->conversation_) return;
    g_signal_emit(G_OBJECT(self), chat_view_signals[ADD_CONVERSATION_CLICKED], 0, qUtf8Printable(priv->conversation_->uid));
}

static void
send_text_clicked(ChatView *self, const std::string& body)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);
    if (!priv->conversation_) return;
    g_signal_emit(G_OBJECT(self), chat_view_signals[SEND_TEXT_CLICKED],
        0, qUtf8Printable(priv->conversation_->uid), body.c_str());
}

static gchar*
file_to_manipulate(GtkWindow* top_window, bool send)
{
    GtkFileChooserAction action = send? GTK_FILE_CHOOSER_ACTION_OPEN : GTK_FILE_CHOOSER_ACTION_SAVE;
    gint res;
    gchar *filename = nullptr;

#if GTK_CHECK_VERSION(3,20,0)
    GtkFileChooserNative *native = gtk_file_chooser_native_new(
        send ? _("Send File") : _("Save File"),
        top_window,
        action,
        send ? _("_Open") : _("_Save"),
        _("_Cancel"));

    res = gtk_native_dialog_run(GTK_NATIVE_DIALOG(native));
    if (res == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(native));
    }

    g_object_unref(native);
#else
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        send ? _("Send File") : _("Save File"),
        top_window,
        action,
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        send ? _("_Open") : _("_Save"), GTK_RESPONSE_ACCEPT,
        nullptr);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    }

    gtk_widget_destroy(dialog);
#endif

    return filename;
}

static void update_chatview_frame(ChatView *self);

void
on_record_closed(GtkPopover*, ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    auto* priv = CHAT_VIEW_GET_PRIVATE(self);

    if (!priv->cpp->saveFileName_.empty()) {
        priv->cpp->avModel_->stopLocalRecorder(priv->cpp->saveFileName_.c_str());
        if (!priv->cpp->saveFileName_.empty()) {
            std::remove(priv->cpp->saveFileName_.c_str());
        }
        priv->cpp->saveFileName_ = "";
    }

    priv->cpp->current_action_ = RecordAction::RECORD;
    if (priv->timer_duration) g_source_remove(priv->timer_duration);
    if (priv->is_video_record) {
        priv->cpp->avModel_->stopPreview();
        QObject::disconnect(priv->local_renderer_connection);
    }
    priv->duration = 0;
}

void
init_control_box_buttons(ChatView *self, GtkWidget *box_contols) {
    g_return_if_fail(IS_CHAT_VIEW(self));
    auto* priv = CHAT_VIEW_GET_PRIVATE(self);

    std::string rsc = !priv->useDarkTheme && !priv->is_video_record ?
        "/net/jami/JamiGnome/retry" : "/net/jami/JamiGnome/retry-white";
    auto image_retry = gtk_image_new_from_resource (rsc.c_str());
    auto image_record = gtk_image_new_from_resource("/net/jami/JamiGnome/record");

    priv->button_retry = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(priv->button_retry), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(priv->button_retry, _("Retry"));
    gtk_button_set_image(GTK_BUTTON(priv->button_retry), image_retry);
    gtk_widget_set_size_request(GTK_WIDGET(priv->button_retry), 48, 48);
    auto context = gtk_widget_get_style_context(GTK_WIDGET(priv->button_retry));
    gtk_style_context_add_class(context, "record-button");
    g_signal_connect_swapped(priv->button_retry, "clicked", G_CALLBACK(reset_recorder), self);
    gtk_container_add(GTK_CONTAINER(box_contols), priv->button_retry);

    priv->button_main_action = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(priv->button_main_action), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(priv->button_main_action, _("Record"));
    gtk_button_set_image(GTK_BUTTON(priv->button_main_action), image_record);
    gtk_widget_set_size_request(GTK_WIDGET(priv->button_main_action), 48, 48);
    context = gtk_widget_get_style_context(GTK_WIDGET(priv->button_main_action));
    gtk_style_context_add_class(context, "record-button");
    g_signal_connect_swapped(priv->button_main_action, "clicked", G_CALLBACK(on_main_action_clicked), self);
    gtk_container_add(GTK_CONTAINER(box_contols), priv->button_main_action);

    gtk_widget_set_halign(box_contols, GTK_ALIGN_CENTER);
}

void
chat_view_show_recorder(ChatView *self, int pt_x, int pt_y, bool is_video_record)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    auto* priv = CHAT_VIEW_GET_PRIVATE(self);
    if (!priv->readyToRecord_) return;

    priv->is_video_record = is_video_record;
    if (is_video_record) priv->cpp->avModel_->startPreview();
    std::string css = is_video_record ? ".record-button { background: rgba(0, 0, 0, 0.2); border-radius: 50%; border: 0; transition: all 0.3s ease; } \
        .record-button:hover { background: rgba(0, 0, 0, 0.2); border-radius: 50%; border: 0; transition: all 0.3s ease; } \
        .label_time { color: white; }"
        : ".record-button { background: transparent; border-radius: 50%; border: 0; transition: all 0.3s ease; } \
        .record-button:hover { background: transparent; border-radius: 50%; border: 0; transition: all 0.3s ease; }";
    if (is_video_record) css += "";

    // CSS styles
    auto provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css.c_str(), -1, nullptr);
    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

#if GTK_CHECK_VERSION(3,22,0)
    GdkRectangle workarea = {};
    gdk_monitor_get_workarea(
        gdk_display_get_primary_monitor(gdk_display_get_default()),
        &workarea);
    auto widget_size = std::max(300, workarea.width / 6);
#else
    auto widget_size = std::max(300, gdk_screen_width() / 6);
#endif

    auto width = .0;
    auto height = .0;
    if (is_video_record) {
        auto deviceId = priv->cpp->avModel_->getDefaultDevice();
        auto settings = priv->cpp->avModel_->getDeviceSettings(deviceId);
        auto res = settings.size.toStdString();
        if (res.find("x") == std::string::npos) return;
        auto res_width = static_cast<double>(std::stoi(res.substr(0, res.find("x"))));
        auto res_height = static_cast<double>(std::stoi(res.substr(res.find("x") + 1)));
        auto max = std::max(res_width, res_height);

        width = res_width / max * widget_size;
        height = res_height / max * widget_size;
    } else {
        width = widget_size;
        height = 200;
    }

    if (priv->record_popover) {
        gtk_widget_destroy(priv->record_popover);
    }
    priv->record_popover = gtk_popover_new(GTK_WIDGET(priv->box_webkit_chat_container));
    g_signal_connect(priv->record_popover, "closed", G_CALLBACK(on_record_closed), self);
    gtk_popover_set_relative_to(GTK_POPOVER(priv->record_popover), GTK_WIDGET(priv->box_webkit_chat_container));

    if (is_video_record) {
        init_video_widget(self);
        gtk_container_add(GTK_CONTAINER(GTK_POPOVER(priv->record_popover)), priv->video_widget);
        gtk_widget_set_size_request(GTK_WIDGET(priv->video_widget), width, height);
    } else {
        auto* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        priv->label_time =  gtk_label_new("00:00");
        auto* attributes = pango_attr_list_new();
        auto* font_desc = pango_attr_font_desc_new(pango_font_description_from_string("24"));
        pango_attr_list_insert(attributes, font_desc);
        gtk_label_set_attributes(GTK_LABEL(priv->label_time), attributes);
        pango_attr_list_unref(attributes);
        auto* box_contols = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        init_control_box_buttons(self, box_contols);
        gtk_box_pack_start(GTK_BOX(box), priv->label_time, true, true, 0);
        gtk_box_pack_end(GTK_BOX(box), box_contols, false, false, 0);
        gtk_container_add(GTK_CONTAINER(GTK_POPOVER(priv->record_popover)), box);
    }

    GdkRectangle rect;
    rect.width = 1;
    rect.height = 1;
    rect.x = pt_x;
    rect.y = pt_y;
    gtk_popover_set_pointing_to(GTK_POPOVER(priv->record_popover), &rect);
    gtk_widget_set_size_request(GTK_WIDGET(priv->record_popover), width, height);
#if GTK_CHECK_VERSION(3,22,0)
    gtk_popover_popdown(GTK_POPOVER(priv->record_popover));
#endif
    gtk_widget_show_all(priv->record_popover);
    gtk_widget_hide(priv->button_retry);
}

static gboolean
on_timer_duration_timeout(ChatView* view)
{
    g_return_val_if_fail(IS_CHAT_VIEW(view), G_SOURCE_REMOVE);
    auto* priv = CHAT_VIEW_GET_PRIVATE(view);
    priv->duration += 1;
    auto m = std::to_string(priv->duration / 60);
    if (m.length() == 1) {
        m = "0" + m;
    }
    auto s = std::to_string(priv->duration % 60);
    if (s.length() == 1) {
        s = "0" + s;
    }
    auto time_txt = m + ":" + s;
    gtk_label_set_text(GTK_LABEL(priv->label_time), time_txt.c_str());
    return G_SOURCE_CONTINUE;
}

static bool
start_recorder(ChatView* self)
{
    g_return_val_if_fail(IS_CHAT_VIEW(self), false);
    auto* priv = CHAT_VIEW_GET_PRIVATE(self);
    QString file_name = priv->cpp->avModel_->startLocalRecorder(!priv->is_video_record);
    if (file_name.isEmpty()) {
        priv->startRecorderWhenReady = true;
        g_warning("set_state: failed to start recording, wait preview");
        return false;
    }

    if (!priv->cpp->saveFileName_.empty()) {
        std::remove(priv->cpp->saveFileName_.c_str());
    }
    priv->cpp->saveFileName_ = file_name.toStdString();
    return true;
}

static void
reset_recorder(ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    auto* priv = CHAT_VIEW_GET_PRIVATE(self);
    auto result = start_recorder(self);

    if (result) {
        gtk_widget_hide(GTK_WIDGET(priv->button_retry));
        std::string rsc = !priv->useDarkTheme && !priv->is_video_record ?
            "/net/jami/JamiGnome/stop" : "/net/jami/JamiGnome/stop-white";
        auto image = gtk_image_new_from_resource (rsc.c_str());
        gtk_button_set_image(GTK_BUTTON(priv->button_main_action), image);
        gtk_widget_set_tooltip_text(priv->button_main_action, _("Stop"));
        priv->cpp->current_action_ = RecordAction::STOP;
        gtk_label_set_text(GTK_LABEL(priv->label_time), "00:00");
        priv->duration = 0;
        priv->timer_duration = g_timeout_add(1000, (GSourceFunc)on_timer_duration_timeout, self);
    }

}

static void
webkit_chat_container_script_dialog(GtkWidget* webview, gchar *interaction, ChatView* self)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);
    auto order = std::string(interaction);
    if (!priv->conversation_) return;
    if (order == "ACCEPT") {
        (*priv->accountInfo_)->conversationModel->makePermanent(priv->conversation_->uid);
    } else if (order == "REFUSE") {
        (*priv->accountInfo_)->conversationModel->removeConversation(priv->conversation_->uid);
    } else if (order == "BLOCK") {
        (*priv->accountInfo_)->conversationModel->removeConversation(priv->conversation_->uid, true);
    } else if (order == "UNBLOCK") {
        try {
            auto contactUri = priv->conversation_->participants.front();
            auto& contact = (*priv->accountInfo_)->contactModel->getContact(contactUri);
            (*priv->accountInfo_)->contactModel->addContact(contact);
        } catch (std::out_of_range&) {
            g_debug("webkit_chat_container_script_dialog: oor while retrieving invalid contact info. Chatview bug ?");
        }
    } else if (order.find("PLACE_CALL") == 0) {
        placecall_clicked(self);
    } else if (order.find("PLACE_AUDIO_CALL") == 0) {
        place_audio_call_clicked(self);
    } else if (order.find("CLOSE_CHATVIEW") == 0) {
        hide_chat_view(webview, self);
    } else if (order.find("SEND:") == 0) {
        auto toSend = order.substr(std::string("SEND:").size());
        if ((*priv->accountInfo_)->profileInfo.type == lrc::api::profile::Type::RING) {
            (*priv->accountInfo_)->conversationModel->sendMessage(priv->conversation_->uid, toSend.c_str());
        } else {
            // For SIP accounts, we need to wait that the conversation is created to send text
            send_text_clicked(self, toSend);
        }
    } else if (order.find("SEND_FILE") == 0) {
        if (auto model = (*priv->accountInfo_)->conversationModel.get()) {
            if (auto filename = file_to_manipulate(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(self))), true))
                model->sendFile(priv->conversation_->uid, filename, g_path_get_basename(filename));
        }
    } else if (order.find("ACCEPT_FILE:") == 0) {
        if (auto model = (*priv->accountInfo_)->conversationModel.get()) {
            try {
                auto interactionId = std::stoull(order.substr(std::string("ACCEPT_FILE:").size()));

                lrc::api::datatransfer::Info info = {};
                (*priv->accountInfo_)->conversationModel->getTransferInfo(interactionId, info);

                // get preferred directory destination.
                auto* download_directory_variant = g_settings_get_value(priv->settings, "download-folder");
                char* download_directory_value;
                g_variant_get(download_directory_variant, "&s", &download_directory_value);
                std::string default_download_dir = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD);
                auto current_value = std::string(download_directory_value);
                if (current_value.empty()) {
                    g_settings_set_value(priv->settings, "download-folder", g_variant_new("s", default_download_dir.c_str()));
                }
                // get full path
                std::string download_dir = current_value.empty()? default_download_dir.c_str() : download_directory_value;
                if (!download_dir.empty() && download_dir.back() != '/') download_dir += "/";
                auto file_displayname = info.displayName.toStdString();
                auto wantedFilename = file_displayname;
                auto duplicate = 0;
                while (std::ifstream(download_dir + wantedFilename).good()) {
                    ++duplicate;
                    auto extensionIdx = file_displayname.find_last_of(".");
                    if (extensionIdx == std::string::npos)
                        wantedFilename = file_displayname + " (" + std::to_string(duplicate) + ")";
                    else
                        wantedFilename = file_displayname.substr(0, extensionIdx) + " (" + std::to_string(duplicate) + ")" + file_displayname.substr(extensionIdx);
                }
                model->acceptTransfer(priv->conversation_->uid, interactionId, wantedFilename.c_str());
            } catch (...) {
                // ignore
            }
        }
    } else if (order.find("REFUSE_FILE:") == 0) {
        if (auto model = (*priv->accountInfo_)->conversationModel.get()) {
            try {
                auto interactionId = std::stoull(order.substr(std::string("REFUSE_FILE:").size()));
                model->cancelTransfer(priv->conversation_->uid, interactionId);
            } catch (...) {
                // ignore
            }
        }
    } else if (order.find("OPEN_FILE:") == 0) {
        // Get text body
        auto filename {"file://" + order.substr(std::string("OPEN_FILE:").size())};
        filename.erase(std::find_if(filename.rbegin(), filename.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), filename.end());
        GError* error = nullptr;
        if (!gtk_show_uri(nullptr, filename.c_str(), GDK_CURRENT_TIME, &error)) {
            g_debug("Could not open file: %s", error->message);
            g_error_free(error);
        }
    } else if (order.find("ADD_TO_CONVERSATIONS") == 0) {
        add_to_conversations_clicked(self);
    } else if (order.find("DELETE_INTERACTION:") == 0) {
        try {
            auto interactionId = std::stoull(order.substr(std::string("DELETE_INTERACTION:").size()));
            if (!priv->conversation_) return;
            (*priv->accountInfo_)->conversationModel->clearInteractionFromConversation(priv->conversation_->uid, interactionId);
        } catch (...) {
            g_warning("delete interaction failed: can't find %s", order.substr(std::string("DELETE_INTERACTION:").size()).c_str());
        }
    } else if (order.find("RETRY_INTERACTION:") == 0) {
        try {
            auto interactionId = std::stoull(order.substr(std::string("RETRY_INTERACTION:").size()));
            if (!priv->conversation_) return;
            (*priv->accountInfo_)->conversationModel->retryInteraction(priv->conversation_->uid, interactionId);
        } catch (...) {
            g_warning("delete interaction failed: can't find %s", order.substr(std::string("RETRY_INTERACTION:").size()).c_str());
        }
    } else if (order.find("VIDEO_RECORD:") == 0) {
        auto pos_str {order.substr(std::string("VIDEO_RECORD:").size())};
        auto sep_idx = pos_str.find("x");
        if (sep_idx == std::string::npos)
            return;
        try {
            int pt_x = stoi(pos_str.substr(0, sep_idx));
            int pt_y = stoi(pos_str.substr(sep_idx + 1));
            chat_view_show_recorder(self, pt_x, pt_y, true);
        } catch (...) {
            // ignore
        }
    } else if (order.find("AUDIO_RECORD:") == 0) {
        auto pos_str {order.substr(std::string("AUDIO_RECORD:").size())};
        auto sep_idx = pos_str.find("x");
        if (sep_idx == std::string::npos)
            return;
        try {
            int pt_x = stoi(pos_str.substr(0, sep_idx));
            int pt_y = stoi(pos_str.substr(sep_idx + 1));
            chat_view_show_recorder(self, pt_x, pt_y, false);
        } catch (...) {
            // ignore
        }
    } else if (order.find("ON_COMPOSING:") == 0) {
        auto composing = order.substr(std::string("ON_COMPOSING:").size()) == "true";
        if (!priv->conversation_) return;
        if (g_settings_get_boolean(priv->settings, "enable-typing-indication")) {
            (*priv->accountInfo_)->conversationModel->setIsComposing(priv->conversation_->uid, composing);
        }
    }
}

static void
chat_view_init(ChatView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(view);
    priv->settings = g_settings_new_full(get_settings_schema(), NULL, NULL);
}

static void
chat_view_class_init(ChatViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = chat_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/net/jami/JamiGnome/chatview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, box_webkit_chat_container);

    chat_view_signals[NEW_MESSAGES_DISPLAYED] = g_signal_new (
        "new-interactions-displayed",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    chat_view_signals[HIDE_VIEW_CLICKED] = g_signal_new (
        "hide-view-clicked",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    chat_view_signals[PLACE_CALL_CLICKED] = g_signal_new (
        "place-call-clicked",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1, G_TYPE_STRING);

    chat_view_signals[PLACE_AUDIO_CALL_CLICKED] = g_signal_new (
        "place-audio-call-clicked",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1, G_TYPE_STRING);

    chat_view_signals[ADD_CONVERSATION_CLICKED] = g_signal_new (
        "add-conversation-clicked",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1, G_TYPE_STRING);

    chat_view_signals[SEND_TEXT_CLICKED] = g_signal_new (
        "send-text-clicked",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_user_marshal_VOID__STRING_STRING,
        G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
}

static void
print_interaction_to_buffer(ChatView* self, uint64_t interactionId, const lrc::api::interaction::Info& interaction)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    if (!priv->conversation_) return;
    if (!interaction.isRead)
        (*priv->accountInfo_)->conversationModel->setInteractionRead(priv->conversation_->uid, interactionId);

    webkit_chat_container_print_new_interaction(
        WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
        *(*priv->accountInfo_)->conversationModel,
        interactionId,
        interaction
    );
}

static void
update_interaction(ChatView* self, uint64_t interactionId, const lrc::api::interaction::Info& interaction)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);
    webkit_chat_container_update_interaction(
        WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
        *(*priv->accountInfo_)->conversationModel,
        interactionId,
        interaction
    );
}

static void
remove_interaction(ChatView* self, uint64_t interactionId)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);
    webkit_chat_container_remove_interaction(
        WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
        interactionId
    );
}

static void
load_participants_images(ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    // Contact
    if (!priv->conversation_) return;
    auto contactUri = priv->conversation_->participants.front();
    try{
        auto& contact = (*priv->accountInfo_)->contactModel->getContact(contactUri);
        std::string avatar_str = contact.profileInfo.avatar.toStdString();
        if (avatar_str.empty()) {
            auto var_photo = Interfaces::PixbufManipulator().conversationPhoto(
                *priv->conversation_,
                **(priv->accountInfo_),
                QSize(AVATAR_WIDTH, AVATAR_HEIGHT),
                contact.isPresent
            );
            auto image = var_photo.value<std::shared_ptr<GdkPixbuf>>();
            auto ba = Interfaces::PixbufManipulator().toByteArray(var_photo);
            avatar_str = ba.toBase64().toStdString();
        }
        webkit_chat_container_set_sender_image(
            WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
            contactUri.toStdString(),
            avatar_str
        );
    } catch (const std::out_of_range&) {
        // ContactModel::getContact() exception
    }

    // For this account
    std::string avatar_str = (*priv->accountInfo_)->profileInfo.avatar.toStdString();
    if (avatar_str.empty()) {
        auto default_photo = QVariant::fromValue(Interfaces::PixbufManipulator().scaleAndFrame(
            Interfaces::PixbufManipulator().generateAvatar("", "").get(),
            QSize(AVATAR_WIDTH, AVATAR_HEIGHT), false));
        auto ba = Interfaces::PixbufManipulator().toByteArray(default_photo);
        avatar_str = ba.toBase64().toStdString();
    }
    webkit_chat_container_set_sender_image(
        WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
        (*priv->accountInfo_)->profileInfo.uri.toStdString(),
        avatar_str
    );
}

static void
print_text_recording(ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    // Read interactions
    if (!priv->conversation_) return;
    (*priv->accountInfo_)->conversationModel->clearUnreadInteractions(priv->conversation_->uid);

    webkit_chat_container_print_history(
        WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
        *(*priv->accountInfo_)->conversationModel,
        priv->conversation_->interactions
    );
}

static void
webkit_chat_container_ready(ChatView* self)
{
    /* The webkit chat container has loaded the javascript libraries, we can
     * now use it. */

    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    webkit_chat_container_clear(
        WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container)
    );

    display_links_toggled(self);
    print_text_recording(self);
    load_participants_images(self);
    webkit_chat_set_dark_mode(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container), priv->useDarkTheme, priv->background);

    priv->ready_ = true;
    for (const auto& interaction: priv->cpp->interactionsBuffer_) {
        if (interaction.conv == priv->conversation_->uid.toStdString()) {
            print_interaction_to_buffer(self, interaction.id, interaction.info);
        }
    }

    priv->update_interaction_connection = QObject::connect(
    &*(*priv->accountInfo_)->conversationModel, &lrc::api::ConversationModel::interactionStatusUpdated,
    [self, priv](const QString& uid, uint64_t msgId, lrc::api::interaction::Info msg) {
        if (!priv->conversation_) return;
        if (uid == priv->conversation_->uid) {
            update_interaction(self, msgId, msg);
        }
    });

    priv->interaction_removed = QObject::connect(
    &*(*priv->accountInfo_)->conversationModel, &lrc::api::ConversationModel::interactionRemoved,
    [self, priv](const QString& convUid, uint64_t interactionId) {
        if (!priv->conversation_) return;
        if (convUid == priv->conversation_->uid) {
            remove_interaction(self, interactionId);
        }
    });

    if (!priv->conversation_) return;
    auto contactUri = priv->conversation_->participants.front();
    try {
        auto contactInfo = (*priv->accountInfo_)->contactModel->getContact(contactUri);
        auto bestName = contactInfo.profileInfo.alias;
        if (bestName.isEmpty())
            bestName = contactInfo.registeredName;
        if (bestName.isEmpty())
            bestName = contactInfo.profileInfo.uri;
        bestName.remove('\r');
        bestName.remove('\n');
    } catch (const std::out_of_range&) {
        // ContactModel::getContact() exception
    }

    update_chatview_frame(self);
}

static void
update_chatview_frame(ChatView* self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);
    if (!priv->conversation_) return;

    auto contactUri = priv->conversation_->participants.front();

    lrc::api::contact::Info contactInfo;
    try {
        contactInfo = (*priv->accountInfo_)->contactModel->getContact(contactUri);
    } catch (const std::out_of_range&) {
        g_debug("update_chatview_frame: failed to retrieve contactInfo");
        return;
    }

    // get alias and bestName
    auto alias = contactInfo.profileInfo.alias;
    auto bestName = contactInfo.registeredName;
    if (bestName.isEmpty())
        bestName = contactInfo.profileInfo.uri;
    if (bestName == alias)
        alias = "";
    bestName.remove('\r');
    alias.remove('\r');
    // get temporary status
    bool temp = contactInfo.profileInfo.type == lrc::api::profile::Type::TEMPORARY || contactInfo.profileInfo.type == lrc::api::profile::Type::PENDING;

    webkit_chat_update_chatview_frame(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
                                     (*priv->accountInfo_)->enabled,
                                     contactInfo.isBanned, temp, qUtf8Printable(alias), qUtf8Printable(bestName));

    webkit_chat_container_set_invitation(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
                                             (contactInfo.profileInfo.type == lrc::api::profile::Type::PENDING),
                                             bestName.toStdString(),
                                             contactInfo.profileInfo.uri.toStdString());

    // hide navbar if we are in call
    try {
        QString callId;
        if (priv->conversation_->confId.isEmpty()) {
            callId = priv->conversation_->callId;
        } else {
            callId = priv->conversation_->confId;
        }

        if (*priv->accountInfo_) {
            const lrc::api::call::Status& status = (*priv->accountInfo_)->callModel->getCall(callId).status;
            if (status != lrc::api::call::Status::ENDED &&
                status != lrc::api::call::Status::INVALID &&
                status != lrc::api::call::Status::TERMINATING) {
                g_debug("call has status %s, hiding", qUtf8Printable(lrc::api::call::to_string(status)));
                chat_view_set_header_visible(self, FALSE);
            } else {
                chat_view_set_header_visible(self, TRUE);
            }
        }
    } catch (const std::out_of_range&) {}
    chat_view_set_record_visible(self, lrc::api::Lrc::activeCalls().size() == 0);
}

static void
on_webkit_drag_drop(GtkWidget*, gchar* data, ChatView* self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);
    if (!priv->conversation_) return;
    if (!data) return;

    foreach_file(data, [&](const char* file) {
        if (auto model = (*priv->accountInfo_)->conversationModel.get()) {
            model->sendFile(priv->conversation_->uid, file,
                            g_path_get_basename(file));
        }
    });
}

static void
on_main_action_clicked(ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    auto* priv = CHAT_VIEW_GET_PRIVATE(self);

    switch (priv->cpp->current_action_) {
        case RecordAction::RECORD: {
            reset_recorder(self);
            break;
        }
        case RecordAction::STOP: {
            if (!priv->cpp->saveFileName_.empty()) {
                priv->cpp->avModel_->stopLocalRecorder(priv->cpp->saveFileName_.c_str());
            }
            gtk_widget_show(GTK_WIDGET(priv->button_retry));
            std::string rsc = !priv->useDarkTheme && !priv->is_video_record ?
                "/net/jami/JamiGnome/send" : "/net/jami/JamiGnome/send-white";
            auto image = gtk_image_new_from_resource (rsc.c_str());
            gtk_widget_set_tooltip_text(priv->button_main_action, _("Send"));
            gtk_button_set_image(GTK_BUTTON(priv->button_main_action), image);
            priv->cpp->current_action_ = RecordAction::SEND;
            g_source_remove(priv->timer_duration);
            break;
        }
        case RecordAction::SEND: {
            if (auto model = (*priv->accountInfo_)->conversationModel.get()) {
                model->sendFile(priv->conversation_->uid,
                                priv->cpp->saveFileName_.c_str(),
                                g_path_get_basename(priv->cpp->saveFileName_.c_str()));
                priv->cpp->saveFileName_ = "";
            }
            gtk_widget_destroy(priv->record_popover);
            priv->cpp->current_action_ = RecordAction::RECORD;
            priv->cpp->avModel_->stopPreview();
            QObject::disconnect(priv->local_renderer_connection);
            break;
        }
    }
}

static void
init_video_widget(ChatView* self)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);
    if (priv->video_widget && GTK_IS_WIDGET(priv->video_widget)) gtk_widget_destroy(priv->video_widget);
    priv->startRecorderWhenReady = false;
    priv->video_widget = video_widget_new();

    const lrc::api::video::Renderer* prenderer = nullptr;
    try {
        prenderer = &priv->cpp->avModel_->getRenderer(
            lrc::api::video::PREVIEW_RENDERER_ID);
    } catch (const std::out_of_range& e) {}

    priv->video_started_by_settings =
        prenderer && prenderer->isRendering();
    if (priv->video_started_by_settings) {
        video_widget_add_new_renderer(
            VIDEO_WIDGET(priv->video_widget),
            priv->cpp->avModel_, prenderer,
            VIDEO_RENDERER_REMOTE);
    } else {
        priv->video_started_by_settings = true;
        QObject::disconnect(priv->local_renderer_connection);
        priv->local_renderer_connection = QObject::connect(
            &*priv->cpp->avModel_,
            &lrc::api::AVModel::rendererStarted,
            [=](const QString& id) {
                if (id != lrc::api::video::PREVIEW_RENDERER_ID
                    || !priv->readyToRecord_)
                    return;
                try {
                    const auto* prenderer =
                        &priv->cpp->avModel_->getRenderer(id);
                    video_widget_add_new_renderer(
                        VIDEO_WIDGET(priv->video_widget),
                        priv->cpp->avModel_, prenderer,
                        VIDEO_RENDERER_REMOTE);
                } catch (const std::out_of_range& e) {
                    g_warning("Cannot start preview");
                }
            });
        // Start recorder
        if (priv->startRecorderWhenReady)
            reset_recorder(self);
    }

    auto stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(priv->video_widget));

    auto* hbox_record_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    init_control_box_buttons(self, hbox_record_controls);

    priv->label_time = gtk_label_new("00:00");
    auto context = gtk_widget_get_style_context(GTK_WIDGET(priv->label_time));
    gtk_style_context_add_class(context, "label_time");
    gtk_container_add(GTK_CONTAINER(hbox_record_controls), priv->label_time);

    gtk_widget_show_all(hbox_record_controls);
    gtk_widget_hide(priv->button_retry);

    auto actor_controls = gtk_clutter_actor_new_with_contents(hbox_record_controls);
    clutter_actor_add_child(stage, actor_controls);
    clutter_actor_set_x_align(actor_controls, CLUTTER_ACTOR_ALIGN_CENTER);
    clutter_actor_set_y_align(actor_controls, CLUTTER_ACTOR_ALIGN_END);
}

static void
build_chat_view(ChatView* self)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    gtk_container_add(GTK_CONTAINER(priv->box_webkit_chat_container), priv->webkit_chat_container);
    gtk_widget_show(priv->webkit_chat_container);

    update_chatview_frame(self);

    priv->webkit_ready = g_signal_connect_swapped(
        priv->webkit_chat_container,
        "ready",
        G_CALLBACK(webkit_chat_container_ready),
        self
    );

    priv->webkit_send_text = g_signal_connect(priv->webkit_chat_container,
        "script-dialog",
        G_CALLBACK(webkit_chat_container_script_dialog),
        self);

    priv->webkit_drag_drop = g_signal_connect(
        priv->webkit_chat_container,
        "data-dropped",
        G_CALLBACK(on_webkit_drag_drop),
        self
    );

    g_signal_connect(
        self,
        "draw",
        G_CALLBACK(on_redraw),
        self
    );

    priv->new_interaction_connection = QObject::connect(
    &*(*priv->accountInfo_)->conversationModel, &lrc::api::ConversationModel::newInteraction,
    [self, priv](const QString& uid, uint64_t interactionId, lrc::api::interaction::Info interaction) {
        if (!priv->conversation_) return;
        if (!priv->ready_ && priv->cpp) {
            priv->cpp->interactionsBuffer_.emplace_back(CppImpl::Interaction {
                uid.toStdString(), interactionId, interaction});
        } else if (uid == priv->conversation_->uid) {
            print_interaction_to_buffer(self, interactionId, interaction);
        }
    });

    priv->composing_changed_connection = QObject::connect(
    &*(*priv->accountInfo_)->conversationModel, &lrc::api::ConversationModel::composingStatusChanged,
    [self, priv](const QString& uid, const QString& contactUri, bool isComposing) {
        if (!g_settings_get_boolean(priv->settings, "enable-typing-indication")) return;
        if (!priv->conversation_) return;
        if (uid == priv->conversation_->uid) {
            webkit_chat_set_is_composing(
                WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
                contactUri.toStdString(), isComposing);
        }
    });

    priv->cpp = new CppImpl();

    if (webkit_chat_container_is_ready(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container)))
        webkit_chat_container_ready(self);
}

GtkWidget *
chat_view_new (WebKitChatContainer* webkit_chat_container,
               AccountInfoPointer const & accountInfo,
               lrc::api::conversation::Info& conversation,
               lrc::api::AVModel& avModel)
{
    ChatView *self = CHAT_VIEW(g_object_new(CHAT_VIEW_TYPE, NULL));

    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);
    priv->webkit_chat_container = GTK_WIDGET(webkit_chat_container);
    priv->conversation_ = &conversation;
    priv->accountInfo_ = &accountInfo;

    build_chat_view(self);
    priv->cpp->avModel_ = &avModel;
    priv->readyToRecord_ = true;
    return (GtkWidget *)self;
}

void
chat_view_update_temporary(ChatView* self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    update_chatview_frame(self);
}

lrc::api::conversation::Info&
chat_view_get_conversation(ChatView *self)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);
    return *priv->conversation_;
}

void
chat_view_set_header_visible(ChatView *self, gboolean visible)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);
    webkit_chat_set_header_visible(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container), visible);
}

void
chat_view_set_record_visible(ChatView *self, gboolean visible)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);
    webkit_chat_set_record_visible(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container), visible);
}
