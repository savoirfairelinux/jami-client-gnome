/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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
#include "utils/accounts.h"

#include <gtk/gtk.h>
#include <call.h>
#include <callmodel.h>
#include <contactmethod.h>
#include <person.h>
#include <media/text.h>
#include <media/textrecording.h>
#include "ringnotify.h"
#include "profilemodel.h"
#include "profile.h"
#include "numbercategory.h"
#include <QtCore/QDateTime>
#include "utils/calling.h"
#include "webkitchatcontainer.h"

// LRC
#include <account.h>


static constexpr GdkRGBA RING_BLUE  = {0.0508, 0.594, 0.676, 1.0}; // outgoing msg color: (13, 152, 173)

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
    GtkWidget *button_chat_input;
    GtkWidget *entry_chat_input;
    GtkWidget *scrolledwindow_chat;
    GtkWidget *hbox_chat_info;
    GtkWidget *label_peer;
    GtkWidget *combobox_cm;
    GtkWidget *button_close_chatview;
    GtkWidget *button_placecall;
    GtkWidget *button_send_invitation;

    /* only one of the three following pointers should be non void;
     * either this is an in-call chat (and so the in-call chat APIs will be used)
     * or it is an out of call chat (and so the account chat APIs will be used) */
    Call          *call;
    Person        *person;
    ContactMethod *cm;

    QMetaObject::Connection new_message_connection;
    QMetaObject::Connection message_changed_connection;
    QMetaObject::Connection update_name;

    gulong webkit_ready;
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

    /* Destroying the box will also destroy its children, and we wouldn't
     * want that. So we remove the webkit_chat_container from the box. */
    if (priv->webkit_chat_container) {
        /* disconnect for webkit signals */
        g_signal_handler_disconnect(priv->webkit_chat_container, priv->webkit_ready);
        priv->webkit_ready = 0;

        gtk_container_remove(
            GTK_CONTAINER(priv->box_webkit_chat_container),
            GTK_WIDGET(priv->webkit_chat_container)
        );
        priv->webkit_chat_container = nullptr;

    }

    G_OBJECT_CLASS(chat_view_parent_class)->dispose(object);
}


static void
send_chat(G_GNUC_UNUSED GtkWidget *widget, ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    /* make sure there is more than just whitespace, but if so, send the original text */
    const auto text = QString(gtk_entry_get_text(GTK_ENTRY(priv->entry_chat_input)));
    if (!text.trimmed().isEmpty()) {
        QMap<QString, QString> messages;
        messages["text/plain"] = text;

        if (priv->call) {
            // in call message
            priv->call->addOutgoingMedia<Media::Text>()->send(messages);
        } else if (priv->person) {
            // get the chosen cm
            auto active = gtk_combo_box_get_active(GTK_COMBO_BOX(priv->combobox_cm));
            if (active >= 0) {
                auto cm = priv->person->phoneNumbers().at(active);
                if (!cm->sendOfflineTextMessage(messages))
                    g_warning("message failed to send"); // TODO: warn the user about this in the UI
            } else {
                g_warning("no ContactMethod chosen; message not sent");
            }
        } else if (priv->cm) {
            if (!priv->cm->sendOfflineTextMessage(messages))
                g_warning("message failed to send"); // TODO: warn the user about this in the UI
        } else {
            g_warning("no Call, Person, or ContactMethod set; message not sent");
        }

        /* clear the entry */
        gtk_entry_set_text(GTK_ENTRY(priv->entry_chat_input), "");
    }
}

static void
hide_chat_view(G_GNUC_UNUSED GtkWidget *widget, ChatView *self)
{
    g_signal_emit(G_OBJECT(self), chat_view_signals[HIDE_VIEW_CLICKED], 0);
}

static void
placecall_clicked(ChatView *self)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    if (priv->person) {
        // get the chosen cm
        auto active = gtk_combo_box_get_active(GTK_COMBO_BOX(priv->combobox_cm));
        if (active >= 0) {
            auto cm = priv->person->phoneNumbers().at(active);
            place_new_call(cm);
        } else {
            g_warning("no ContactMethod chosen; cannot place call");
        }
    } else if (priv->cm) {
        place_new_call(priv->cm);
    } else {
        g_warning("no Person or ContactMethod set; cannot place call");
    }
}

static void
button_send_invitation_clicked(ChatView *self)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    // get the account associated to the selected cm
    auto active = gtk_combo_box_get_active(GTK_COMBO_BOX(priv->combobox_cm));

    if (priv->person)
        priv->cm = priv->person->phoneNumbers().at(active);

    if (!priv->cm) {
        g_warning("invalid contact, cannot send invitation!");
        return;
    }

    // try first to use the account associated to the contact method
    auto account = priv->cm->account();
    if (not account) {
        // if there is no account associated try then to get the choosen account
        account = get_active_ring_account();

        if (not account) {
            g_warning("invalid account, cannot send invitation!");
            return;
        }
    }

    // perform the request
    if (!account->sendContactRequest(priv->cm))
        g_warning("contact request not forwarded, cannot send invitation!");

    // TODO : add an entry in the conversation to tell the user an invitation was sent.
}

static void
chat_view_init(ChatView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(view);

    g_signal_connect(priv->button_chat_input, "clicked", G_CALLBACK(send_chat), view);
    g_signal_connect(priv->entry_chat_input, "activate", G_CALLBACK(send_chat), view);
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, button_chat_input);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, entry_chat_input);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, scrolledwindow_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, hbox_chat_info);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, label_peer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, combobox_cm);
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
print_message_to_buffer(ChatView* self, const QModelIndex &idx)
{
    if (!idx.isValid()) {
        g_warning("QModelIndex in im model is not valid");
        return;
    }

    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    webkit_chat_container_print_new_message(
        WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
        idx
    );
}

ContactMethod*
get_active_contactmethod(ChatView *self)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    auto cms = priv->person->phoneNumbers();
    auto active = gtk_combo_box_get_active(GTK_COMBO_BOX(priv->combobox_cm));
    if (active >= 0 && active < cms.size()) {
        return cms.at(active);
    } else {
        return nullptr;
    }
}

static void
set_participant_images(ChatView* self)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    webkit_chat_container_clear_sender_images(
        WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container)
    );

    /* Set the sender image for the peer */
    ContactMethod* sender_contact_method_peer;
    QVariant photo_variant_peer;

    if (priv->person)
    {
        photo_variant_peer = priv->person->photo();
        sender_contact_method_peer = get_active_contactmethod(self);
    }
    else
    {
        if (priv->cm)
        {
            sender_contact_method_peer = priv->cm;
        }
        else
        {
            sender_contact_method_peer = priv->call->peerContactMethod();
        }
        photo_variant_peer = sender_contact_method_peer->roleData((int) Call::Role::Photo);
    }

    if (photo_variant_peer.isValid())
    {
        webkit_chat_container_set_sender_image(
            WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
            sender_contact_method_peer,
            photo_variant_peer
        );
    }

    /* set sender image for "ME" */
    auto profile = ProfileModel::instance().selectedProfile();
    if (profile)
    {
        auto person = profile->person();
        if (person)
        {
            auto photo_variant_me = person->photo();
            if (photo_variant_me.isValid())
            {
                webkit_chat_container_set_sender_image(
                    WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
                    nullptr,
                    photo_variant_me
                );
            }
        }
    }
}

static void
print_text_recording(Media::TextRecording *recording, ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

     /* set the photos of the chat participants */
     set_participant_images(self);

    /* only text messages are supported for now */
    auto model = recording->instantTextMessagingModel();

    /* new model, disconnect from the old model updates and clear the text buffer */
    QObject::disconnect(priv->new_message_connection);

    /* put all the messages in the im model into the text view */
    for (int row = 0; row < model->rowCount(); ++row) {
        QModelIndex idx = model->index(row, 0);
        print_message_to_buffer(self, idx);
    }
    /* mark all messages as read */
    recording->setAllRead();


    /* messages modified */
    /* connecting on instantMessagingModel and not textMessagingModel */
    priv->message_changed_connection = QObject::connect(
        model,
        &QAbstractItemModel::dataChanged,
        [self, priv] (const QModelIndex & topLeft, G_GNUC_UNUSED const QModelIndex & bottomRight)
        {
            webkit_chat_container_update_message(
                WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container),
                topLeft
            );
        }
    );

    /* append new messages */
    priv->new_message_connection = QObject::connect(
        model,
        &QAbstractItemModel::rowsInserted,
        [self, priv, model] (const QModelIndex &parent, int first, int last) {
            for (int row = first; row <= last; ++row) {
                QModelIndex idx = model->index(row, 0, parent);
                print_message_to_buffer(self, idx);
                /* make sure these messages are marked as read */
                model->setData(idx, true, static_cast<int>(Media::TextRecording::Role::IsRead));
                g_signal_emit(G_OBJECT(self), chat_view_signals[NEW_MESSAGES_DISPLAYED], 0);
            }
        }
    );
}

static void
selected_cm_changed(ChatView *self)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    /* make sure the webkit view is ready, in case we get this signal before it is */
    if (!webkit_chat_container_is_ready(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container)))
        return;

    auto cm = get_active_contactmethod(self);
    if (cm){
        print_text_recording(cm->textRecording(), self);
    } else {
        g_warning("no valid ContactMethod selected to display chat conversation");
    }
}

static void
render_contact_method(G_GNUC_UNUSED GtkCellLayout *cell_layout,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    GValue value = G_VALUE_INIT;
    gtk_tree_model_get_value(model, iter, 0, &value);
    auto cm = (ContactMethod *)g_value_get_pointer(&value);

    gchar *number = nullptr;
    if (cm && cm->category()) {
        // try to get the number category, eg: "home"
        number = g_strdup_printf("(%s) %s", cm->category()->name().toUtf8().constData(),
                                            cm->getBestId().toUtf8().constData());
    } else if (cm) {
        number = g_strdup_printf("%s", cm->getBestId().toUtf8().constData());
    }

    g_object_set(G_OBJECT(cell), "text", number, NULL);
    g_free(number);
}

static void
update_contact_methods(ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    g_return_if_fail(priv->call || priv->person || priv->cm);

    /* model for the combobox for the choice of ContactMethods */
    auto cm_model = gtk_list_store_new(
        1, G_TYPE_POINTER
    );

    Person::ContactMethods cms;

    if (priv->person)
        cms = priv->person->phoneNumbers();
    else if (priv->cm)
        cms << priv->cm;
    else if (priv->call)
        cms << priv->call->peerContactMethod();

    for (int i = 0; i < cms.size(); ++i) {
        GtkTreeIter iter;
        gtk_list_store_append(cm_model, &iter);
        gtk_list_store_set(cm_model, &iter,
                           0, cms.at(i),
                           -1);
    }

    gtk_combo_box_set_model(GTK_COMBO_BOX(priv->combobox_cm), GTK_TREE_MODEL(cm_model));
    g_object_unref(cm_model);

    auto renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->combobox_cm), renderer, FALSE);
    gtk_cell_layout_set_cell_data_func(
        GTK_CELL_LAYOUT(priv->combobox_cm),
        renderer,
        (GtkCellLayoutDataFunc)render_contact_method,
        nullptr, nullptr);

    /* select the last used cm */
    if (!cms.isEmpty()) {
        auto last_used_cm = cms.at(0);
        int last_used_cm_idx = 0;
        for (int i = 1; i < cms.size(); ++i) {
            auto new_cm = cms.at(i);
            if (difftime(new_cm->lastUsed(), last_used_cm->lastUsed()) > 0) {
                last_used_cm = new_cm;
                last_used_cm_idx = i;
            }
        }

        gtk_combo_box_set_active(GTK_COMBO_BOX(priv->combobox_cm), last_used_cm_idx);
    }

    /* if there is only one CM, make the combo box insensitive */
    if (cms.size() < 2)
        gtk_widget_set_sensitive(priv->combobox_cm, FALSE);

    /* if no CMs make the call button insensitive */
    gtk_widget_set_sensitive(priv->button_placecall, !cms.isEmpty());
}

static void
update_name(ChatView *self)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    g_return_if_fail(priv->person || priv->cm || priv->call);

    QString name;
    if (priv->person) {
        name = priv->person->roleData(static_cast<int>(Ring::Role::Name)).toString();
    } else if (priv->cm) {
        name = priv->cm->roleData(static_cast<int>(Ring::Role::Name)).toString();
    } else if (priv->call) {
        name = priv->call->peerContactMethod()->roleData(static_cast<int>(Ring::Role::Name)).toString();
    }
    gtk_label_set_text(GTK_LABEL(priv->label_peer), name.toUtf8().constData());
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

    /* print the text recordings */
    if (priv->call) {
        print_text_recording(priv->call->peerContactMethod()->textRecording(), self);
    } else if (priv->cm) {
        print_text_recording(priv->cm->textRecording(), self);
    } else if (priv->person) {
        /* get the selected cm and print the text recording */
        selected_cm_changed(self);
    }
}

static void
build_chat_view(ChatView* self)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    gtk_container_add(GTK_CONTAINER(priv->box_webkit_chat_container), priv->webkit_chat_container);
    gtk_widget_show(priv->webkit_chat_container);

    /* keep name updated */
    if (priv->call) {
        priv->update_name = QObject::connect(
            priv->call,
            &Call::changed,
            [self] () { update_name(self); }
        );
    } else if (priv->cm) {
        priv->update_name = QObject::connect(
            priv->cm,
            &ContactMethod::changed,
            [self] () { update_name(self); }
        );
    } else if (priv->person) {
        priv->update_name = QObject::connect(
            priv->person,
            &Person::changed,
            [self] () { update_name(self); }
        );
    }
    update_name(self);

    /* keep selected cm updated */
    update_contact_methods(self);
    g_signal_connect_swapped(priv->combobox_cm, "changed", G_CALLBACK(selected_cm_changed), self);

    priv->webkit_ready = g_signal_connect_swapped(
        priv->webkit_chat_container,
        "ready",
        G_CALLBACK(webkit_chat_container_ready),
        self
    );

    if (webkit_chat_container_is_ready(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container)))
        webkit_chat_container_ready(self);

    /* we only show the chat info in the case of cm / person */
    gtk_widget_set_visible(priv->hbox_chat_info, (priv->cm || priv->person));
}

GtkWidget *
chat_view_new_call(WebKitChatContainer *webkit_chat_container, Call *call)
{
    g_return_val_if_fail(call, nullptr);

    ChatView *self = CHAT_VIEW(g_object_new(CHAT_VIEW_TYPE, NULL));

    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);
    priv->webkit_chat_container = GTK_WIDGET(webkit_chat_container);
    priv->call = call;

    build_chat_view(self);

    return (GtkWidget *)self;
}

GtkWidget *
chat_view_new_cm(WebKitChatContainer *webkit_chat_container, ContactMethod *cm)
{
    g_return_val_if_fail(cm, nullptr);

    ChatView *self = CHAT_VIEW(g_object_new(CHAT_VIEW_TYPE, NULL));

    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);
    priv->webkit_chat_container = GTK_WIDGET(webkit_chat_container);
    priv->cm = cm;

    build_chat_view(self);

    return (GtkWidget *)self;
}

GtkWidget *
chat_view_new_person(WebKitChatContainer *webkit_chat_container, Person *p)
{
    g_return_val_if_fail(p, nullptr);

    ChatView *self = CHAT_VIEW(g_object_new(CHAT_VIEW_TYPE, NULL));

    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);
    priv->webkit_chat_container = GTK_WIDGET(webkit_chat_container);
    priv->person = p;

    build_chat_view(self);

    return (GtkWidget *)self;
}

Call*
chat_view_get_call(ChatView *self)
{
    g_return_val_if_fail(IS_CHAT_VIEW(self), nullptr);
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    return priv->call;
}

ContactMethod*
chat_view_get_cm(ChatView *self)
{
    g_return_val_if_fail(IS_CHAT_VIEW(self), nullptr);
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    return priv->cm;
}

Person*
chat_view_get_person(ChatView *self)
{
    g_return_val_if_fail(IS_CHAT_VIEW(self), nullptr);
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    return priv->person;
}

void
chat_view_set_header_visible(ChatView *self, gboolean visible)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    gtk_widget_set_visible(priv->hbox_chat_info, visible);
}
