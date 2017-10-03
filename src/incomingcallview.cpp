/*
 *  Copyright (C) 2015-2017 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

// Gtk
#include <gtk/gtk.h>
#include <QSize>

// Lrc
#include <api/newcallmodel.h>
#include <api/contactmodel.h>
#include <api/conversationmodel.h>
#include <api/contact.h>
#include <globalinstances.h>

// Client
#include "chatview.h"
#include "native/pixbufmanipulator.h"
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

    // The webkit_chat_container is created once, then reused for all chat views
    GtkWidget *webkit_chat_container;

    ConversationContainer* conversation_;
    AccountContainer* accountContainer_;

    QMetaObject::Connection state_change_connection;

    GSettings *settings;
};

G_DEFINE_TYPE_WITH_PRIVATE(IncomingCallView, incoming_call_view, GTK_TYPE_BOX);

#define INCOMING_CALL_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), INCOMING_CALL_VIEW_TYPE, IncomingCallViewPrivate))

static void
incoming_call_view_dispose(GObject *object)
{
    IncomingCallView *view;
    IncomingCallViewPrivate *priv;

    view = INCOMING_CALL_VIEW(object);
    priv = INCOMING_CALL_VIEW_GET_PRIVATE(view);

    QObject::disconnect(priv->state_change_connection);

    g_clear_object(&priv->settings);

    G_OBJECT_CLASS(incoming_call_view_parent_class)->dispose(object);
}

static gboolean
map_boolean_to_orientation(GValue *value, GVariant *variant, G_GNUC_UNUSED gpointer user_data)
{
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_BOOLEAN)) {
        if (g_variant_get_boolean(variant)) {
            // true, chat should be horizontal (to the right)
            g_value_set_enum(value, GTK_ORIENTATION_HORIZONTAL);
        } else {
            // false, chat should be vertical (at the bottom)
            g_value_set_enum(value, GTK_ORIENTATION_VERTICAL);
        }
        return TRUE;
    }
    return FALSE;
}

static void
reject_incoming_call(G_GNUC_UNUSED GtkWidget *widget, ChatView *self)
{
    auto priv = INCOMING_CALL_VIEW_GET_PRIVATE(self);
    priv->accountContainer_->info.callModel->hangUp(priv->conversation_->info.callId);
}

static void
accept_incoming_call(G_GNUC_UNUSED GtkWidget *widget, ChatView *self)
{
    auto priv = INCOMING_CALL_VIEW_GET_PRIVATE(self);
    auto contactUri = priv->conversation_->info.participants[0];
    auto contact = priv->accountContainer_->info.contactModel->getContact(contactUri);
    // If the contact is pending, we should accept its request
    if (contact.profileInfo.type == lrc::api::profile::Type::PENDING)
        priv->accountContainer_->info.conversationModel->makePermanent(contactUri);
    // Accept call
    priv->accountContainer_->info.callModel->accept(priv->conversation_->info.callId);
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
    priv->settings = g_settings_new_full(get_ring_schema(), NULL, NULL);
    g_settings_bind_with_mapping(priv->settings, "chat-pane-horizontal",
                                 priv->paned_call, "orientation",
                                 G_SETTINGS_BIND_GET,
                                 map_boolean_to_orientation,
                                 nullptr, nullptr, nullptr);

    g_signal_connect(priv->button_reject_incoming, "clicked", G_CALLBACK(reject_incoming_call), view);
    g_signal_connect(priv->button_accept_incoming, "clicked", G_CALLBACK(accept_incoming_call), view);
}

static void
incoming_call_view_class_init(IncomingCallViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = incoming_call_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/incomingcallview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, paned_call);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, image_incoming);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, label_name);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, label_bestId);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, spinner_status);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, label_status);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, button_accept_incoming);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, button_reject_incoming);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), IncomingCallView, frame_chat);
}

static void
update_state(IncomingCallView *view)
{
    IncomingCallViewPrivate *priv = INCOMING_CALL_VIEW_GET_PRIVATE(view);

    // change state label
    auto callId = priv->conversation_->info.callId;
    if (!priv->accountContainer_->info.callModel->hasCall(callId)) return;
    auto call = priv->accountContainer_->info.callModel->getCall(callId);

    gchar *status = g_strdup_printf("%s", lrc::api::call::to_string(call.status).c_str());
    gtk_label_set_text(GTK_LABEL(priv->label_status), status);
    g_free(status);

    if (call.status == lrc::api::call::Status::INCOMING_RINGING)
        gtk_widget_show(priv->button_accept_incoming);
    else
        gtk_widget_hide(priv->button_accept_incoming);
    gtk_widget_show(priv->button_reject_incoming);

    gtk_widget_show(priv->spinner_status);
}

static void
update_name_and_photo(IncomingCallView *view)
{
    auto priv = INCOMING_CALL_VIEW_GET_PRIVATE(view);

    QVariant var_i = GlobalInstances::pixmapManipulator().conversationPhoto(
        priv->conversation_->info,
        priv->accountContainer_->info,
        QSize(110, 110),
        false
    );
    std::shared_ptr<GdkPixbuf> image = var_i.value<std::shared_ptr<GdkPixbuf>>();
    gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_incoming), image.get());

    auto contactInfo = priv->accountContainer_->info.contactModel->getContact(
        priv->conversation_->info.participants.front()
    );

    auto name = contactInfo.profileInfo.alias;
    gtk_label_set_text(GTK_LABEL(priv->label_name), name.c_str());

    auto bestId = contactInfo.registeredName;
    if (name != bestId) {
        gtk_label_set_text(GTK_LABEL(priv->label_bestId), bestId.c_str());
        gtk_widget_show(priv->label_bestId);
    }
}

static void
set_call_info(IncomingCallView *view) {
    IncomingCallViewPrivate *priv = INCOMING_CALL_VIEW_GET_PRIVATE(view);

    update_state(view);
    update_name_and_photo(view);

    // Update view if call state changes
    priv->state_change_connection = QObject::connect(
    &*priv->accountContainer_->info.callModel,
    &lrc::api::NewCallModel::callStatusChanged,
    [view, priv] (const std::string& callId) {
        if (callId == priv->conversation_->info.callId) {
            update_state(view);
            update_name_and_photo(view);
        }
    });

    auto chat_view = chat_view_new(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
                                                         priv->accountContainer_,
                                                         priv->conversation_);
    gtk_widget_show(chat_view);
    chat_view_set_header_visible(CHAT_VIEW(chat_view), FALSE);
    gtk_container_add(GTK_CONTAINER(priv->frame_chat), chat_view);
}

GtkWidget *
incoming_call_view_new(WebKitChatContainer* view,
                       AccountContainer* accountContainer,
                       ConversationContainer* conversationContainer)
{
    auto self = g_object_new(INCOMING_CALL_VIEW_TYPE, NULL);

    IncomingCallViewPrivate *priv = INCOMING_CALL_VIEW_GET_PRIVATE(self);
    priv->webkit_chat_container = GTK_WIDGET(view);
    priv->conversation_ = conversationContainer;
    priv->accountContainer_ = accountContainer;

    set_call_info(INCOMING_CALL_VIEW(self));

    return GTK_WIDGET(self);
}

lrc::api::conversation::Info
incoming_call_view_get_conversation(IncomingCallView *self)
{
    g_return_val_if_fail(IS_INCOMING_CALL_VIEW(self), lrc::api::conversation::Info());
    auto priv = INCOMING_CALL_VIEW_GET_PRIVATE(self);

    return priv->conversation_->info;
}
