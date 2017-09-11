/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Alexandre Viau <alexandre.viau@savoirfairelinux.com>
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

// LRC
#include <api/contactmodel.h>
#include <api/conversationmodel.h>

// Client
#include "utils/files.h"

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
    GtkWidget *scrolledwindow_chat;
    GtkWidget *hbox_chat_info;
    GtkWidget *label_peer;
    GtkWidget *label_cm;
    GtkWidget *button_close_chatview;
    GtkWidget *button_placecall;
    GtkWidget *button_send_invitation;

    GSettings *settings;

    ConversationContainer* conversation_;
    AccountContainer* accountContainer_;
    bool isTemporary_;

    QMetaObject::Connection new_message_connection;
    QMetaObject::Connection message_changed_connection;
    QMetaObject::Connection update_name;
    QMetaObject::Connection update_send_invitation;

    gulong webkit_ready;
    gulong webkit_send_text;
};

G_DEFINE_TYPE_WITH_PRIVATE(ChatView, chat_view, GTK_TYPE_BOX);

#define CHAT_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CHAT_VIEW_TYPE, ChatViewPrivate))

enum {
    NEW_MESSAGES_DISPLAYED,
    HIDE_VIEW_CLICKED,
    LAST_SIGNAL
};

static guint chat_view_signals[LAST_SIGNAL] = { 0 };

static void
chat_view_dispose(GObject *object)
{
    ChatView *view;
    ChatViewPrivate *priv;

    view = CHAT_VIEW(object);
    priv = CHAT_VIEW_GET_PRIVATE(view);

    QObject::disconnect(priv->new_message_connection);
    QObject::disconnect(priv->message_changed_connection);
    QObject::disconnect(priv->update_name);
    QObject::disconnect(priv->update_send_invitation);

    /* Destroying the box will also destroy its children, and we wouldn't
     * want that. So we remove the webkit_chat_container from the box. */
    if (priv->webkit_chat_container) {
        /* disconnect for webkit signals */
        g_signal_handler_disconnect(priv->webkit_chat_container, priv->webkit_ready);
        priv->webkit_ready = 0;
        g_signal_handler_disconnect(priv->webkit_chat_container, priv->webkit_send_text);
        priv->webkit_send_text = 0;

        gtk_container_remove(
            GTK_CONTAINER(priv->box_webkit_chat_container),
            GTK_WIDGET(priv->webkit_chat_container)
        );
        priv->webkit_chat_container = nullptr;

    }

    G_OBJECT_CLASS(chat_view_parent_class)->dispose(object);
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
    priv->accountContainer_->info.conversationModel->placeCall(priv->conversation_->info.uid);
}

static void
button_send_invitation_clicked(ChatView *self)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);
    priv->accountContainer_->info.conversationModel->addConversation(priv->conversation_->info.uid);
}

static void
webkit_chat_container_script_dialog(G_GNUC_UNUSED GtkWidget* webview, gchar *message, ChatView* self)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);
    auto order = std::string(message);
    if (order == "ACCEPT") {
        priv->accountContainer_->info.conversationModel->addConversation(priv->conversation_->info.uid);
    } else if (order == "REFUSE") {
        priv->accountContainer_->info.conversationModel->removeConversation(priv->conversation_->info.uid);
    } else if (order == "BLOCK") {
        priv->accountContainer_->info.conversationModel->removeConversation(priv->conversation_->info.uid, true);
    } else if (order.find("SEND:") == 0) {
        auto toSend = order.substr(std::string("SEND:").size());
        priv->accountContainer_->info.conversationModel->sendMessage(priv->conversation_->info.uid, toSend);
    }
}

static void
chat_view_init(ChatView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(view);
    priv->settings = g_settings_new_full(get_ring_schema(), NULL, NULL);

    g_signal_connect(priv->button_close_chatview, "clicked", G_CALLBACK(hide_chat_view), view);
    g_signal_connect_swapped(priv->button_placecall, "clicked", G_CALLBACK(placecall_clicked), view);
    g_signal_connect_swapped(priv->button_send_invitation, "clicked", G_CALLBACK(button_send_invitation_clicked), view);
}

static void
chat_view_class_init(ChatViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = chat_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/chatview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, box_webkit_chat_container);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, scrolledwindow_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, hbox_chat_info);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, label_peer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, label_cm);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, button_close_chatview);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, button_placecall);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, button_send_invitation);

    chat_view_signals[NEW_MESSAGES_DISPLAYED] = g_signal_new (
        "new-messages-displayed",
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
}

static void
print_message_to_buffer(ChatView* self, const lrc::api::message::Info& msg)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    webkit_chat_container_print_new_message(
        WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
        msg
    );
}

static void
set_participant_images(ChatView* self)
{
    // TODO
}

static void
print_text_recording(ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    for (const auto& msg : priv->conversation_->info.messages)
    {
        print_message_to_buffer(self, msg.second);
    }

    QObject::disconnect(priv->new_message_connection);
}

static void
update_send_invitation(ChatView *self)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);
    if(priv->conversation_->info.isUsed)
        gtk_widget_hide(priv->button_send_invitation);
}

static void
update_contact_methods(ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    gtk_widget_hide(priv->label_cm);
}

static void
update_name(ChatView *self)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);
    auto contactUri = priv->conversation_->info.participants.front();
    auto contact = priv->accountContainer_->info.contactModel->getContact(contactUri);
    gtk_label_set_text(GTK_LABEL(priv->label_peer), contact.uri.c_str());
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

    priv->new_message_connection = QObject::connect(
    &*priv->accountContainer_->info.conversationModel, &lrc::api::ConversationModel::newMessageAdded,
    [self, priv](const std::string& uid, lrc::api::message::Info msg) {
        if(uid == priv->conversation_->info.uid) {
            print_message_to_buffer(self, msg);
        }
    });

    priv->isTemporary_ = !priv->conversation_->info.isUsed;
    webkit_chat_container_set_temporary(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container), priv->isTemporary_);
    auto contactUri = priv->conversation_->info.participants.front();
    auto contact = priv->accountContainer_->info.contactModel->getContact(contactUri);
    webkit_chat_container_set_invitation(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
                                         (contact.type == lrc::api::contact::Type::PENDING),
                                         contactUri);
    webkit_chat_disable_send_message(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
                                     (contact.type == lrc::api::contact::Type::SIP));
}

static void
build_chat_view(ChatView* self)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    gtk_container_add(GTK_CONTAINER(priv->box_webkit_chat_container), priv->webkit_chat_container);
    gtk_widget_show(priv->webkit_chat_container);

    update_name(self);
    update_send_invitation(self);

    /* keep selected cm updated */
    update_contact_methods(self);

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

    if (webkit_chat_container_is_ready(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container)))
        webkit_chat_container_ready(self);

    gtk_widget_set_visible(priv->hbox_chat_info, TRUE);

}

GtkWidget *
chat_view_new (WebKitChatContainer* webkit_chat_container, AccountContainer* accountContainer,  ConversationContainer* conversationContainer)
{
    ChatView *self = CHAT_VIEW(g_object_new(CHAT_VIEW_TYPE, NULL));

    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);
    priv->webkit_chat_container = GTK_WIDGET(webkit_chat_container);
    priv->conversation_ = conversationContainer;
    priv->accountContainer_ = accountContainer;

    build_chat_view(self);

    return (GtkWidget *)self;
}

void
chat_view_update_temporary(ChatView* self, bool newValue)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    priv->isTemporary_ = newValue;
    if (!priv->isTemporary_) {
        gtk_widget_hide(priv->button_send_invitation);
    }
    webkit_chat_container_set_temporary(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container), priv->isTemporary_);
    auto contactUri = priv->conversation_->info.participants.front();
    auto contact = priv->accountContainer_->info.contactModel->getContact(contactUri);
    webkit_chat_container_set_invitation(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
                                         (contact.type == lrc::api::contact::Type::PENDING),
                                         contactUri);
}

bool
chat_view_get_temporary(ChatView *self)
{
    g_return_val_if_fail(IS_CHAT_VIEW(self), false);
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    return priv->isTemporary_;
}

lrc::api::conversation::Info
chat_view_get_conversation(ChatView *self)
{
    g_return_val_if_fail(IS_CHAT_VIEW(self), lrc::api::conversation::Info());
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    return priv->conversation_->info;
}

void
chat_view_set_header_visible(ChatView *self, gboolean visible)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    gtk_widget_set_visible(priv->hbox_chat_info, visible);
}
