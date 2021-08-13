/*
 *  Copyright (C) 2015-2021 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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

#include "incomingcallview.h"

/* std */
#include <stdexcept>

// GTK+ related
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

// Qt
#include <QSize>

// Lrc
#include <api/avmodel.h>
#include <api/newcallmodel.h>
#include <api/contactmodel.h>
#include <api/conversationmodel.h>
#include <api/contact.h>

// Client
#include "chatview.h"
#include "marshals.h"
#include "messagingwidget.h"
#include "utils/drawing.h"
#include "utils/files.h"

struct _IncomingCallView
{
    GtkBox parent;
};

struct _IncomingCallViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _IncomingCallViewPrivate IncomingCallViewPrivate;

struct _IncomingCallViewPrivate
{
    GtkWidget *paned_call;
    GtkWidget *image_incoming;
    GtkWidget *label_name;
    GtkWidget *label_bestId;
    GtkWidget *spinner_status;
    GtkWidget *label_status;
    GtkWidget *button_accept_incoming;
    GtkWidget *button_reject_incoming;
    GtkWidget *frame_chat;
    GtkWidget *box_messaging_widget;
    GtkWidget *messaging_widget;
    bool showMessaging {false};

    // The webkit_chat_container is created once, then reused for all chat views
    GtkWidget *webkit_chat_container;

    lrc::api::conversation::Info* conversation_;
    AccountInfoPointer const * accountInfo_;

    QMetaObject::Connection state_change_connection;

    GSettings *settings;

    lrc::api::AVModel* avModel_;
    lrc::api::PluginModel* pluginModel_;
};

G_DEFINE_TYPE_WITH_PRIVATE(IncomingCallView, incoming_call_view, GTK_TYPE_BOX);

#define INCOMING_CALL_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), INCOMING_CALL_VIEW_TYPE, IncomingCallViewPrivate))

/* signals */
enum {
    CALL_HUNGUP,
    LAST_SIGNAL
};

static guint incoming_call_view_signals[LAST_SIGNAL] = { 0 };

static void
incoming_call_view_dispose(GObject *object)
{
    IncomingCallView *view;
    IncomingCallViewPrivate *priv;

    view = INCOMING_CALL_VIEW(object);
    priv = INCOMING_CALL_VIEW_GET_PRIVATE(view);

    // navbar was hidden during init, we need to make it visible again before view destruction
    auto children = gtk_container_get_children(GTK_CONTAINER(priv->frame_chat));
    auto chat_view = children->data;
    chat_view_set_header_visible(CHAT_VIEW(chat_view), TRUE);

    QObject::disconnect(priv->state_change_connection);

    g_clear_object(&priv->settings);

    G_OBJECT_CLASS(incoming_call_view_parent_class)->dispose(object);
}

static gboolean
map_boolean_to_orientation(GValue *value, GVariant *variant, G_GNUC_UNUSED gpointer user_data)
{
    bool ret = FALSE;
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_BOOLEAN)) {
        if (g_variant_get_boolean(variant)) {
            // true, chat should be horizontal (to the right)
            g_value_set_enum(value, GTK_ORIENTATION_HORIZONTAL);
        } else {
            // false, chat should be vertical (at the bottom)
            g_value_set_enum(value, GTK_ORIENTATION_VERTICAL);
        }
        ret = TRUE;
    }
    return ret;
}

static void
reject_incoming_call(IncomingCallView *self)
{
    g_return_if_fail(IS_INCOMING_CALL_VIEW(self));
    auto priv = INCOMING_CALL_VIEW_GET_PRIVATE(self);
    g_signal_emit(G_OBJECT(self), incoming_call_view_signals[CALL_HUNGUP], 0,
                  (*priv->accountInfo_)->id.toStdString().c_str(),
                  priv->conversation_->callId.toStdString().c_str());
    (*priv->accountInfo_)->callModel->hangUp(priv->conversation_->callId);
}

static void
accept_incoming_call(IncomingCallView *self)
{
    g_return_if_fail(IS_INCOMING_CALL_VIEW(self));
    auto priv = INCOMING_CALL_VIEW_GET_PRIVATE(self);

    if (priv->conversation_->uid.isEmpty())
        return;
    (*priv->accountInfo_)->callModel->accept(priv->conversation_->callId);
    if (priv->conversation_->isRequest)
        (*priv->accountInfo_)->conversationModel->makePermanent(priv->conversation_->uid);
}

static void
on_leave_action(IncomingCallView *view)
{
    g_return_if_fail(IS_INCOMING_CALL_VIEW(view));
    auto priv = INCOMING_CALL_VIEW_GET_PRIVATE(view);
    priv->showMessaging = false;
    (*priv->accountInfo_)->conversationModel->selectConversation(priv->conversation_->uid);
}

static void
incoming_call_view_init(IncomingCallView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    auto provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        ".flat-button { border: 0; border-radius: 50%; transition: all 0.3s ease; } \
        .red-button { background: #dc3a37; } \
        .green-button { background: #27ae60; } \
        .red-button:hover { background: #dc2719; } \
        .green-button:hover { background: #219d55; }",
        -1, nullptr
    );

    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    auto priv = INCOMING_CALL_VIEW_GET_PRIVATE(view);

    /* bind the chat orientation to the gsetting */
    priv->settings = g_settings_new_full(get_settings_schema(), NULL, NULL);
    g_settings_bind_with_mapping(priv->settings, "chat-pane-horizontal",
                                 priv->paned_call, "orientation",
                                 G_SETTINGS_BIND_GET,
                                 map_boolean_to_orientation,
                                 nullptr, nullptr, nullptr);

    g_signal_connect_swapped(priv->button_reject_incoming, "clicked", G_CALLBACK(reject_incoming_call), view);
    g_signal_connect_swapped(priv->button_accept_incoming, "clicked", G_CALLBACK(accept_incoming_call), view);
}

static void
incoming_call_view_class_init(IncomingCallViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = incoming_call_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/net/jami/JamiGnome/incomingcallview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, paned_call);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, image_incoming);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, label_name);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, label_bestId);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, spinner_status);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, label_status);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, button_reject_incoming);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, button_accept_incoming);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, frame_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, box_messaging_widget);

    /* add signals */
    incoming_call_view_signals[CALL_HUNGUP] = g_signal_new("call-hungup",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0, nullptr, nullptr,
        g_cclosure_user_marshal_VOID__STRING_STRING, G_TYPE_NONE,
        2, G_TYPE_STRING, G_TYPE_STRING);
}

static void
update_state(IncomingCallView *view)
{
    g_return_if_fail(IS_INCOMING_CALL_VIEW(view));
    IncomingCallViewPrivate *priv = INCOMING_CALL_VIEW_GET_PRIVATE(view);

    // change state label
    auto callId = priv->conversation_->callId;
    if (!(*priv->accountInfo_)->callModel->hasCall(callId)) return;
    auto call = (*priv->accountInfo_)->callModel->getCall(callId);

    gtk_label_set_text(GTK_LABEL(priv->label_status), qUtf8Printable(lrc::api::call::to_string(call.status)));

    if (call.status == lrc::api::call::Status::INCOMING_RINGING)
        gtk_widget_show(priv->button_accept_incoming);
    else
        gtk_widget_hide(priv->button_accept_incoming);

    if (call.status != lrc::api::call::Status::PEER_BUSY &&
             call.status != lrc::api::call::Status::ENDED) {
        gtk_widget_hide(priv->messaging_widget);
        gtk_widget_show(priv->label_status);
        gtk_widget_show(priv->spinner_status);
        gtk_widget_show(priv->button_reject_incoming);
        gtk_widget_show(priv->spinner_status);
    }
}

static void
update_name_and_photo(IncomingCallView *view)
{
    g_return_if_fail(IS_INCOMING_CALL_VIEW(view));
    auto priv = INCOMING_CALL_VIEW_GET_PRIVATE(view);

    GdkPixbuf *p = draw_conversation_photo(
        *priv->conversation_,
        **(priv->accountInfo_),
        QSize(110, 110),
        false
    );
    gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_incoming), p);
    g_object_unref(p);

    try {
        auto contacts = (*priv->accountInfo_)->conversationModel->peersForConversation(priv->conversation_->uid);
        if (contacts.empty()) return;
        auto contactInfo = (*priv->accountInfo_)->contactModel->getContact(contacts.front());

        auto name = contactInfo.profileInfo.alias;
        gtk_label_set_text(GTK_LABEL(priv->label_name), qUtf8Printable(name));

        auto bestId = contactInfo.registeredName;
        if (name != bestId) {
            gtk_label_set_text(GTK_LABEL(priv->label_bestId), qUtf8Printable(bestId));
            gtk_widget_show(priv->label_bestId);
        }
    } catch (const std::out_of_range&) {
        // ContactModel::getContact() exception
    }
}

static void
set_call_info(IncomingCallView *view) {
    g_return_if_fail(IS_INCOMING_CALL_VIEW(view));
    IncomingCallViewPrivate *priv = INCOMING_CALL_VIEW_GET_PRIVATE(view);

    update_state(view);
    update_name_and_photo(view);

    // Update view if call state changes
    priv->state_change_connection = QObject::connect(
    &*(*priv->accountInfo_)->callModel,
    &lrc::api::NewCallModel::callStatusChanged,
    [view, priv] (const QString& callId) {
        if (callId == priv->conversation_->callId) {
            update_state(view);
            update_name_and_photo(view);
        }
    });

    auto chat_view = chat_view_new(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
                                                         *priv->accountInfo_,
                                                         *priv->conversation_,
                                                         *priv->avModel_,
                                                         *priv->pluginModel_);
    gtk_widget_show(chat_view);
    chat_view_set_header_visible(CHAT_VIEW(chat_view), FALSE);
    chat_view_set_record_visible(CHAT_VIEW(chat_view), FALSE);
    chat_view_set_plugin_visible(CHAT_VIEW(chat_view), FALSE);
    gtk_container_add(GTK_CONTAINER(priv->frame_chat), chat_view);
}

GtkWidget *
incoming_call_view_new(WebKitChatContainer* view,
                       lrc::api::AVModel& avModel,
                       lrc::api::PluginModel& pluginModel,
                       AccountInfoPointer const & accountInfo,
                       lrc::api::conversation::Info& conversation)
{
    auto self = g_object_new(INCOMING_CALL_VIEW_TYPE, NULL);

    IncomingCallViewPrivate *priv = INCOMING_CALL_VIEW_GET_PRIVATE(self);
    priv->webkit_chat_container = GTK_WIDGET(view);
    priv->conversation_ = &conversation;
    priv->accountInfo_ = &accountInfo;
    priv->avModel_ = &avModel;
    priv->pluginModel_ = &pluginModel;

    priv->messaging_widget = messaging_widget_new(avModel, conversation, accountInfo);
    gtk_box_pack_start(GTK_BOX(priv->box_messaging_widget), priv->messaging_widget, TRUE, TRUE, 0);
    g_signal_connect_swapped(priv->messaging_widget, "leave-action", G_CALLBACK(on_leave_action), self);

    set_call_info(INCOMING_CALL_VIEW(self));

    return GTK_WIDGET(self);
}

lrc::api::conversation::Info&
incoming_call_view_get_conversation(IncomingCallView *self)
{
    auto priv = INCOMING_CALL_VIEW_GET_PRIVATE(self);

    return *priv->conversation_;
}

void
incoming_call_view_let_a_message(IncomingCallView* view, const lrc::api::conversation::Info& conv)
{
    g_return_if_fail(IS_INCOMING_CALL_VIEW(view));
    auto priv = INCOMING_CALL_VIEW_GET_PRIVATE(view);
    g_return_if_fail(priv->conversation_->uid == conv.uid);

    gtk_widget_hide(priv->label_status);
    gtk_widget_hide(priv->spinner_status);
    gtk_widget_hide(priv->button_accept_incoming);
    gtk_widget_hide(priv->button_reject_incoming);

    priv->showMessaging = true;
    gtk_widget_show(priv->messaging_widget);
}

bool
is_showing_let_a_message_view(IncomingCallView* view, lrc::api::conversation::Info& conv)
{
    g_return_val_if_fail(IS_INCOMING_CALL_VIEW(view), false);
    auto priv = INCOMING_CALL_VIEW_GET_PRIVATE(view);
    g_return_val_if_fail(priv->conversation_->uid == conv.uid, false);
    return priv->showMessaging;
}
